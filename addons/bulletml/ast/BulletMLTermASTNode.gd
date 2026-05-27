extends BulletMLValueElementASTNode
## Internal BulletML class.

class_name BulletMLTermASTNode

func get_value() -> float:
    return float(super.get_value()) / BulletMLContext.FRAME_MULTIPLIER
