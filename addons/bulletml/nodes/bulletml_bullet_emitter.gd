@icon("res://addons/bulletml/icons/gun-blue.svg")
extends BulletMLBulletInstance
class_name BulletMLBulletEmitter
## Executes a BulletML script.
##
## Executes the BulletML script provided in [member script_filename],
## or a temporary script previously loaded through
## [method BulletMLScriptRepository.load_temp_script].  

## Whether to use the temporary script loaded via
## [method BulletMLScriptRepository.load_temp_script].
@export var use_temp_script: bool = false

## Filename of the BulletML script to run.
## Note that you don't need to provide the whole path,
## but just the filename itself, as you have previously loaded the path via
## [method BulletMLScriptRepository.load_scripts].
@export var script_filename: String

var _top : String
var _running = false

## Start execution of the BulletML script indicated by [member script_filename],
## or the temp script loaded via
## [method BulletMLScriptRepository.load_temp_script]
## if [member use_temp_script] is set to true.
func start():
    var script = BulletMLScriptRepository._get_bulletml_temp_parsed_script() if use_temp_script else BulletMLScriptRepository._get_bulletml_parsed_script(script_filename)
    if not script_filename.is_empty() and not script and not use_temp_script:
        printerr("BulletML script " + script_filename + " not found! Did you load scripts with BulletMLScriptRepository.load_scripts()?")
    if script:
        _runner.actions = script.actions
        _runner.fires = script.fires
        _runner.bullets = script.bullets
        _runner.tops = script.tops

        for top in _runner.tops.slice(1, len(_runner.tops)):
            BulletMLSpawnManager._spawn_emitter(_runner.actions, _runner.fires, _runner.bullets, global_position, top)

        _top = _runner.tops[0]
        _running = true
        _runner.run_specific(_top)
    elif not _runner.actions.is_empty():
        _runner.run_actions(_actions_local)

## Stop BulletML script execution.
func stop():
    if _running:
        _runner.stop()
        _running = false

## Resume BulletML script execution.
func resume():
    if not _running:
        _running = true
        _runner.run_specific(_top)
