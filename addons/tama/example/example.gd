extends Node

const EXAMPLE_BULLET_SCENE = preload("res://addons/tama/example/bullets/example_bullet.tscn")

var emitter: TamaEmitter
var _loading_script := false

func _ready() -> void:
	TamaManager.register_bullet("example", EXAMPLE_BULLET_SCENE)
	TamaManager.set_default_bullet(EXAMPLE_BULLET_SCENE)
	TamaManager.context = ExampleTamaContext.new()
	_populate_script_list()
	_on_option_button_item_selected(0)

func _populate_script_list() -> void:
	var btn: OptionButton = $CanvasLayer/Container/OptionButton
	var scripts_path := ProjectSettings.get_setting("tama/scripts_path", "res://tamascripts") as String
	var dir := DirAccess.open(scripts_path)
	if not dir:
		return
	dir.list_dir_begin()
	var file := dir.get_next()
	while not file.is_empty():
		if file.ends_with(".tama") or file.ends_with(".tam"):
			btn.add_item(file.get_basename())
		file = dir.get_next()
	dir.list_dir_end()

func _on_option_button_item_selected(index: int) -> void:
	var script_name = $CanvasLayer/Container/OptionButton.get_item_text(index)
	var scripts_path := ProjectSettings.get_setting("tama/scripts_path", "res://tamascripts") as String
	for ext in [".tama", ".tam", ""]:
		var path := scripts_path.path_join(script_name + ext)
		if FileAccess.file_exists(path):
			_loading_script = true
			$CanvasLayer/Container/TextEdit.text = FileAccess.get_file_as_string(path)
			_loading_script = false
			break
	_restart_emitter(script_name)

func _restart_emitter(script_name: String) -> void:
	get_tree().call_group(&"tama_emitters", &"queue_free")
	get_tree().call_group(&"tama_bullets", &"destroy")
	if emitter:
		emitter.queue_free()
	emitter = TamaEmitter.new()
	emitter.global_position = $EmitterPosition.global_position
	add_child(emitter)
	emitter.script_filename = script_name
	emitter.start()

func _on_text_edit_text_changed() -> void:
	if _loading_script:
		return
	TamaManager.load_script_from_source("temp", $CanvasLayer/Container/TextEdit.text)
	_restart_emitter("temp")
