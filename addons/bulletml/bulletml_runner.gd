extends Node
class_name BulletMLRunner
## Internal BulletML class.

signal change_direction(change_direction: BulletMLChangeDirectionASTNode)
signal change_speed(change_speed: BulletMLChangeSpeedASTNode)
signal accel(accel: BulletMLAccelASTNode)
signal vanish
signal spawn_requested

var bullet_instance

var actions : Dictionary
var fires : Dictionary
var bullets : Dictionary
var stack : Array # front is top due to the need for concatenation
var params_stack : Array # back is top
var tops : Array
var timer : Timer

func _ready():
    timer = Timer.new()
    timer.one_shot = true
    timer.process_callback = Timer.TIMER_PROCESS_PHYSICS
    bullet_instance = get_parent()
    add_child(timer)

func run_top():
    stack = actions["top"].commands.duplicate(true)
    run()

func run_specific(label : String):
    stack = actions[label].commands.duplicate(true)
    run()

func run_actions(actions : Array):
    stack = actions.duplicate(true)
    run()

func run():
    while len(stack) >= 1:
        var command = stack.pop_front()
        if command is BulletMLActionASTNode:
            stack = command.commands + stack
        elif command is BulletMLActionRefASTNode:
            push_params(command.params)
            stack = actions[command.label].commands + [BulletMLPopParamsASTNode.new()] + stack
        elif command is BulletMLFireASTNode:
            run_fire(command)
        elif command is BulletMLFireRefASTNode:
            push_params(command.params)
            run_fire(fires[command.label])
        elif command is BulletMLChangeSpeedASTNode:
            command.speed.expression = parse_value(command.speed.value)
            command.term.expression = parse_value(command.term.value)
            change_speed.emit(command)
        elif command is BulletMLChangeDirectionASTNode:
            command.direction.expression = parse_value(command.direction.value)
            command.term.expression = parse_value(command.term.value)
            change_direction.emit(command)
        elif command is BulletMLAccelASTNode:
            if command.horizontal:
                command.horizontal.expression = parse_value(command.horizontal.value)
            if command.vertical:
                command.vertical.expression = parse_value(command.vertical.value)
            command.term.expression = parse_value(command.term.value)
            accel.emit(command)
        elif command is BulletMLVanishASTNode:
            vanish.emit()
        elif command is BulletMLRepeatASTNode:
            var rep_action = BulletMLRepeatedActionASTNode.new()
            rep_action.action = command.action
            command.times.expression = parse_value(command.times.value)
            rep_action.times = command.times.get_value()
            stack.push_front(rep_action)
        elif command is BulletMLRepeatedActionASTNode:
            if command.times > 0:
                command.times -= 1
                stack.push_front(command)
                stack = command.action.commands + stack
            elif command.times <= -1:
                stack.push_front(command)
                stack = command.action.commands + stack
        elif command is BulletMLWaitASTNode:
            command.expression = parse_value(command.value)
            timer.start(command.get_value())
            await timer.timeout
        elif command is BulletMLPopParamsASTNode:
            params_stack.pop_back()

func stop():
    stack = []

func push_params(params : Array):
    if params.is_empty():
        return
    params_stack.append(create_params(params))

func create_params(params : Array) -> Array:
    if params.is_empty():
        return []
    
    var new_params : Array

    if params_stack.is_empty():
        for param in params:
            new_params.append(parse_value(param).execute([], BulletMLContext))
    else:
        for param in params:
            new_params.append(parse_value(param).execute([], BulletMLContext))
    
    return new_params

func parse_value(value : String) -> Expression:
    var expression = null
    if value.is_empty() or value == null:
        expression = BulletMLContext._parse_expression("0")
    elif params_stack.is_empty():
        expression = BulletMLContext._parse_expression(value)
    else:
        var new_value = value
        for result in BulletMLContext._param_regex.search_all(value):
            new_value = BulletMLContext._param_regex.sub(new_value, "(%s)" % params_stack.back()[int(result.strings[1]) - 1])
        expression = BulletMLContext._parse_expression(new_value)
    
    return expression

func run_fire(fire : BulletMLFireASTNode):
    if fire.direction:
        fire.direction.expression = parse_value(fire.direction.value)
    if fire.speed:
        fire.speed.expression = parse_value(fire.speed.value)
    if fire.offset:
        fire.offset.expression = parse_value(fire.offset.value)
        if fire.offset.horizontal:
            fire.offset.horizontal.expression = parse_value(fire.offset.horizontal.value)
        if fire.offset.vertical:
            fire.offset.vertical.expression = parse_value(fire.offset.vertical.value)
    if fire.bullet_ref:
        fire.bullet = bullets[fire.bullet_ref.label]
        if not fire.bullet_ref.shooter.is_empty():
            fire.bullet.shooter = fire.bullet_ref.shooter
        BulletMLSpawnManager._spawn_bullet(bullet_instance, fire, create_params(fire.bullet_ref.params))
    else:
        BulletMLSpawnManager._spawn_bullet(bullet_instance, fire, params_stack.back() if not params_stack.is_empty() else [])
