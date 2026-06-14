extends CharacterBody2D

const SPEED = 200

var is_shooting = false

func _ready() -> void:
	TamaManager.bullet_hit.connect(_on_server_bullet_hit)
	TamaManager.curved_laser_hit.connect(_on_server_curved_laser_hit)
	
func _physics_process(delta):
	var input = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	var velocity = input * SPEED
	set_velocity(velocity)
	move_and_slide()

	TamaManager.set_player_position(global_position)
	
	if !is_shooting and Input.is_action_just_pressed("ui_accept"):
		is_shooting = true
		$TamaEmitter.start()
		
	if is_shooting and Input.is_action_just_released("ui_accept"):
		is_shooting = false
		$TamaEmitter.stop()

func _on_server_bullet_hit(bullet) -> void:
	#print("bullet hit")
	pass

func _on_server_curved_laser_hit(laser) -> void:
	#print("curved laser hit")
	pass
