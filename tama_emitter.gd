@tool
extends Node2D
class_name TamaEmitter

const _Interpreter = preload("res://tama_interpreter.gd")
const _Ast         = preload("res://tama_ast.gd")
const _Lexer       = preload("res://tama_lexer.gd")
const _Parser      = preload("res://tama_parser.gd")

var _script_filename: String
@export var script_filename: String:
	set(value):
		_script_filename = value
		var old_sig := _exports_signature()
		_refresh_exports()
		_last_modified_time = _get_script_mtime()
		if _exports_signature() != old_sig:
			notify_property_list_changed()
	get():
		return _script_filename

@export var excluded_from_group: bool = false

var _interpreter
var _running: bool = false

@warning_ignore("unused_private_class_variable")
var _last_angle: float = 0.0
@warning_ignore("unused_private_class_variable")
var _last_speed: float = 0.0

# Stored export values: {var_name -> float or String}
var _export_values: Dictionary = {}
# Cached list of {name, type} dicts derived from the script's export declarations.
var _cached_export_defs: Array = []

var _last_modified_time: int = 0
var _poll_timer: float = 0.0
const _POLL_INTERVAL := 1.0

func _ready() -> void:
	if Engine.is_editor_hint():
		_refresh_exports()
		_last_modified_time = _get_script_mtime()
		notify_property_list_changed()
		return
	if not excluded_from_group:
		add_to_group(&"tama_emitters")

func _process(delta: float) -> void:
	if not Engine.is_editor_hint():
		return
	_poll_timer += delta
	if _poll_timer < _POLL_INTERVAL:
		return
	_poll_timer = 0.0
	_check_file_changed()

func _check_file_changed() -> void:
	if _script_filename.is_empty():
		return
	var mtime := _get_script_mtime()
	if mtime == 0 or mtime == _last_modified_time:
		return
	_last_modified_time = mtime
	var old_sig := _exports_signature()
	_refresh_exports()
	if _exports_signature() != old_sig:
		notify_property_list_changed()

# ---------------------------------------------------------------------------
# Dynamic inspector properties for TamaScript exports
# ---------------------------------------------------------------------------

func _get_property_list() -> Array[Dictionary]:
	var list: Array[Dictionary] = []
	if not _cached_export_defs.is_empty():
		list.append({
			"name": "TamaScript Exports",
			"type": TYPE_NIL,
			"usage": PROPERTY_USAGE_GROUP,
			"hint_string": "tama_export_",
		})
		for def in _cached_export_defs:
			list.append({
				"name": "tama_export_" + def["name"],
				"type": TYPE_FLOAT if def["type"] == "num" else TYPE_STRING,
				"usage": PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE,
				"hint": PROPERTY_HINT_NONE,
				"hint_string": "",
			})
	return list

func _set(property: StringName, value: Variant) -> bool:
	if str(property).begins_with("tama_export_"):
		var key := str(property).substr("tama_export_".length())
		_export_values[key] = value
		return true
	return false

func _get(property: StringName) -> Variant:
	if str(property).begins_with("tama_export_"):
		var key := str(property).substr("tama_export_".length())
		return _export_values.get(key)
	return null

func _property_can_revert(property: StringName) -> bool:
	if not str(property).begins_with("tama_export_"):
		return false
	var name := str(property).substr("tama_export_".length())
	for def in _cached_export_defs:
		if def["name"] == name:
			return def["default"] != null
	return false

func _property_get_revert(property: StringName) -> Variant:
	var name := str(property).substr("tama_export_".length())
	for def in _cached_export_defs:
		if def["name"] == name:
			return def["default"]
	return null

func _refresh_exports() -> void:
	_cached_export_defs = []
	if _script_filename.is_empty():
		return
	var program: _Ast.ProgramNode
	if Engine.is_editor_hint():
		program = _parse_script_file_for_exports()
	else:
		if TamaManager._has_tama_script(_script_filename):
			program = TamaManager._get_script_from_repository(_script_filename)
	if not program:
		return
	for exp in program.exports:
		_cached_export_defs.append({"name": exp.name, "type": exp.export_type, "default": exp.default_value})
		if exp.default_value != null and not _export_values.has(exp.name):
			_export_values[exp.name] = exp.default_value


func _exports_signature() -> String:
	var parts: Array[String] = []
	for def in _cached_export_defs:
		parts.append(def["name"] + ":" + def["type"])
	return ",".join(parts)

func _find_script_path() -> String:
	for path in [
		"res://tamascripts/" + _script_filename,
		"res://tamascripts/" + _script_filename + ".tama",
		"res://tamascripts/" + _script_filename + ".tam",
	]:
		if FileAccess.file_exists(path):
			return path
	return ""

func _get_script_mtime() -> int:
	var path := _find_script_path()
	return FileAccess.get_modified_time(path) if not path.is_empty() else 0

func _parse_script_file_for_exports() -> _Ast.ProgramNode:
	var path := _find_script_path()
	if path.is_empty():
		return null
	var source := FileAccess.get_file_as_string(path)
	if source.is_empty():
		return null
	var tokens = _Lexer.new().tokenize(source)
	var result = _Parser.new().parse(tokens, _make_editor_resolver())
	if not result.ok():
		for err in result.errors:
			push_warning("TamaEmitter [%s] parse error: %s" % [_script_filename, str(err)])
	return result.program

func _make_editor_resolver() -> Callable:
	return func(name: String) -> _Ast.ProgramNode:
		for path in [
			"res://tamascripts/" + name,
			"res://tamascripts/" + name + ".tama",
			"res://tamascripts/" + name + ".tam",
		]:
			if FileAccess.file_exists(path):
				var src := FileAccess.get_file_as_string(path)
				if not src.is_empty():
					var tokens = _Lexer.new().tokenize(src)
					return _Parser.new().parse(tokens).program
		return null

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

func start() -> void:
	if _script_filename.is_empty():
		push_error("TamaEmitter: script_filename is not set")
		return
	var program = TamaManager._get_tama_script(_script_filename)
	if not program:
		return
	_interpreter = _Interpreter.new()
	_interpreter.context = TamaManager._get_context()
	TamaManager._connect_interpreter(_interpreter, self)
	add_child(_interpreter)
	_running = true
	await _interpreter.start(program, _export_values.duplicate())
	_running = false

func stop() -> void:
	if _interpreter and _running:
		_interpreter.stop()
		_running = false

# ---------------------------------------------------------------------------
# Export variable API
# ---------------------------------------------------------------------------

func set_export(name: String, value: Variant) -> void:
	_export_values[name] = value

func set_export_num(name: String, value: float) -> void:
	_export_values[name] = value

func set_export_str(name: String, value: String) -> void:
	_export_values[name] = value

func get_export(name: String) -> Variant:
	return _export_values.get(name)
