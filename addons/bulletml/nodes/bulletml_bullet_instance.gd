@icon("res://addons/bulletml/icons/comet-blue.svg")
extends CharacterBody2D
class_name BulletMLBulletInstance
## Bullet spawnable from BulletML scripts.
##
## A bullet which can be spawned by BulletML.
## Extend this in your bullet scenes to allow them to be spawned by the addon.

## Emitted after [method Node._ready] is run.
signal bullet_ready(bullet: BulletMLBulletInstance)

## Emitted when destroying the bullet, before it is freed with
## [method Node.queue_free].
signal destroyed(type: String)

var _bullet : BulletMLBulletASTNode

var _runner: BulletMLRunner

var _type : String

var _angle : float
var _speed : float

var _speed_x : float
var _speed_y : float
var _current_speed : float

var _last_angle : float = 0
var _last_speed : float = 0

var _accel_inc : Vector2
var _accel_end : Vector2

var _actions_local : Array

var _accel_horizontal : float
var _accel_vertical : float = 0
var _accel_term : float = 0

var _direction_tween: Tween
var _speed_tween: Tween
var _accel_tween: Tween

## Initialize position internally.
## Disable if you want to set the position via the editor.
@export var initialize_position: bool = true

## Exempt from the [code]bulletml_bullet_instances[/code] group.
## If you free bullets using [method SceneTree.call_group],
## this is useful for keeping it alive.
@export var exempt_from_group: bool = false

## Whether the bullet rotates depending on its direction.
@export var rotates: bool = true

var _initial_position = Vector2()
var _shooter : String

func _ready():
    if not exempt_from_group:
        add_to_group("bulletml_bullet_instances")
    var screen = Vector2(ProjectSettings.get_setting("display/window/size/viewport_width"), ProjectSettings.get_setting("display/window/size/viewport_height"))
    if initialize_position:
        position = _initial_position
    if (position.x < 0 or position.x > screen.x) and (position.y < 0 or position.y > screen.y):
        set_physics_process(false)
        destroy()
        return
    
    if not _runner:
        _runner = BulletMLRunner.new()
    if not _runner in get_children():
        add_child(_runner)
    _runner.change_direction.connect(_on_change_direction)
    _runner.change_speed.connect(_on_change_speed)
    _runner.accel.connect(_on_accel)
    _runner.vanish.connect(_on_vanish)

    bullet_ready.emit(self)

func _physics_process(delta):
    velocity = Vector2.ZERO
    velocity.x += cos(_angle+(PI/2)*3) * _speed
    velocity.y += sin(_angle+(PI/2)*3) * _speed
    velocity.x += _speed_x
    velocity.y += _speed_y
    move_and_slide()

## Used internally.
## Executes any BulletML corresponding to this bullet.
func start():
    visible = true
    _runner.stack = _actions_local.duplicate(true)
    _runner.run()

## Destroy the bullet.
## Emits [signal destroyed] and runs [method Node.queue_free].
func destroy():
    destroyed.emit(_type)
    queue_free()

func _on_change_direction(change_direction : BulletMLChangeDirectionASTNode):
    var change_direction_angle = BulletMLContext._direction_to_value(change_direction.direction, self)
    if _direction_tween:
        _direction_tween.kill()
    _direction_tween = create_tween()
    _direction_tween.set_process_mode(Tween.TWEEN_PROCESS_PHYSICS)
    _direction_tween.tween_property(self, "_angle", change_direction_angle, change_direction.term.get_value()).set_trans(Tween.TRANS_LINEAR)

func _on_change_speed(change_speed : BulletMLChangeSpeedASTNode):
    var change_speed_value = BulletMLContext._speed_to_value(change_speed.speed, _last_speed)
    if _speed_tween:
        _speed_tween.kill()
    _speed_tween = create_tween()
    _speed_tween.set_process_mode(Tween.TWEEN_PROCESS_PHYSICS)
    _speed_tween.tween_property(self, "_speed", change_speed_value, change_speed.term.get_value()).set_trans(Tween.TRANS_LINEAR)

func _on_accel(accel : BulletMLAccelASTNode):
    _accel_end = BulletMLContext._accel_to_vec2_end(accel, _speed_x, _speed_y)
    if _accel_tween:
        _accel_tween.kill()
    _accel_tween = create_tween()
    _accel_tween.set_process_mode(Tween.TWEEN_PROCESS_PHYSICS)
    _accel_tween.set_parallel(true)
    _accel_tween.tween_property(self, "_speed_x", _accel_end.x, accel.term.get_value()).set_trans(Tween.TRANS_LINEAR)
    _accel_tween.tween_property(self, "_speed_y", _accel_end.y, accel.term.get_value()).set_trans(Tween.TRANS_LINEAR)


func _on_vanish():
    destroy()
