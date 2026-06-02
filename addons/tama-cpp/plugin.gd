@tool
extends EditorPlugin

const _SETTING_SCRIPTS_PATH  := "tama/scripts_path"
const _SETTING_COMPOSITE_THR := "tama/server_bullet_composite_threshold"

func _enable_plugin() -> void:
	# Remove legacy autoload from older versions of the plugin
	if ProjectSettings.has_setting("autoload/TamaManager"):
		remove_autoload_singleton("TamaManager")
	if not ProjectSettings.has_setting(_SETTING_SCRIPTS_PATH):
		ProjectSettings.set_setting(_SETTING_SCRIPTS_PATH, "res://tamascripts")
	ProjectSettings.set_initial_value(_SETTING_SCRIPTS_PATH, "res://tamascripts")
	ProjectSettings.add_property_info({
		"name": _SETTING_SCRIPTS_PATH,
		"type": TYPE_STRING,
		"hint": PROPERTY_HINT_DIR,
	})
	if not ProjectSettings.has_setting(_SETTING_COMPOSITE_THR):
		ProjectSettings.set_setting(_SETTING_COMPOSITE_THR, 1000)
	ProjectSettings.set_initial_value(_SETTING_COMPOSITE_THR, 1000)
	ProjectSettings.add_property_info({
		"name": _SETTING_COMPOSITE_THR,
		"type": TYPE_INT,
		"hint": PROPERTY_HINT_RANGE,
		"hint_string": "0,100000,1",
	})
	ProjectSettings.save()


func _disable_plugin() -> void:
	pass
