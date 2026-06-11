extends CharacterBody2D

const SPEED = 200

func _ready() -> void:
	TamaManager.bullet_hit.connect(_on_server_bullet_hit)
	TamaManager.curved_laser_hit.connect(_on_server_curved_laser_hit)
	
func _physics_process(delta):
	var input = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	var velocity = input * SPEED
	set_velocity(velocity)
	move_and_slide()

	TamaManager.set_player_position(global_position)

func _on_server_bullet_hit(bullet) -> void:
	#print("bullet hit")
	pass

func _on_server_curved_laser_hit(laser) -> void:
	#print("curved laser hit")
	pass
