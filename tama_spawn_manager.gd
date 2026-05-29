extends Node

const _Interpreter = preload("res://tama_interpreter.gd")
const _Ast         = preload("res://tama_ast.gd")

## Registry resource mapping bullet type strings to PackedScenes, with an optional default.
var registry: TamaBulletRegistry

var _repository: Node

## Node under which spawned bullets are added.
var spawn_parent: NodePath

## Context object for expression evaluation. Shared across all interpreters.
var context: TamaContext = TamaContext.new()

## Updated each frame by the game to allow AIM direction resolution.
var player_position: Vector2 = Vector2.ZERO

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

## Connect an interpreter's bullet_fired signal so this manager handles spawning.
## spawner is the Node2D that owns the interpreter (TamaEmitter or TamaBullet).
func connect_interpreter(interpreter, spawner: Node2D) -> void:
	interpreter.bullet_fired.connect(func(data): _on_bullet_fired(data, spawner))

## Retrieve a program from the repository by filename.
func get_tama_script(filename: String):
	var prog = _repository.get_tama_script(filename)
	if not prog:
		push_error("TamaSpawnManager: script '%s' not found in repository" % filename)
	return prog

# ---------------------------------------------------------------------------
# Bullet firing
# ---------------------------------------------------------------------------

func _on_bullet_fired(data: _Interpreter.BulletFireData, spawner: Node2D) -> void:
	# Resolve bullet_type if it's a bullet param variable name rather than a literal
	if not data.bullet_type.is_empty() and not data.bullet_params.is_empty():
		for i in mini(data.bullet_params.size(), data.bullet_args.size()):
			if data.bullet_params[i] == data.bullet_type:
				data.bullet_type = str(data.bullet_args[i])
				break
	var scene: PackedScene
	if registry:
		if data.bullet_type.is_empty():
			scene = registry.default_bullet
		else:
			scene = registry.entries.get(data.bullet_type) as PackedScene
			if not scene:
				push_error("TamaSpawnManager: unknown bullet type '%s', using default bullet" % data.bullet_type)
				scene = registry.default_bullet
	if not scene:
		push_error("TamaSpawnManager: no scene for bullet type '%s' (no default set)" % data.bullet_type)
		return
	var bullet := scene.instantiate() as TamaBullet
	if not bullet:
		push_error("TamaSpawnManager: scene for type '%s' is not a TamaBullet" % data.bullet_type)
		return

	var bullet_runner = _Interpreter.new()
	bullet_runner.context = context
	bullet.add_child(bullet_runner)
	bullet._runner = bullet_runner

	var angle := _resolve_angle(data, spawner)
	var speed := _resolve_speed(data, spawner)
	spawner.set("_last_angle", angle)
	spawner.set("_last_speed", speed)

	bullet._angle = angle
	bullet._speed = speed
	bullet._initial_position = _resolve_position(data, spawner, angle)

	_get_spawn_parent().call_deferred("add_child", bullet)

	if data.bullet_act:
		var act_scope: Dictionary = {}
		for i in mini(data.bullet_params.size(), data.bullet_args.size()):
			act_scope[data.bullet_params[i]] = data.bullet_args[i]
		connect_interpreter(bullet_runner, bullet)
		bullet.ready.connect(
			func(): bullet_runner.start_act(data.source_program, data.bullet_act, act_scope),
			CONNECT_ONE_SHOT
		)

	if data.bullet_emitter_act != null:
		# When the bullet also has an act, the act already owns bullet_runner.
		# Create a separate interpreter so both run in parallel.
		var spawner_runner
		if data.bullet_act:
			spawner_runner = _Interpreter.new()
			spawner_runner.context = context
			bullet.add_child(spawner_runner)
		else:
			spawner_runner = bullet_runner
		connect_interpreter(spawner_runner, bullet)
		# Build the bullet scope so emitter acts can resolve param-held act refs.
		var emitter_scope: Dictionary = {}
		for i in mini(data.bullet_params.size(), data.bullet_args.size()):
			emitter_scope[data.bullet_params[i]] = data.bullet_args[i]
		bullet.ready.connect(
			func(): spawner_runner.start_act(data.source_program, data.bullet_emitter_act, emitter_scope),
			CONNECT_ONE_SHOT
		)


# ---------------------------------------------------------------------------
# Resolution helpers
# ---------------------------------------------------------------------------

func _resolve_angle(data: _Interpreter.BulletFireData, spawner: Node2D) -> float:
	match data.dir_type:
		_Ast.DirType.AIM:
			return (player_position - spawner.global_position).angle() + deg_to_rad(data.dir_value)
		_Ast.DirType.ABS:
			return deg_to_rad(data.dir_value)
		_Ast.DirType.REL:
			return spawner.rotation + deg_to_rad(data.dir_value)
		_Ast.DirType.SEQ:
			return spawner.get("_last_angle") + deg_to_rad(data.dir_value)
	return spawner.get_angle_to(player_position) + deg_to_rad(data.dir_value)

func _resolve_speed(data: _Interpreter.BulletFireData, spawner: Node2D) -> float:
	match data.speed_type:
		_Ast.ValueType.ABS:
			return data.speed_value
		_Ast.ValueType.REL, _Ast.ValueType.SEQ:
			return spawner.get("_last_speed") + data.speed_value
	return data.speed_value

func _resolve_position(data: _Interpreter.BulletFireData, spawner: Node2D, bullet_angle: float) -> Vector2:
	match data.offset_mode:
		_Ast.OffsetMode.INLINE:
			return spawner.global_position + Vector2(0.0, -data.offset_value).rotated(bullet_angle)
		_Ast.OffsetMode.BLOCK:
			var pos := spawner.global_position
			match data.offset_x_type:
				_Ast.ValueType.ABS, _Ast.ValueType.SEQ:
					pos.x = data.offset_x
				_Ast.ValueType.REL:
					pos.x += data.offset_x
			match data.offset_y_type:
				_Ast.ValueType.ABS, _Ast.ValueType.SEQ:
					pos.y = data.offset_y
				_Ast.ValueType.REL:
					pos.y += data.offset_y
			return pos
	return spawner.global_position

func _get_spawn_parent() -> Node:
	if not spawn_parent.is_empty():
		var n := get_node_or_null(spawn_parent)
		if n:
			return n
	return get_tree().get_current_scene()
