class_name TamaContext
extends RefCounted

func time() -> float:
	return Engine.get_physics_frames() / Engine.physics_ticks_per_second
