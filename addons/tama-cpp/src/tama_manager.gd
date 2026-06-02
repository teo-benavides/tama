extends TamaManagerBase

func _ready() -> void:
	load_scripts(ProjectSettings.get_setting("tama/scripts_path", "res://tamascripts"))
