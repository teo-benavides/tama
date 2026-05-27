extends BulletMLValueElementASTNode
## Internal BulletML class.

class_name BulletMLSpeedASTNode

enum Type {ABSOLUTE, RELATIVE, SEQUENCE}

var type = Type.ABSOLUTE

func get_value():
    return super.get_value() * BulletMLContext.SPEED_MULTIPLIER
