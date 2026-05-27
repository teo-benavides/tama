extends Node2D
class_name TamaEmitter

## Filename of the .tam script to run (must be loaded in spawn_manager.repository first).
@export var script_filename: String
@export var excluded_from_group: bool = false

var _interpreter: TamaInterpreter
var _running: bool = false

@warning_ignore("unused_private_class_variable")
var _last_angle: float = 0.0
@warning_ignore("unused_private_class_variable")
var _last_speed: float = 0.0

func _ready() -> void:
	if not excluded_from_group:
		add_to_group(&"tama_emitters")

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

func start() -> void:
	if script_filename.is_empty():
		push_error("TamaEmitter: script_filename is not set")
		return
	var program: TamaAst.ProgramNode = TamaSpawnManager.get_tama_script(script_filename)
	if not program:
		return
	_interpreter = TamaInterpreter.new()
	_interpreter.context = TamaSpawnManager.context
	TamaSpawnManager.connect_interpreter(_interpreter, self)
	add_child(_interpreter)
	_running = true
	await _interpreter.start(program)
	_running = false

func stop() -> void:
	if _interpreter and _running:
		_interpreter.stop()
		_running = false
