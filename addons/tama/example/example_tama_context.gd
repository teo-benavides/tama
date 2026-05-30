extends TamaContext
class_name ExampleTamaContext

func mid_x() -> float:
	return DisplayServer.window_get_size().x / 2.0

func mid_y() -> float:
	return DisplayServer.window_get_size().y / 2.0

func end_x() -> float:
	return DisplayServer.window_get_size().x

func end_y() -> float:
	return DisplayServer.window_get_size().y

func spiral_x(sx: float, radius: float, ofs: float, exp_spd: float, rot_spd: float, rot_dir: float, t0: float) -> float:
	var dt := time() - t0
	return sx + (radius + exp_spd * dt) * cos(ofs + rot_dir * rot_spd * dt)

func spiral_y(sy: float, radius: float, ofs: float, exp_spd: float, rot_spd: float, rot_dir: float, t0: float) -> float:
	var dt := time() - t0
	return sy + (radius + exp_spd * dt) * sin(ofs + rot_dir * rot_spd * dt)
