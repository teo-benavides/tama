## Node that runs a TamaScript bullet pattern.
##
## Place a [TamaEmitter] anywhere in your scene tree. Set [member script_filename] to the
## name of a TamaScript file (without path or extension), then call [method start] at
## runtime after [method TamaManager.load_scripts] has finished.
##
## Export variables declared in the TamaScript with the [code]export[/code] keyword appear
## automatically in the Godot inspector under [b]TamaScript Exports[/b] and can also be
## set at runtime via [method set_export_num] / [method set_export_str].
@tool
@icon("res://addons/tama/icons/ammunition.svg")
extends Node2D
class_name TamaEmitter

const _Interpreter = preload("res://addons/tama/src/tama_interpreter.gd")
const _Ast         = preload("res://addons/tama/src/tama_ast.gd")
const _Lexer       = preload("res://addons/tama/src/tama_lexer.gd")
const _Parser      = preload("res://addons/tama/src/tama_parser.gd")

var _script_filename: String
## Name of the TamaScript to run (no path, no extension).
## The file is looked up in the directory passed to [method TamaManager.load_scripts].
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

## When [code]true[/code], this emitter is not added to the [code]tama_emitters[/code] group
## and will not be affected by group-wide calls such as [code]get_tree().call_group()[/code].
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
			var prop_type: int
			match def["type"]:
				"num":  prop_type = TYPE_FLOAT
				"bool": prop_type = TYPE_BOOL
				_:      prop_type = TYPE_STRING
			list.append({
				"name": "tama_export_" + def["name"],
				"type": prop_type,
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
			return def["default"]  # already stored as the correct GDScript type
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
	var dir := ProjectSettings.get_setting("tama/scripts_path", "res://tamascripts") as String
	for path in [
		dir.path_join(_script_filename),
		dir.path_join(_script_filename + ".tama"),
		dir.path_join(_script_filename + ".tam"),
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
		var dir := ProjectSettings.get_setting("tama/scripts_path", "res://tamascripts") as String
		for path in [
			dir.path_join(name),
			dir.path_join(name + ".tama"),
			dir.path_join(name + ".tam"),
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

## Starts executing the TamaScript. Has no effect if [member script_filename] is not set
## or the script has not been loaded via [TamaManager].
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

## Stops the running TamaScript immediately. Safe to call when not running.
func stop() -> void:
	if _interpreter and _running:
		_interpreter.stop()
		_running = false

# ---------------------------------------------------------------------------
# Export variable API
# ---------------------------------------------------------------------------

## Sets a TamaScript export variable at runtime, bypassing the inspector.
## Use [method set_export_num] or [method set_export_str] for typed variants.
func set_export(name: String, value: Variant) -> void:
	_export_values[name] = value

## Sets a [code]num[/code] export variable named [param name] to [param value].
func set_export_num(name: String, value: float) -> void:
	_export_values[name] = value

## Sets a [code]str[/code] export variable named [param name] to [param value].
func set_export_str(name: String, value: String) -> void:
	_export_values[name] = value

## Sets a [code]bool[/code] export variable named [param name] to [param value].
func set_export_bool(name: String, value: bool) -> void:
	_export_values[name] = value

## Returns the current value of the export variable named [param name],
## or [code]null[/code] if it has not been set.
func get_export(name: String) -> Variant:
	return _export_values.get(name)
