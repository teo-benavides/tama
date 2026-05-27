extends Node
## Used for loading BulletML scripts into memory.

var _parsed_scripts : Dictionary = {}
var _temp_script: BulletMLParsedScript

## Load BulletML scripts (XML files) from the given path.
## You must call this before starting any [BulletMLBulletEmitter]s.
func load_scripts(path: String):
    if not path.ends_with("/") and not path.ends_with("\\"):
        path = path + "/"
    var dir := DirAccess.open(path)
    dir.list_dir_begin() # TODOConverter3To4 fill missing arguments https://github.com/godotengine/godot/pull/40547

    var file = dir.get_next()
    while not file.is_empty():
        _set_bulletml_parsed_script(file, BulletMLScriptLoader.parse_script(path + file))
        file = dir.get_next()

    dir.list_dir_end()

## Load a temporary script.
## You must pass in a full script (not a filename, but the script contents)
## which can later be run from [BulletMLBulletEmitter]s with
## [member BulletMLBulletEmitter.use_temp_script] set to [code]true[/code].
func load_temp_script(script: String):
    _temp_script = BulletMLScriptLoader.parse_temp_script(script.to_utf8_buffer())

func _get_bulletml_parsed_script(script_name : String) -> BulletMLParsedScript:
    var script = _parsed_scripts.get(script_name)
    return script

func _get_bulletml_temp_parsed_script() -> BulletMLParsedScript:
    return _temp_script
  
func _set_bulletml_parsed_script(script_name : String, parsed_script : BulletMLParsedScript):
    _parsed_scripts[script_name] = parsed_script
