extends Label

func _process(delta: float) -> void:
	text = "Bullets: " + str(TamaManager.bullet_count)
