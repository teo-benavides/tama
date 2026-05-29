extends CharacterBody2D
class_name TamaBullet

const _Ast = preload("res://tama_ast.gd")

var _runner

var _angle:   float = 0.0
var _speed:   float = 0.0
var _speed_x: float = 0.0
var _speed_y: float = 0.0

var _last_angle: float = 0.0
var _last_speed: float = 0.0

var _direction_tween: Tween
var _speed_tween:     Tween
var _accel_tween:     Tween

## Whether the bullet rotates to match the direction it's going in.
@export var rotates: bool = true

var _initial_position := Vector2.ZERO


func _ready() -> void:
	add_to_group(&"tama_bullets")
	global_position = _initial_position
	var screen := Vector2(
		ProjectSettings.get_setting("display/window/size/viewport_width"),
		ProjectSettings.get_setting("display/window/size/viewport_height")
	)
	if (global_position.x < 0 or global_position.x > screen.x) and (global_position.y < 0 or global_position.y > screen.y):
		set_physics_process(false)
		destroy()
		return

	if rotates:
		rotation = _angle

	_runner.changed_direction.connect(_on_changed_direction)
	_runner.changed_speed.connect(_on_changed_speed)
	_runner.accelerated.connect(_on_accelerated)
	_runner.vanished.connect(_on_vanished)

func _physics_process(_delta: float) -> void:
	velocity = Vector2.ZERO
	velocity.x += cos(_angle) * _speed
	velocity.y += sin(_angle) * _speed
	velocity.x += _speed_x
	velocity.y += _speed_y
	if rotates:
		rotation = _angle
	move_and_slide()

func destroy() -> void:
	queue_free()

# ---------------------------------------------------------------------------
# Signal handlers
# ---------------------------------------------------------------------------

func _on_changed_direction(data) -> void:
	var target := _dir_to_angle(data.dir_type, data.dir_value)
	_last_angle = _angle
	if _direction_tween:
		_direction_tween.kill()
	_direction_tween = create_tween()
	_direction_tween.set_process_mode(Tween.TWEEN_PROCESS_PHYSICS)
	_direction_tween.tween_property(self, "_angle", target, data.over).set_trans(Tween.TRANS_LINEAR)

func _on_changed_speed(data) -> void:
	var target := _spd_to_value(data.speed_type, data.speed_value)
	_last_speed = _speed
	if _speed_tween:
		_speed_tween.kill()
	_speed_tween = create_tween()
	_speed_tween.set_process_mode(Tween.TWEEN_PROCESS_PHYSICS)
	_speed_tween.tween_property(self, "_speed", target, data.over).set_trans(Tween.TRANS_LINEAR)

func _on_accelerated(data) -> void:
	if _accel_tween:
		_accel_tween.kill()
	_accel_tween = create_tween()
	_accel_tween.set_process_mode(Tween.TWEEN_PROCESS_PHYSICS)
	_accel_tween.set_parallel(true)
	if data.has_x:
		var end_x := _accel_axis_end(data.x_type, data.x, _speed_x, data.over)
		_accel_tween.tween_property(self, "_speed_x", end_x, data.over).set_trans(Tween.TRANS_LINEAR)
	if data.has_y:
		var end_y := _accel_axis_end(data.y_type, data.y, _speed_y, data.over)
		_accel_tween.tween_property(self, "_speed_y", end_y, data.over).set_trans(Tween.TRANS_LINEAR)

func _on_vanished() -> void:
	destroy()

# ---------------------------------------------------------------------------
# Conversion helpers
# ---------------------------------------------------------------------------

func _dir_to_angle(dir_type, value: float) -> float:
	match dir_type:
		_Ast.DirType.AIM:
			return (TamaManager.player_position - global_position).angle() + deg_to_rad(value)
		_Ast.DirType.ABS:
			return deg_to_rad(value)
		_Ast.DirType.REL:
			return _angle + deg_to_rad(value)
		_Ast.DirType.SEQ:
			return _last_angle + deg_to_rad(value)
	return get_angle_to(TamaManager.player_position) + deg_to_rad(value)

func _spd_to_value(speed_type, value: float) -> float:
	match speed_type:
		_Ast.ValueType.ABS:
			return value
		_Ast.ValueType.REL:
			return _speed + value
		_Ast.ValueType.SEQ:
			return _last_speed + value
	return value

func _accel_axis_end(axis_type, value: float, current: float, over: float) -> float:
	match axis_type:
		_Ast.ValueType.ABS, _Ast.ValueType.SEQ:
			return value
		_Ast.ValueType.REL:
			return (value - current) * over
	return value
