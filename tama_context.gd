extends RefCounted
class_name TamaContext

func time() -> float:
	return float(Engine.get_physics_frames()) / float(Engine.physics_ticks_per_second)

func get_1_in(number: int) -> int:
	return 1 if randi() % number == 0 else 0
