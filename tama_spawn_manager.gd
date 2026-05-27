extends Node
## Registry mapping bullet type strings to PackedScenes extending TamaBullet.
var bullet_registry: Dictionary[String, PackedScene] = {}

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
func connect_interpreter(interpreter: TamaInterpreter, spawner: Node2D) -> void:
	interpreter.bullet_fired.connect(func(data): _on_bullet_fired(data, spawner))

## Retrieve a program from the repository by filename.
func get_tama_script(filename: String) -> TamaAst.ProgramNode:
	var prog: TamaAst.ProgramNode = TamaScriptRepository.get_tama_script(filename)
	if not prog:
		push_error("TamaSpawnManager: script '%s' not found in repository" % filename)
	return prog

# ---------------------------------------------------------------------------
# Bullet firing
# ---------------------------------------------------------------------------

func _on_bullet_fired(data: TamaInterpreter.BulletFireData, spawner: Node2D) -> void:
	var scene := bullet_registry.get(data.bullet_type) as PackedScene
	if not scene:
		push_error("TamaSpawnManager: unknown bullet type '%s'" % data.bullet_type)
		return
	var bullet := scene.instantiate() as TamaBullet
	if not bullet:
		push_error("TamaSpawnManager: scene for type '%s' is not a TamaBullet" % data.bullet_type)
		return

	var bullet_runner := TamaInterpreter.new()
	bullet_runner.context = context
	bullet.add_child(bullet_runner)
	bullet._runner = bullet_runner
	bullet.player_position = player_position

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

	if not data.bullet_spawner.is_empty():
		# When the bullet also has an act, the act already owns bullet_runner.
		# Create a separate interpreter so both run in parallel.
		var spawner_runner: TamaInterpreter
		if data.bullet_act:
			spawner_runner = TamaInterpreter.new()
			spawner_runner.context = context
			bullet.add_child(spawner_runner)
		else:
			spawner_runner = bullet_runner
		connect_interpreter(spawner_runner, bullet)
		bullet.ready.connect(
			func(): _start_spawner(spawner_runner, data.bullet_spawner),
			CONNECT_ONE_SHOT
		)

# ---------------------------------------------------------------------------
# Spawner support
# ---------------------------------------------------------------------------

func _start_spawner(interpreter: TamaInterpreter, spawner_filename: String) -> void:
	var filename := spawner_filename if spawner_filename.get_extension() != "" else spawner_filename + ".tam"
	var prog := get_tama_script(filename)
	if prog:
		interpreter.start(prog)


# ---------------------------------------------------------------------------
# Resolution helpers
# ---------------------------------------------------------------------------

func _resolve_angle(data: TamaInterpreter.BulletFireData, spawner: Node2D) -> float:
	match data.dir_type:
		TamaAst.DirType.AIM:
			return (player_position - spawner.global_position).angle() + deg_to_rad(data.dir_value)
		TamaAst.DirType.ABS:
			return deg_to_rad(data.dir_value)
		TamaAst.DirType.REL:
			return spawner.rotation + deg_to_rad(data.dir_value)
		TamaAst.DirType.SEQ:
			return spawner.get("_last_angle") + deg_to_rad(data.dir_value)
	return spawner.get_angle_to(player_position) + deg_to_rad(data.dir_value)

func _resolve_speed(data: TamaInterpreter.BulletFireData, spawner: Node2D) -> float:
	match data.speed_type:
		TamaAst.ValueType.ABS:
			return data.speed_value
		TamaAst.ValueType.REL, TamaAst.ValueType.SEQ:
			return spawner.get("_last_speed") + data.speed_value
	return data.speed_value

func _resolve_position(data: TamaInterpreter.BulletFireData, spawner: Node2D, bullet_angle: float) -> Vector2:
	match data.offset_mode:
		TamaAst.OffsetMode.INLINE:
			return spawner.global_position + Vector2(0.0, -data.offset_value).rotated(bullet_angle)
		TamaAst.OffsetMode.BLOCK:
			var pos := spawner.global_position
			match data.offset_x_type:
				TamaAst.ValueType.ABS, TamaAst.ValueType.SEQ:
					pos.x = data.offset_x
				TamaAst.ValueType.REL:
					pos.x += data.offset_x
			match data.offset_y_type:
				TamaAst.ValueType.ABS, TamaAst.ValueType.SEQ:
					pos.y = data.offset_y
				TamaAst.ValueType.REL:
					pos.y += data.offset_y
			return pos
	return spawner.global_position

func _get_spawn_parent() -> Node:
	if not spawn_parent.is_empty():
		var n := get_node_or_null(spawn_parent)
		if n:
			return n
	return get_parent()
