extends Node

const _Ast    = preload("res://tama_ast.gd")
const _Lexer  = preload("res://tama_lexer.gd")
const _Parser = preload("res://tama_parser.gd")

var _scripts: Dictionary = {}
var _scripts_dir: String = ""

## Parse every .tam/.tama file in path and cache the resulting ASTs.
## Call this once at startup before running any TamaEmitters.
func load_scripts(path: String) -> void:
	_scripts_dir = path
	var dir := DirAccess.open(path)
	if not dir:
		push_error("TamaScriptRepository: cannot open directory '%s'" % path)
		return
	dir.list_dir_begin()
	var file := dir.get_next()
	while not file.is_empty():
		if file.ends_with(".tam") or file.ends_with(".tama"):
			_parse_and_store(file.get_basename(), path.path_join(file))
		file = dir.get_next()
	dir.list_dir_end()

## Parse a single .tam/.tama file and cache it under its filename.
func load_script(filename: String, full_path: String) -> void:
	_parse_and_store(filename, full_path)

## Parse raw TamaScript source and cache it under the given name.
func load_script_from_source(name: String, source: String) -> void:
	var program := _parse_source(source, name, _make_resolver([]))
	if program:
		_scripts[name] = program

## Retrieve a previously loaded program by filename.
func get_tama_script(filename: String) -> _Ast.ProgramNode:
	return _scripts.get(filename)

## True if a script with this filename has been loaded.
func has_tama_script(filename: String) -> bool:
	return _scripts.has(filename)

func _parse_and_store(filename: String, full_path: String) -> void:
	var source := FileAccess.get_file_as_string(full_path)
	if source.is_empty():
		push_error("TamaScriptRepository: could not read '%s'" % full_path)
		return
	var program := _parse_source(source, full_path, _make_resolver([]))
	if program:
		_scripts[filename] = program

func _parse_source(source: String, label: String, resolver: Callable = Callable()) -> _Ast.ProgramNode:
	var tokens = _Lexer.new().tokenize(source)
	var result = _Parser.new().parse(tokens, resolver)
	if not result:
		push_error("TamaScriptRepository: parse failed for '%s'" % label)
	return result.program

func _find_script_path(name: String) -> String:
	if _scripts_dir.is_empty():
		return ""
	for path in [
		_scripts_dir.path_join(name),
		_scripts_dir.path_join(name + ".tama"),
		_scripts_dir.path_join(name + ".tam"),
	]:
		if FileAccess.file_exists(path):
			return path
	return ""

func _make_resolver(loading: Array) -> Callable:
	return func(name: String) -> _Ast.ProgramNode:
		if name in loading:
			push_warning("TamaScriptRepository: circular include '%s'" % name)
			return null
		if _scripts.has(name):
			return _scripts[name]
		var path := _find_script_path(name)
		if path.is_empty():
			push_warning("TamaScriptRepository: included script '%s' not found" % name)
			return null
		var source := FileAccess.get_file_as_string(path)
		if source.is_empty():
			return null
		loading.append(name)
		var tokens = _Lexer.new().tokenize(source)
		var result = _Parser.new().parse(tokens, _make_resolver(loading))
		loading.erase(name)
		if result.program:
			_scripts[name] = result.program
		return result.program
