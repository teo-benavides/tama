extends Node

const FRAME_MULTIPLIER = 1
#const FRAME_MULTIPLIER = 60
const SPEED_MULTIPLIER = 1
#const SPEED_MULTIPLIER = 300

var _param_regex = RegEx.new()
var _delta_inc = 0

## Update this every [method Node._process] or [method Node._physics_process]
## in your game.
## It's used for bullets which aim at the player.
var player_position: Vector2

## Where bullets will be spawned.
## You must set this before starting any [BulletMLBulletEmitter]s.
var spawn_parent: NodePath

func _ready():
    _param_regex.compile("\\$([0-9]+)")

func _physics_process(delta):
    _delta_inc += delta

func _parse_expression(expression : String) -> Expression:
    var ex = Expression.new()
    if ex.parse(expression) == 0:
        return ex
    else:
        assert(false, "Bad expression (" + expression + ")!")
        return null

## Can be used from within BulletML scripts to get the current game time.
## Simply use [code]time()[/code] wherever you need it from within your scripts.
## Useful for some patterns.
func time() -> float:
    return _delta_inc

func _direction_to_value(direction : BulletMLDirectionASTNode, bullet_instance : BulletMLBulletInstance) -> float:
    if direction != null:
        if direction.type != null:
            match direction.type:
                BulletMLDirectionASTNode.Type.AIM:
                    return bullet_instance.get_angle_to(player_position)+(PI/2) + deg_to_rad(direction.get_value())
                BulletMLDirectionASTNode.Type.ABSOLUTE:
                    return deg_to_rad(direction.get_value())
                BulletMLDirectionASTNode.Type.RELATIVE:
                    return bullet_instance._angle + deg_to_rad(direction.get_value())
                BulletMLDirectionASTNode.Type.SEQUENCE:
                    return bullet_instance._last_angle + deg_to_rad(direction.get_value())
                _:
                    return bullet_instance._last_angle + deg_to_rad(direction.get_value())
        if direction.value:
            return bullet_instance.get_angle_to(player_position)+(PI/2)*3 + deg_to_rad(direction.get_value())
        return bullet_instance.get_angle_to(player_position)+(PI/2)*3
    return bullet_instance.get_angle_to(player_position)+(PI/2)*3

func _speed_to_value(speed : BulletMLSpeedASTNode, last_speed : float) -> float:
    if speed != null:
        match speed.type:
            BulletMLSpeedASTNode.Type.ABSOLUTE:
                return speed.get_value()
            BulletMLSpeedASTNode.Type.RELATIVE:
                return speed.get_value()
            BulletMLSpeedASTNode.Type.SEQUENCE:
                return last_speed + speed.get_value()
            _:
                return last_speed + speed.get_value()
        return speed.get_value()
    return 0.0

func _offset_to_value(offset : BulletMLOffsetASTNode) -> float:
    if offset != null:
        return offset.get_value()
    return 0.0

func _offset_position(position : Vector2, offset : BulletMLOffsetASTNode, rotation = 0.0) -> Vector2:
    var vec2 = Vector2()
    if offset != null:
        if not offset.value.is_empty():
            return position + Vector2(0, -offset.get_value()).rotated(rotation)
        else:
            if offset.horizontal != null:
                match offset.horizontal.type:
                    BulletMLHorizontalASTNode.Type.ABSOLUTE:
                        vec2.x = offset.horizontal.get_value()
                    BulletMLHorizontalASTNode.Type.SEQUENCE:
                        vec2.x = offset.horizontal.get_value()
                    BulletMLHorizontalASTNode.Type.RELATIVE:
                        vec2.x = position.x + offset.horizontal.get_value()
                    _:
                        vec2.x = position.x + offset.horizontal.get_value()
            else:
                vec2.x = position.x
                
            if offset.vertical != null:
                match offset.vertical.type:
                    BulletMLVerticalASTNode.Type.ABSOLUTE:
                        vec2.y = offset.vertical.get_value()
                    BulletMLVerticalASTNode.Type.SEQUENCE:
                        vec2.y = offset.vertical.get_value()
                    BulletMLVerticalASTNode.Type.RELATIVE:
                        vec2.y = position.y + offset.vertical.get_value()
                    _:
                        vec2.y = position.y + offset.vertical.get_value()
            else:
                vec2.y = position.y
    return vec2

# Returns final velocity for given BulletMLAccelASTNode and current velocity
# Copied from https://github.com/daishihmr/bulletml.js/blob/master/src/main/bulletml.runner.js
# bulletml.runner.SubRunner.prototype.accel @ line 400
# Shouldn't there be a difference between ABSOLUTE and SEQUENCE?
# Maybe ABSOLUTE is fine, while SEQUENCE should be the formula in RELATIVE,
# and RELATIVE should be current.velocity + accel.get_value().
func _accel_to_vec2_end(accel : BulletMLAccelASTNode, speed_x : float, speed_y : float) -> Vector2:
    var vec2 = Vector2()
    if accel.horizontal:
        match accel.horizontal.type:
            BulletMLHorizontalASTNode.Type.ABSOLUTE:
                vec2.x = accel.horizontal.get_value()
            BulletMLHorizontalASTNode.Type.SEQUENCE:
                vec2.x = accel.horizontal.get_value()
            BulletMLHorizontalASTNode.Type.RELATIVE:
                vec2.x = (accel.horizontal.get_value() - speed_x) * accel.term.get_value()
            _:
                vec2.x = accel.horizontal.get_value()
    else:
        vec2.x = speed_x
    if accel.vertical:
        match accel.vertical.type:
            BulletMLVerticalASTNode.Type.ABSOLUTE:
                vec2.y = accel.vertical.get_value()
            BulletMLVerticalASTNode.Type.SEQUENCE:
                vec2.y = accel.vertical.get_value()
            BulletMLVerticalASTNode.Type.RELATIVE:
                vec2.y = (accel.vertical.get_value() - speed_y) * accel.term.get_value()
            _:
                vec2.y = accel.vertical.get_value()
    else:
        vec2.y = speed_y
    return vec2
    
func _accel_to_vec2_inc(accel : BulletMLAccelASTNode, current_velocity : Vector2 = Vector2(0, 0)) -> Vector2:
    var vec2 = Vector2()
    if accel.horizontal:
        match accel.horizontal.type:
            BulletMLHorizontalASTNode.Type.ABSOLUTE:
                vec2.x = (accel.horizontal.get_value() - current_velocity.x) / accel.term.get_value()
            BulletMLHorizontalASTNode.Type.SEQUENCE:
                vec2.x = (accel.horizontal.get_value() - current_velocity.x) / accel.term.get_value()
            BulletMLHorizontalASTNode.Type.RELATIVE:
                vec2.x = accel.horizontal.get_value()
            _:
                vec2.x = (accel.horizontal.get_value() - current_velocity.x) / accel.term.get_value()
    if accel.vertical:
        match accel.vertical.type:
            BulletMLVerticalASTNode.Type.ABSOLUTE:
                vec2.y = (accel.vertical.get_value() - current_velocity.y) / accel.term.get_value()
            BulletMLVerticalASTNode.Type.SEQUENCE:
                vec2.y = (accel.vertical.get_value() - current_velocity.y) / accel.term.get_value()
            BulletMLVerticalASTNode.Type.RELATIVE:
                vec2.y = accel.vertical.get_value()
            _:
                vec2.y = (accel.vertical.get_value() - current_velocity.y) / accel.term.get_value()
    return vec2
    
