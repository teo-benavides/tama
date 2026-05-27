@tool
extends EditorPlugin

func _enter_tree():
    add_autoload_singleton("BulletMLContext", "res://addons/bulletml/bulletml_context.gd")
    add_autoload_singleton("BulletMLScriptRepository", "res://addons/bulletml/bulletml_script_repository.gd")
    add_autoload_singleton("BulletMLSpawnManager", "res://addons/bulletml/bulletml_spawn_manager.gd")

func _exit_tree():
    remove_autoload_singleton("BulletMLContext")
    remove_autoload_singleton("BulletMLScriptRepository")
    remove_autoload_singleton("BulletMLSpawnManager")
