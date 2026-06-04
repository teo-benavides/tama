extends TamaBullet


func _on_visible_on_screen_notifier_2d_screen_exited() -> void:
	destroy()
