extends GPUParticles2D

func _ready() -> void:
	finished.connect(func (): queue_free())
