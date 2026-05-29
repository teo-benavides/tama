@tool
extends EditorPlugin


func _enable_plugin() -> void:
	add_autoload_singleton("TamaManager", "res://addons/tama/src/tama_manager.gd")


func _disable_plugin() -> void:
	remove_autoload_singleton("TamaManager")
