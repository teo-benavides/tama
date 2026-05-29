@tool
extends EditorPlugin

const _SETTING_SCRIPTS_PATH := "tama/scripts_path"

func _enable_plugin() -> void:
	add_autoload_singleton("TamaManager", "res://addons/tama/src/tama_manager.gd")
	if not ProjectSettings.has_setting(_SETTING_SCRIPTS_PATH):
		ProjectSettings.set_setting(_SETTING_SCRIPTS_PATH, "res://tamascripts")
	ProjectSettings.set_initial_value(_SETTING_SCRIPTS_PATH, "res://tamascripts")
	ProjectSettings.add_property_info({
		"name": _SETTING_SCRIPTS_PATH,
		"type": TYPE_STRING,
		"hint": PROPERTY_HINT_DIR,
	})


func _disable_plugin() -> void:
	remove_autoload_singleton("TamaManager")
