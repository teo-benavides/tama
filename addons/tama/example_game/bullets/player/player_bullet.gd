extends "res://addons/tama/example_game/bullets/base_scene_bullet.gd"

func _bullet_ready():
	$SpawnParticles.emitting = true

func _on_area_2d_area_entered(area: Area2D) -> void:
	destroy()
