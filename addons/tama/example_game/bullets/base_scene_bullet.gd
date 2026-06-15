extends TamaBullet

@export var out_of_bounds_margin: float = 16.0

func _bullet_process(delta: float) -> void:
	var world: Rect2 = TamaManager.world_rect
	if not world.grow(out_of_bounds_margin).has_point(global_position):
		destroy()
		
