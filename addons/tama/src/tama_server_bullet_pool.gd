## Manages server-based bullet pools, one per registered bullet type.
##
## Each type gets one [PhysicsServer2D] area (N shapes for N pool slots) for collision,
## and a dynamic set of [MultiMesh] batch nodes for rendering. One batch node is created
## per physics frame that fires bullets of a given type, so bullets spawned later always
## render above bullets spawned earlier (scene-draw-order z-layering via call order in
## [method _draw]).
##
## When the total active bullet count exceeds the project setting
## [code]tama/server_bullet_composite_threshold[/code] (default 1000), all active batches
## for a type are merged into a single [MultiMesh] draw call per frame via a composite
## buffer built from each batch's transform data.
##
## Register types by calling [method TamaManager.register_server_bullet] before any
## bullets are fired. All active bullets across all types are updated in a single
## [method _physics_process] loop.
##
## Limitations:
##   - [code]bullet_emitter_act[/code] (a bullet that fires sub-bullets) is not
##     supported. Bullets using that construct must use node-based spawning instead.
extends Node2D
class_name TamaServerBulletPool

const _Ast         = preload("res://addons/tama/src/tama_ast.gd")
const _Interpreter = preload("res://addons/tama/src/tama_interpreter.gd")

## Fixed slot capacity of each batch [MultiMesh]. If more bullets than this are fired in
## a single physics frame, additional batches are created for the overflow (same
## [code]birth_frame[/code], still drawn consecutively).
const _BATCH_CHUNK := 256

## Emitted when a physics body overlaps a bullet's area.
## [param bullet] is the server bullet that was hit.
## [param body_instance_id] is [method Object.get_instance_id] of the overlapping body.
signal bullet_hit(bullet: TamaServerBullet, body_instance_id: int)

## Bullets this far outside the visible world rect are recycled automatically.
@export var bounds_margin: float = 64.0

## All active bullets across all registered types, driven each physics frame.
var _active: Array[TamaServerBullet] = []

## Per-type resources and state. Populated by register_type().
var _types: Dictionary = {}  # String → _TypeData

## Rendering data for one frame-batch of bullets of a single type.
## All bullets in a batch share one [MultiMesh] and were spawned in the same physics frame.
class _BatchData:
	var multimesh_res: MultiMesh
	var multimesh:     RID
	var used:          int = 0   # local slots assigned so far this frame
	var active_count:  int = 0   # bullets still alive in this batch
	var birth_frame:   int = -1

## Holds all GPU and physics resources for one registered bullet type.
class _TypeData:
	var quad:        QuadMesh       # shared by all batch multimeshes for this type
	var area:        RID
	var shapes:      Array[RID] = []
	var all_bullets: Array      = []  # Array[TamaServerBullet], indexed by global physics slot
	var config:      TamaServerBulletConfig
	# Circular FIFO queue for global physics slot allocation.
	var _ring:       PackedInt32Array
	var _ring_r:     int = 0
	var _ring_w:     int = 0
	var _ring_count: int = 0
	# Render batch state.
	var active_batches: Array = []  # Array[_BatchData], oldest first (draw order = z order)
	var free_batches:   Array = []  # Array[_BatchData], available for reuse
	var _cur_batch:     _BatchData = null  # batch currently being filled this frame
	var _cur_frame:     int = -1           # Engine.get_physics_frames() of _cur_batch
	# Composite multimesh — rebuilt each draw when above threshold; one draw call per type.
	var composite_res: MultiMesh
	var composite:     RID

# ---------------------------------------------------------------------------
# Type registration
# ---------------------------------------------------------------------------

## Creates GPU and physics resources for [param key]. Called automatically by
## [method TamaManager.register_server_bullet]. Must be called while the pool
## is in the scene tree so that [code]get_world_2d()[/code] is available.
func register_type(key: String, config: TamaServerBulletConfig) -> void:
	if _types.has(key):
		push_warning("TamaServerBulletPool: type '%s' already registered, skipping." % key)
		return

	var td    := _TypeData.new()
	td.config  = config
	_types[key] = td

	var n := config.pool_size

	# QuadMesh shared by all batch multimeshes and the composite for this type.
	td.quad      = QuadMesh.new()
	td.quad.size = config.rect.size

	# Composite multimesh — instance_count is set dynamically at draw time.
	td.composite_res                  = MultiMesh.new()
	td.composite_res.transform_format = MultiMesh.TRANSFORM_2D
	td.composite_res.mesh             = td.quad
	td.composite                      = td.composite_res.get_rid()

	# Area with N shapes — shape index == global physics slot.
	var space := get_world_2d().space
	td.area = PhysicsServer2D.area_create()
	PhysicsServer2D.area_set_space(td.area, space)
	PhysicsServer2D.area_set_collision_layer(td.area, config.collision_layer)
	PhysicsServer2D.area_set_collision_mask(td.area, config.collision_mask)
	PhysicsServer2D.area_set_monitorable(td.area, false)
	for i in n:
		var shape := PhysicsServer2D.circle_shape_create()
		PhysicsServer2D.shape_set_data(shape, config.shape_radius)
		PhysicsServer2D.area_add_shape(td.area, shape)
		PhysicsServer2D.area_set_shape_disabled(td.area, i, true)
		td.shapes.append(shape)

	# Monitor callback — local_shape is the global slot index.
	var td_ref := td
	PhysicsServer2D.area_set_monitor_callback(td.area,
		func(status: int, _body_rid: RID, body_iid: int, _body_shape: int, local_shape: int) -> void:
			if status == PhysicsServer2D.AREA_BODY_ADDED:
				var b: TamaServerBullet = td_ref.all_bullets[local_shape]
				if b != null and b.active:
					bullet_hit.emit(b, body_iid)
	)

	# Pre-allocate bullet state objects, one per global slot.
	td.all_bullets.resize(n)
	for i in n:
		var b        := TamaServerBullet.new()
		b._mm_index   = i
		b._area       = td.area
		b._type_data  = td
		td.all_bullets[i] = b

	# Fill the ring with slots in ascending order.
	td._ring       = PackedInt32Array(range(n))
	td._ring_r     = 0
	td._ring_w     = 0
	td._ring_count = n

# ---------------------------------------------------------------------------
# Batch management helpers
# ---------------------------------------------------------------------------

func _get_batch(td: _TypeData) -> _BatchData:
	if not td.free_batches.is_empty():
		return td.free_batches.pop_back()
	var b        := _BatchData.new()
	b.multimesh_res                  = MultiMesh.new()
	b.multimesh_res.transform_format = MultiMesh.TRANSFORM_2D
	b.multimesh_res.mesh             = td.quad
	b.multimesh_res.instance_count   = _BATCH_CHUNK
	b.multimesh                      = b.multimesh_res.get_rid()
	var offscreen := Transform2D(0.0, Vector2(-100000.0, -100000.0))
	for i in _BATCH_CHUNK:
		RenderingServer.multimesh_instance_set_transform_2d(b.multimesh, i, offscreen)
	return b

func _recycle_batch(td: _TypeData, batch: _BatchData) -> void:
	if td._cur_batch == batch:
		td._cur_batch = null
	var offscreen := Transform2D(0.0, Vector2(-100000.0, -100000.0))
	for i in batch.used:
		RenderingServer.multimesh_instance_set_transform_2d(batch.multimesh, i, offscreen)
	batch.used         = 0
	batch.active_count = 0
	batch.birth_frame  = -1
	td.active_batches.erase(batch)
	td.free_batches.append(batch)

# ---------------------------------------------------------------------------
# Spawn
# ---------------------------------------------------------------------------

## Spawns a server bullet. Called by [TamaSpawnManager] when a server-configured
## bullet type is fired. Returns the bullet, or null if the pool for this type is full.
func spawn(
		data:     _Interpreter.BulletFireData,
		config:   TamaServerBulletConfig,
		angle:    float,
		speed:    float,
		position: Vector2,
		context:  TamaContext
) -> TamaServerBullet:
	var td = _types.get(data.bullet_type)
	if td == null:
		push_warning("TamaServerBulletPool: type '%s' not registered." % data.bullet_type)
		return null
	if td._ring_count == 0:
		push_warning("TamaServerBulletPool: pool full for type '%s'" % data.bullet_type)
		return null

	# Physics: allocate a global slot from the FIFO ring.
	var n           = td.all_bullets.size()
	var global_slot = td._ring[td._ring_r]
	td._ring_r      = (td._ring_r + 1) % n
	td._ring_count -= 1

	# Rendering: get or start this frame's batch.
	var frame := Engine.get_physics_frames()
	if td._cur_batch == null or td._cur_frame != frame or td._cur_batch.used >= _BATCH_CHUNK:
		td._cur_batch             = _get_batch(td)
		td._cur_batch.birth_frame = frame
		td._cur_frame             = frame
		td.active_batches.append(td._cur_batch)
	var batch      = td._cur_batch
	var local_slot = batch.used
	batch.used         += 1
	batch.active_count += 1

	var b: TamaServerBullet = td.all_bullets[global_slot]
	b.active        = true
	b._active_index = _active.size()
	_active.append(b)

	b._multimesh   = batch.multimesh
	b._local_slot  = local_slot
	b._batch       = batch

	b.position         = position
	b.angle            = angle
	b.speed            = speed
	b.speed_x          = 0.0
	b.speed_y          = 0.0
	b.rotates          = config.rotates
	# Negate Y to compensate for QuadMesh UV inversion in 2D canvas (+Y up in 3D, down in 2D).
	b._texture_scale   = Vector2(config.texture_scale.x, -config.texture_scale.y)
	b.initial_position = position
	b._last_angle      = angle
	b._last_speed      = speed
	b._angle_tweening  = false
	b._speed_tweening  = false
	b._pos_tweening    = false
	b._sx_tweening     = false
	b._sy_tweening     = false

	b.mvmt_x_set  = data.mvmt_x_set
	b.mvmt_x_type = data.mvmt_x_type
	b.mvmt_x_expr = data.mvmt_x_expr
	b.mvmt_y_set  = data.mvmt_y_set
	b.mvmt_y_type = data.mvmt_y_type
	b.mvmt_y_expr = data.mvmt_y_expr

	var spawn_rot := b.angle if b.rotates else 0.0
	RenderingServer.multimesh_instance_set_transform_2d(
		batch.multimesh, local_slot,
		Transform2D(spawn_rot, b._texture_scale, 0.0, position)
	)

	PhysicsServer2D.area_set_shape_transform(td.area, global_slot, Transform2D(0.0, position))
	PhysicsServer2D.area_set_shape_disabled(td.area, global_slot, false)

	if data.bullet_emitter_act != null:
		push_warning("TamaServerBulletPool: bullet_emitter_act is not supported for server bullets. The sub-emitter act will be skipped.")

	var needs_runner := data.bullet_act != null or data.mvmt_x_set or data.mvmt_y_set
	if needs_runner:
		var runner = _Interpreter.new()
		runner.context           = context
		runner._tree             = get_tree()
		runner._frame_loop_owner = runner
		b._runner                = runner

		var act_scope: Dictionary = {}
		for i in mini(data.bullet_params.size(), data.bullet_args.size()):
			act_scope[data.bullet_params[i]] = data.bullet_args[i]
		act_scope["spawn_x"] = position.x
		act_scope["spawn_y"] = position.y

		if data.mvmt_x_set or data.mvmt_y_set:
			b.mvmt_scope = act_scope.duplicate()

		if data.bullet_act:
			_connect_runner(runner, b)
			runner.call_deferred(&"start_act", data.source_program, data.bullet_act, act_scope)
	else:
		b._runner = null

	return b

func _connect_runner(runner, b: TamaServerBullet) -> void:
	runner.frame_loop_registered.connect(TamaManager._register_frame_loop)
	runner.changed_direction.connect(func(d): _on_changed_direction(b, d))
	runner.changed_speed.connect(func(d): _on_changed_speed(b, d))
	runner.changed_position.connect(func(d): _on_changed_position(b, d))
	runner.accelerated.connect(func(d): _on_accelerated(b, d))
	runner.vanished.connect(func(): recycle(b))

# ---------------------------------------------------------------------------
# Recycle
# ---------------------------------------------------------------------------

## Recycles all currently active server bullets.
func recycle_all() -> void:
	for b in _active.duplicate():
		recycle(b)

## Returns [param b] to the pool. Safe to call from signal callbacks.
func recycle(b: TamaServerBullet) -> void:
	if not b.active:
		return
	b.active = false

	# Hide in the batch's multimesh.
	RenderingServer.multimesh_instance_set_transform_2d(
		b._multimesh, b._local_slot,
		Transform2D(0.0, Vector2(-100000.0, -100000.0))
	)

	# Decrement batch counter; recycle the batch node when it empties.
	var batch = b._batch
	batch.active_count -= 1
	if batch.active_count == 0:
		_recycle_batch(b._type_data, batch)

	# Return global physics slot to the FIFO ring.
	var td_r   = b._type_data
	var ring_n = td_r.all_bullets.size()
	td_r._ring[td_r._ring_w] = b._mm_index
	td_r._ring_w     = (td_r._ring_w + 1) % ring_n
	td_r._ring_count += 1

	PhysicsServer2D.area_set_shape_disabled(b._area, b._mm_index, true)

	if b._runner:
		TamaManager._unregister_frame_loop(b._runner)
		b._runner.stop()
		b._runner.call_deferred(&"free")
		b._runner = null

	# O(1) unordered removal from _active.
	var idx  := b._active_index
	var last := _active.back()
	_active[idx]       = last
	last._active_index = idx
	_active.pop_back()
	b._active_index = -1

# ---------------------------------------------------------------------------
# Per-frame update
# ---------------------------------------------------------------------------

func _draw() -> void:
	var threshold := ProjectSettings.get_setting(
		"tama/server_bullet_composite_threshold", 1000) as int

	for key in _types:
		var td: _TypeData = _types[key]
		if td.active_batches.is_empty():
			continue
		if _active.size() <= threshold:
			# Below threshold: one draw call per batch, in birth order.
			# Later draw calls render on top — newest batch = highest z. ✓
			for batch in td.active_batches:
				draw_multimesh(batch.multimesh_res, td.config.texture)
		else:
			# Above threshold: collapse all batches into one draw call.
			_draw_composite(td)

func _draw_composite(td: _TypeData) -> void:
	# Concatenate each batch's transform buffer in birth order.
	# Older batches occupy lower composite slots (drawn behind newer ones). ✓
	var composite_buf := PackedFloat32Array()
	for batch in td.active_batches:
		if batch.used > 0:
			composite_buf.append_array(
				RenderingServer.multimesh_get_buffer(batch.multimesh).slice(0, batch.used * 8)
			)
	if composite_buf.is_empty():
		return
	td.composite_res.instance_count = composite_buf.size() / 8
	RenderingServer.multimesh_set_buffer(td.composite, composite_buf)
	draw_multimesh(td.composite_res, td.config.texture)

func _physics_process(delta: float) -> void:
	var bounds     := _world_bounds().grow(bounds_margin)
	var to_recycle: Array[TamaServerBullet] = []

	for b in _active:
		_step_tweens(b, delta)

		if b.mvmt_x_set or b.mvmt_y_set:
			if b._runner:
				if b.mvmt_x_set:
					var vx: float = b._runner._eval(b.mvmt_x_expr, b.mvmt_scope)
					b.position.x = vx if b.mvmt_x_type == _Ast.ValueType.ABS \
						else b.initial_position.x + vx
				if b.mvmt_y_set:
					var vy: float = b._runner._eval(b.mvmt_y_expr, b.mvmt_scope)
					b.position.y = vy if b.mvmt_y_type == _Ast.ValueType.ABS \
						else b.initial_position.y + vy
		else:
			var vel := Vector2(cos(b.angle), sin(b.angle)) * b.speed
			vel.x += b.speed_x
			vel.y += b.speed_y
			b.position += vel * delta

		var rot := b.angle if b.rotates else 0.0
		RenderingServer.multimesh_instance_set_transform_2d(
			b._multimesh, b._local_slot,
			Transform2D(rot, b._texture_scale, 0.0, b.position)
		)
		PhysicsServer2D.area_set_shape_transform(b._area, b._mm_index, Transform2D(0.0, b.position))

		if not bounds.has_point(b.position):
			to_recycle.append(b)

	for b in to_recycle:
		recycle(b)

	if not _active.is_empty():
		queue_redraw()

# ---------------------------------------------------------------------------
# Tween stepping
# ---------------------------------------------------------------------------

func _step_tweens(b: TamaServerBullet, delta: float) -> void:
	if b._angle_tweening:
		b._angle_elapsed += delta
		var t := minf(b._angle_elapsed / b._angle_duration, 1.0)
		b.angle = lerpf(b._angle_from, b._angle_to, t)
		if t >= 1.0:
			b._angle_tweening = false

	if b._speed_tweening:
		b._speed_elapsed += delta
		var t := minf(b._speed_elapsed / b._speed_duration, 1.0)
		b.speed = lerpf(b._speed_from, b._speed_to, t)
		if t >= 1.0:
			b._speed_tweening = false

	if b._pos_tweening:
		b._pos_elapsed += delta
		var t := minf(b._pos_elapsed / b._pos_duration, 1.0)
		b.position = b._pos_from.lerp(b._pos_to, t)
		if t >= 1.0:
			b._pos_tweening = false

	if b._sx_tweening:
		b._sx_elapsed += delta
		var t := minf(b._sx_elapsed / b._sx_duration, 1.0)
		b.speed_x = lerpf(b._sx_from, b._sx_to, t)
		if t >= 1.0:
			b._sx_tweening = false

	if b._sy_tweening:
		b._sy_elapsed += delta
		var t := minf(b._sy_elapsed / b._sy_duration, 1.0)
		b.speed_y = lerpf(b._sy_from, b._sy_to, t)
		if t >= 1.0:
			b._sy_tweening = false

# ---------------------------------------------------------------------------
# Signal handlers (mirror TamaBullet signal handlers)
# ---------------------------------------------------------------------------

func _on_changed_direction(b: TamaServerBullet, data: _Interpreter.ChdirData) -> void:
	var target := _dir_to_angle(b, data.dir_type, data.dir_value)
	b._last_angle = b.angle
	if data.over <= 0.0:
		b.angle = target
		b._angle_tweening = false
	else:
		b._angle_tweening = true
		b._angle_from     = b.angle
		b._angle_to       = target
		b._angle_elapsed  = 0.0
		b._angle_duration = data.over

func _on_changed_speed(b: TamaServerBullet, data: _Interpreter.ChspdData) -> void:
	var target := _spd_to_value(b, data.speed_type, data.speed_value)
	b._last_speed = b.speed
	if data.over <= 0.0:
		b.speed = target
		b._speed_tweening = false
	else:
		b._speed_tweening = true
		b._speed_from     = b.speed
		b._speed_to       = target
		b._speed_elapsed  = 0.0
		b._speed_duration = data.over

func _on_changed_position(b: TamaServerBullet, data: _Interpreter.ChposData) -> void:
	var target := b.position
	if data.has_x:
		match data.x_type:
			_Ast.ValueType.ABS, _Ast.ValueType.SEQ:
				target.x = data.x
			_Ast.ValueType.REL:
				target.x += data.x
	if data.has_y:
		match data.y_type:
			_Ast.ValueType.ABS, _Ast.ValueType.SEQ:
				target.y = data.y
			_Ast.ValueType.REL:
				target.y += data.y
	if data.over <= 0.0:
		b.position = target
		b._pos_tweening = false
	else:
		b._pos_tweening = true
		b._pos_from     = b.position
		b._pos_to       = target
		b._pos_elapsed  = 0.0
		b._pos_duration = data.over

func _on_accelerated(b: TamaServerBullet, data: _Interpreter.AccelData) -> void:
	if data.over <= 0.0:
		if data.has_x:
			match data.x_type:
				_Ast.ValueType.REL: b.speed_x += data.x
				_:                  b.speed_x  = data.x
		if data.has_y:
			match data.y_type:
				_Ast.ValueType.REL: b.speed_y += data.y
				_:                  b.speed_y  = data.y
	else:
		if data.has_x:
			var end_x := _accel_axis_end(data.x_type, data.x, b.speed_x, data.over)
			b._sx_tweening = true
			b._sx_from     = b.speed_x
			b._sx_to       = end_x
			b._sx_elapsed  = 0.0
			b._sx_duration = data.over
		if data.has_y:
			var end_y := _accel_axis_end(data.y_type, data.y, b.speed_y, data.over)
			b._sy_tweening = true
			b._sy_from     = b.speed_y
			b._sy_to       = end_y
			b._sy_elapsed  = 0.0
			b._sy_duration = data.over

# ---------------------------------------------------------------------------
# Conversion helpers (match TamaBullet's private helpers exactly)
# ---------------------------------------------------------------------------

func _dir_to_angle(b: TamaServerBullet, dir_type: _Ast.DirType, value: float) -> float:
	match dir_type:
		_Ast.DirType.AIM:
			return (TamaManager.player_position - b.position).angle() + deg_to_rad(value)
		_Ast.DirType.ABS:
			return deg_to_rad(value)
		_Ast.DirType.REL:
			return b.angle + deg_to_rad(value)
		_Ast.DirType.SEQ:
			return b._last_angle + deg_to_rad(value)
	return deg_to_rad(value)

func _spd_to_value(b: TamaServerBullet, speed_type: _Ast.ValueType, value: float) -> float:
	match speed_type:
		_Ast.ValueType.ABS:
			return value
		_Ast.ValueType.REL:
			return b.speed + value
		_Ast.ValueType.SEQ:
			return b._last_speed + value
	return value

func _accel_axis_end(axis_type: _Ast.ValueType, value: float, current: float, over: float) -> float:
	match axis_type:
		_Ast.ValueType.ABS, _Ast.ValueType.SEQ:
			return value
		_Ast.ValueType.REL:
			return (value - current) * over
	return value

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

func _world_bounds() -> Rect2:
	var inv     := get_viewport().get_canvas_transform().affine_inverse()
	var vp_rect := get_viewport().get_visible_rect()
	var tl      := inv * vp_rect.position
	var br      := inv * vp_rect.end
	return Rect2(
		minf(tl.x, br.x), minf(tl.y, br.y),
		absf(br.x - tl.x), absf(br.y - tl.y)
	)

func _exit_tree() -> void:
	for td in _types.values():
		for shape in td.shapes:
			PhysicsServer2D.free_rid(shape)
		PhysicsServer2D.free_rid(td.area)
		td.active_batches.clear()
		td.free_batches.clear()
