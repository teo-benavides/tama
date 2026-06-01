## Manages a fixed pool of server-based bullets using [RenderingServer] for
## rendering and [PhysicsServer2D] areas for overlap detection — no per-bullet
## scene-tree nodes, no move_and_slide(), no Tween objects.
##
## All active bullets are driven in a single [method _physics_process] loop.
## Collision is handled via area monitor callbacks rather than polling.
##
## Limitations:
##   - [code]bullet_emitter_act[/code] (a bullet that also fires sub-bullets) is not
##     supported. Bullets with that construct must use node-based spawning instead.
extends Node2D
class_name TamaServerBulletPool

const _Ast         = preload("res://addons/tama/src/tama_ast.gd")
const _Interpreter = preload("res://addons/tama/src/tama_interpreter.gd")

## Emitted when a physics body overlaps a bullet's area.
## Connect to this signal to implement hit detection (e.g. damage the player).
## [param bullet] is the server bullet that was hit.
## [param body_instance_id] is the [method Object.get_instance_id] of the overlapping body.
signal bullet_hit(bullet: TamaServerBullet, body_instance_id: int)

## Maximum simultaneous server bullets. Pre-allocated at startup.
@export var pool_size: int = 10000

## Bullets this far outside the visible world rect are recycled automatically.
@export var bounds_margin: float = 64.0

var _all:       Array[TamaServerBullet] = []
var _active:    Array[TamaServerBullet] = []
var _free_list: Array[TamaServerBullet] = []

var _z_counter: int = 0

func _ready() -> void:
	_preallocate()

func _preallocate() -> void:
	var parent_ci := get_canvas_item()
	var space     := get_world_2d().space
	for i in pool_size:
		var b := TamaServerBullet.new()

		b.canvas_item = RenderingServer.canvas_item_create()
		RenderingServer.canvas_item_set_parent(b.canvas_item, parent_ci)
		RenderingServer.canvas_item_set_visible(b.canvas_item, false)

		b.shape = PhysicsServer2D.circle_shape_create()
		PhysicsServer2D.shape_set_data(b.shape, 6.0)

		b.area = PhysicsServer2D.area_create()
		PhysicsServer2D.area_set_space(b.area, space)
		PhysicsServer2D.area_add_shape(b.area, b.shape)
		PhysicsServer2D.area_set_transform(b.area, Transform2D(0.0, Vector2(-100000.0, -100000.0)))
		PhysicsServer2D.area_set_monitorable(b.area, false)
		# Collision layer/mask are 0 until the bullet is active
		PhysicsServer2D.area_set_collision_layer(b.area, 0)
		PhysicsServer2D.area_set_collision_mask(b.area, 0)

		# Per-bullet monitor callback — captures b so we know which bullet was hit
		var capture_b := b
		PhysicsServer2D.area_set_monitor_callback(b.area,
			func(status: int, _body_rid: RID, body_iid: int, _body_shape: int, _area_shape: int) -> void:
				if status == PhysicsServer2D.AREA_BODY_ADDED and capture_b.active:
					bullet_hit.emit(capture_b, body_iid)
		)

		_all.append(b)
		_free_list.append(b)

# ---------------------------------------------------------------------------
# Spawn
# ---------------------------------------------------------------------------

## Spawns a server bullet. Called by [TamaSpawnManager] when a server-configured
## bullet type is fired. Returns the bullet, or null if the pool is exhausted.
func spawn(
		data:     _Interpreter.BulletFireData,
		config:   TamaServerBulletConfig,
		angle:    float,
		speed:    float,
		position: Vector2,
		context:  TamaContext
) -> TamaServerBullet:
	if _free_list.is_empty():
		return null

	var b: TamaServerBullet = _free_list.pop_back()
	b.active       = true
	b._active_index = _active.size()
	_active.append(b)

	b.position         = position
	b.angle            = angle
	b.speed            = speed
	b.speed_x          = 0.0
	b.speed_y          = 0.0
	b.rotates          = config.rotates
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

	PhysicsServer2D.shape_set_data(b.shape, config.shape_radius)
	PhysicsServer2D.area_set_collision_layer(b.area, config.collision_layer)
	PhysicsServer2D.area_set_collision_mask(b.area, config.collision_mask)

	RenderingServer.canvas_item_clear(b.canvas_item)
	if config.texture:
		var draw_rect := Rect2(
			config.rect.position * config.texture_scale,
			config.rect.size * config.texture_scale
		)
		RenderingServer.canvas_item_add_texture_rect(
			b.canvas_item, draw_rect, config.texture.get_rid()
		)
	const Z_RANGE := RenderingServer.CANVAS_ITEM_Z_MAX  # range is [1, Z_MAX], stays above z=0 nodes
	RenderingServer.canvas_item_set_z_index(b.canvas_item, 1 + _z_counter % Z_RANGE)
	_z_counter = (_z_counter + 1) % Z_RANGE
	var spawn_rot := b.angle if b.rotates else 0.0
	RenderingServer.canvas_item_set_transform(b.canvas_item, Transform2D(spawn_rot, b.position))
	RenderingServer.canvas_item_set_visible(b.canvas_item, true)

	# Emit a warning for unsupported emitter_act; the bullet still spawns without it.
	if data.bullet_emitter_act != null:
		push_warning("TamaServerBulletPool: bullet_emitter_act is not supported for server bullets. The sub-emitter act will be skipped.")

	var needs_runner := data.bullet_act != null or data.mvmt_x_set or data.mvmt_y_set
	if needs_runner:
		var runner = _Interpreter.new()
		runner.context = context
		runner._tree = get_tree()
		runner._frame_loop_owner = runner  # interpreter is its own frame-loop owner
		b._runner = runner

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

## Returns [param b] to the free list. Safe to call from signal callbacks.
func recycle(b: TamaServerBullet) -> void:
	if not b.active:
		return
	b.active = false

	RenderingServer.canvas_item_set_visible(b.canvas_item, false)
	PhysicsServer2D.area_set_transform(b.area, Transform2D(0.0, Vector2(-100000.0, -100000.0)))
	PhysicsServer2D.area_set_collision_layer(b.area, 0)
	PhysicsServer2D.area_set_collision_mask(b.area, 0)

	if b._runner:
		TamaManager._unregister_frame_loop(b._runner)
		b._runner.stop()  # prevent resumption after any pending await
		b._runner.call_deferred(&"free")  # deferred so we're not mid-signal-emission
		b._runner = null

	# O(1) unordered removal: swap with the last active entry
	var idx  := b._active_index
	var last := _active.back()
	_active[idx]       = last
	last._active_index = idx
	_active.pop_back()
	b._active_index = -1

	_free_list.append(b)

# ---------------------------------------------------------------------------
# Per-frame update
# ---------------------------------------------------------------------------

func _physics_process(delta: float) -> void:
	var bounds    := _world_bounds().grow(bounds_margin)
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
		RenderingServer.canvas_item_set_transform(b.canvas_item, Transform2D(rot, b.position))
		PhysicsServer2D.area_set_transform(b.area, Transform2D(0.0, b.position))

		if not bounds.has_point(b.position):
			to_recycle.append(b)

	for b in to_recycle:
		recycle(b)

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
	for b in _all:
		RenderingServer.free_rid(b.canvas_item)
		PhysicsServer2D.free_rid(b.area)
		PhysicsServer2D.free_rid(b.shape)
