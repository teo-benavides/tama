extends CharacterBody2D

const SPEED = 200

func _physics_process(delta):
    var input = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
    var velocity = input * SPEED
    set_velocity(velocity)
    move_and_slide()
    
    BulletMLContext.player_position = global_position
