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

