class_name TamaContext
extends RefCounted

func time() -> float:
	return float(Engine.get_physics_frames()) / float(Engine.physics_ticks_per_second)
