extends BulletMLBulletEmitter

var t: float = 0.0
var center : Vector2 = Vector2(150, 100)

func _physics_process(delta):
    var s = 0.4 #speed
    var w = 100 #width
    var h = 33 #height
    t += delta
    position = Vector2(sin(2*PI*t*s)*w, sin(4*PI*t*s)*h) + center
