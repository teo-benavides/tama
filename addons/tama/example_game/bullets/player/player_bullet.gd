extends "res://addons/tama/example_game/bullets/base_scene_bullet.gd"

var _spawn_pos: Vector2
var _hold_pos: bool = true

func _bullet_ready():
	_spawn_pos = global_position
	$SpawnParticles.emitting = true
	$SpawnParticles2.emitting = true
	$SpawnParticles.reparent(get_tree().root)
	$SpawnParticles2.reparent(get_tree().root)

func _bullet_process(delta: float) -> void:
	if _hold_pos:
		_hold_pos = false
		global_position = _spawn_pos
	super._bullet_process(delta)

func _on_area_2d_area_entered(area: Area2D) -> void:
	destroy()
