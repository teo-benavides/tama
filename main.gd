extends Node

const EXAMPLE_BULLET_SCENE = preload("res://example_bullet.tscn")

var emitter: TamaEmitter

func _ready() -> void:
	TamaScriptRepository.load_scripts("res://tamascripts")
	var reg := TamaBulletRegistry.new()
	reg.entries = {"example": EXAMPLE_BULLET_SCENE}
	reg.default_bullet = EXAMPLE_BULLET_SCENE
	TamaSpawnManager.registry = reg

func create_emitter():
	emitter = TamaEmitter.new()
	emitter.global_position = $EmitterPosition.global_position
	add_child(emitter)
	
func _on_text_edit_text_changed() -> void:
	TamaScriptRepository.load_script_from_source("temp", $CanvasLayer/TextEdit.text)
	get_tree().call_group(&"tama_emitters", &"queue_free")
	get_tree().call_group(&"tama_bullets", &"destroy")
	if emitter:
		emitter.queue_free()
	create_emitter()
	emitter.script_filename = "temp"
	emitter.start()
