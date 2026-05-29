extends Node

const EXAMPLE_BULLET_SCENE = preload("res://addons/tama/example/bullets/example_bullet.tscn")

var emitter: TamaEmitter

func _ready() -> void:
	TamaManager.load_scripts("res://addons/tama/example/tamascripts")
	TamaManager.register_bullet("example", EXAMPLE_BULLET_SCENE)
	TamaManager.set_default_bullet(EXAMPLE_BULLET_SCENE)
	
func create_emitter():
	emitter = TamaEmitter.new()
	emitter.global_position = $EmitterPosition.global_position
	add_child(emitter)

func _on_text_edit_text_changed() -> void:
	TamaManager.load_script_from_source("temp", $CanvasLayer/TextEdit.text)
	get_tree().call_group(&"tama_emitters", &"queue_free")
	get_tree().call_group(&"tama_bullets", &"destroy")
	if emitter:
		emitter.queue_free()
	create_emitter()
	emitter.script_filename = "temp"
	emitter.start()
