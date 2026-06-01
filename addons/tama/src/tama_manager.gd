## Global manager for the Tama bullet hell framework.
##
## The single entry point for registering bullet scenes and configuring runtime behaviour.
## [TamaEmitter] and [TamaBullet] communicate with it automatically — user code only needs
## to call it directly to configure the registry and keep [member player_position] updated.
## TamaScript files are loaded automatically from the path set in Project Settings under
## [b]tama/scripts_path[/b] (default: [code]res://tamascripts[/code]).
##
## [codeblock]
## func _ready() -> void:
##     TamaManager.registry = load("res://my_bullet_registry.tres")
##     $TamaEmitter.start()
## [/codeblock]
extends Node

const _SpawnManagerScript  = preload("res://addons/tama/src/tama_spawn_manager.gd")
const _RepositoryScript    = preload("res://addons/tama/src/tama_script_repository.gd")
const _RegistryScript      = preload("res://addons/tama/src/tama_bullet_registry.gd")
const _ServerPoolScript    = preload("res://addons/tama/src/tama_server_bullet_pool.gd")

const _SETTING_SCRIPTS_PATH := "tama/scripts_path"
const _DEFAULT_SCRIPTS_PATH := "res://tamascripts"

var _spawn_manager: Node
var _repository:    Node
var _server_pool:   Node
var _scripts_path:  String = ""

var _frame_loop_fns:   Array[Callable] = []
var _frame_loop_owners: Array[Object]  = []

func _init() -> void:
	_repository    = _RepositoryScript.new()
	_spawn_manager = _SpawnManagerScript.new()
	_server_pool   = _ServerPoolScript.new()
	_spawn_manager._repository  = _repository
	_spawn_manager._server_pool = _server_pool

func _ready() -> void:
	add_child(_repository)
	add_child(_spawn_manager)
	add_child(_server_pool)
	_load_scripts()

func _physics_process(_delta: float) -> void:
	var i := 0
	while i < _frame_loop_fns.size():
		var sz := _frame_loop_fns.size()
		_frame_loop_fns[i].call()
		# If the callback removed itself, the next entry slid into index i — don't advance.
		if _frame_loop_fns.size() >= sz:
			i += 1

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

## Global position of the player, used to resolve [code]aim[/code] directions.
## Update this every frame (e.g. in [code]_physics_process[/code]) to keep aimed bullets tracking correctly.
var player_position: Vector2:
	set(v): _spawn_manager.player_position = v
	get:    return _spawn_manager.player_position

## Node under which spawned bullets are added as children.
## Defaults to the current scene root when left empty.
var spawn_parent: NodePath:
	set(v): _spawn_manager.spawn_parent = v
	get:    return _spawn_manager.spawn_parent

func _load_scripts(path: String = "") -> void:
	if path.is_empty():
		path = ProjectSettings.get_setting(_SETTING_SCRIPTS_PATH, _DEFAULT_SCRIPTS_PATH)
	_scripts_path = path
	_repository.load_scripts(path)

## Parses and caches a single TamaScript file located at [param full_path],
## storing it under [param filename] (the key used by [TamaEmitter]).
func load_script(filename: String, full_path: String) -> void:
	_repository.load_script(filename, full_path)

## Parses raw TamaScript [param source] and caches it under [param script_name].
## Useful for loading scripts from a [TextEdit] or other in-memory source.
func load_script_from_source(script_name: String, source: String) -> void:
	_repository.load_script_from_source(script_name, source)

## The [TamaContext] instance shared across all interpreters.
## Assign a custom subclass to expose additional functions to TamaScript expressions.
## Should be set before calling [method TamaEmitter.start].
var context: TamaContext:
	set(v): _spawn_manager.context = v
	get:    return _spawn_manager.context

## The bullet registry used for all spawned bullets.
## Assign a [TamaBulletRegistry] resource created in the editor, or populate it at
## runtime with [method register_bullet] and [method set_default_bullet].
var registry: TamaBulletRegistry:
	set(v):
		_spawn_manager.registry = v
	get:
		return _spawn_manager.registry

## Registers a [PackedScene] for the bullet type string [param type].
## The [param type] string must match the value used in a TamaScript [code]type[/code] statement.
func register_bullet(type: String, scene: PackedScene) -> void:
	_ensure_registry()
	_spawn_manager.registry.entries[type] = scene

## Registers a [TamaServerBulletConfig] for [param type].
## Bullets of this type will be rendered and simulated via [TamaServerBulletPool]
## rather than instantiating a scene, for maximum performance at high counts.
## Must be called before the first bullet of this type is fired.
func register_server_bullet(type: String, config: TamaServerBulletConfig) -> void:
	_ensure_registry()
	_spawn_manager.registry.server_configs[type] = config
	_server_pool.register_type(type, config)

## The [TamaServerBulletPool] node. Connect to its [code]bullet_hit[/code]
## signal to handle hit detection for server bullets.
var server_bullet_pool:
	get: return _server_pool

## Sets the fallback [PackedScene] used when a bullet has no [code]type[/code] statement
## or its type string is not registered.
func set_default_bullet(scene: PackedScene) -> void:
	_ensure_registry()
	_spawn_manager.registry.default_bullet = scene

func _register_frame_loop(owner: Object, fn: Callable) -> void:
	_frame_loop_owners.append(owner)
	_frame_loop_fns.append(fn)

func _unregister_frame_loop(owner: Object) -> void:
	var i := _frame_loop_owners.size() - 1
	while i >= 0:
		if _frame_loop_owners[i] == owner:
			_frame_loop_owners.remove_at(i)
			_frame_loop_fns.remove_at(i)
		i -= 1

func _ensure_registry() -> void:
	if not _spawn_manager.registry:
		_spawn_manager.registry = _RegistryScript.new()

# ---------------------------------------------------------------------------
# Internal — used by TamaEmitter. Not part of the public API.
# ---------------------------------------------------------------------------

func _get_scripts_path() -> String:
	if _scripts_path.is_empty():
		return ProjectSettings.get_setting(_SETTING_SCRIPTS_PATH, _DEFAULT_SCRIPTS_PATH)
	return _scripts_path

func _get_tama_script(filename: String):
	return _spawn_manager.get_tama_script(filename)

func _has_tama_script(filename: String) -> bool:
	return _repository.has_tama_script(filename)

func _get_script_from_repository(filename: String):
	return _repository.get_tama_script(filename)

func _connect_interpreter(interpreter, spawner: Node2D) -> void:
	_spawn_manager.connect_interpreter(interpreter, spawner)

func _get_context():
	return context
