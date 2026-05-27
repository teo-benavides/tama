extends Node

const EXAMPLE_BULLET_SCENE = preload("res://example_bullet.tscn")

func _ready() -> void:
	TamaScriptRepository.load_scripts("res://tamascripts")
	TamaSpawnManager.bullet_registry = {"example": EXAMPLE_BULLET_SCENE}
	$TamaEmitter.start()
