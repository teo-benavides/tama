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
	var cx := sx - radius * cos(ofs)
	return cx + (radius + exp_spd * dt) * cos(ofs + rot_dir * rot_spd * dt)

func spiral_y(sy: float, radius: float, ofs: float, exp_spd: float, rot_spd: float, rot_dir: float, t0: float) -> float:
	var dt := time() - t0
	var cy := sy - radius * sin(ofs)
	return cy + (radius + exp_spd * dt) * sin(ofs + rot_dir * rot_spd * dt)

func group_target_dir(ofs: float, pent_angle: float, circ_r: float, inner_r: float) -> float:
	var step     := PI / 60.0
	var gc_ofs   = floor(ofs / (step * 4.0)) * (step * 4.0) + step * 1.5
	var gc_angle = gc_ofs + pent_angle - PI / 2.0
	var b_angle  := ofs   + pent_angle - PI / 2.0
	var dx := inner_r * cos(gc_angle) - circ_r * cos(b_angle)
	var dy := inner_r * sin(gc_angle) - circ_r * sin(b_angle)
	return rad_to_deg(atan2(dy, dx))

func f2s(frames: int) -> float:
	return float(frames) / float(Engine.physics_ticks_per_second)
