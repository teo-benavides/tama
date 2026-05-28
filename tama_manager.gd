extends Node

const _SpawnManagerScript  = preload("res://tama_spawn_manager.gd")
const _RepositoryScript    = preload("res://tama_script_repository.gd")
const _RegistryScript      = preload("res://tama_bullet_registry.gd")

var _spawn_manager: Node
var _repository:    Node

func _ready() -> void:
	_repository    = _RepositoryScript.new()
	_spawn_manager = _SpawnManagerScript.new()
	_spawn_manager._repository = _repository
	add_child(_repository)
	add_child(_spawn_manager)

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

## Set each frame to the player's world position so AIM directions resolve correctly.
var player_position: Vector2:
	set(v): _spawn_manager.player_position = v
	get:    return _spawn_manager.player_position

## NodePath to the node under which bullets are spawned.
## Defaults to the current scene root when not set.
var spawn_parent: NodePath:
	set(v): _spawn_manager.spawn_parent = v
	get:    return _spawn_manager.spawn_parent

## Load all .tama / .tam scripts found in path into the repository.
func load_scripts(path: String) -> void:
	_repository.load_scripts(path)

## Load a single script file into the repository under filename.
func load_script(filename: String, full_path: String) -> void:
	_repository.load_script(filename, full_path)

## Parse raw TamaScript source and cache it under name.
func load_script_from_source(script_name: String, source: String) -> void:
	_repository.load_script_from_source(script_name, source)

## Map a bullet type string to a PackedScene.
func register_bullet(type: String, scene: PackedScene) -> void:
	_ensure_registry()
	_spawn_manager.registry.entries[type] = scene

## Set the fallback scene used when a bullet has no type or its type is not registered.
func set_default_bullet(scene: PackedScene) -> void:
	_ensure_registry()
	_spawn_manager.registry.default_bullet = scene

func _ensure_registry() -> void:
	if not _spawn_manager.registry:
		_spawn_manager.registry = _RegistryScript.new()

# ---------------------------------------------------------------------------
# Internal — used by TamaEmitter. Not part of the public API.
# ---------------------------------------------------------------------------

func _get_tama_script(filename: String):
	return _spawn_manager.get_tama_script(filename)

func _has_tama_script(filename: String) -> bool:
	return _repository.has_tama_script(filename)

func _get_script_from_repository(filename: String):
	return _repository.get_tama_script(filename)

func _connect_interpreter(interpreter, spawner: Node2D) -> void:
	_spawn_manager.connect_interpreter(interpreter, spawner)

func _get_context():
	return _spawn_manager.context
