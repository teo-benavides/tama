extends RefCounted
class_name BulletMLValueElementASTNode
## Internal BulletML class.

var expression : Expression
var value : String

func get_value():
    if expression:
        var result = expression.execute([], BulletMLContext)
        if result == null:
            return 0
        return result
    else:
        return 0
