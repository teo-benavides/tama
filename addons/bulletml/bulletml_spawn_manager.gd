extends Node

## Emitted when a [BulletMLBulletInstance] is destroyed,
## before its [method Node.queue_free].
signal bullet_destroyed(bullet_type: String)

## Resource holding bullet scenes with their type strings.
## You must set this before starting any [BulletMLBulletEmitter]s.
var bullet_registry: BulletMLBulletRegistry

func _spawn_bullet(spawner : BulletMLBulletInstance, fire : BulletMLFireASTNode, params = []):
    var bullet : BulletMLBulletInstance = _instance_bullet(fire.bullet.type)
    var direction = fire.bullet.direction
    var speed = fire.bullet.speed
    var runner = BulletMLRunner.new()
    bullet._runner = runner

    if fire.direction:
        direction = fire.direction
    if fire.speed:
        speed = fire.speed
    if params:
        if not params.is_empty():
            bullet._runner.params_stack.append(params)
            
    bullet.destroyed.connect(_on_bullet_destroyed)
    spawner._last_angle = BulletMLContext._direction_to_value(direction, spawner)
    spawner._last_speed = BulletMLContext._speed_to_value(speed, spawner._last_speed)
    
    bullet._bullet = fire.bullet
    
    bullet._type = fire.bullet.type
    
    bullet._actions_local = fire.bullet.actions
    bullet._runner.actions = spawner._runner.actions
    bullet._runner.fires = spawner._runner.fires
    bullet._runner.bullets = spawner._runner.bullets
    bullet._shooter = bullet._bullet.shooter
    
    bullet._angle = spawner._last_angle
    bullet._speed = spawner._last_speed

    bullet._initial_position = BulletMLContext._offset_position(spawner.global_position, fire.offset, bullet._angle)
    
    if bullet.rotates:
        bullet.rotation = bullet._angle

    bullet.bullet_ready.connect(_on_bullet_ready)
    if not BulletMLContext.spawn_parent:
        assert(false, "spawn_parent not set! Make sure to set it by assigning a NodePath to BulletMLContext.spawn_parent.")
    var spawn_parent_node = get_node(BulletMLContext.spawn_parent)
    if not spawn_parent_node:
        assert(false, "spawn_parent doesn't point to a valid Node! Make sure to update it by assigning a valid NodePath to BulletMLContext.spawn_parent.")
    spawn_parent_node.call_deferred("add_child", bullet)

func _spawn_emitter(actions: Dictionary, fires: Dictionary, bullets: Dictionary, global_position: Vector2, top: String):
    var new_shooter = BulletMLBulletEmitter.new()
    var new_runner = BulletMLRunner.new()
    new_shooter._runner = new_runner
    new_shooter._runner.actions = actions.duplicate(true)
    new_shooter._runner.fires = fires.duplicate(true)
    new_shooter._runner.bullets = bullets.duplicate(true)
    new_shooter._runner.global_position = global_position

    if not BulletMLContext.spawn_parent:
        assert(false, "spawn_parent not set! Make sure to set it by assigning a NodePath to BulletMLContext.spawn_parent.")
    var spawn_parent_node = get_node(BulletMLContext.spawn_parent)
    if not spawn_parent_node:
        assert(false, "spawn_parent doesn't point to a valid Node! Make sure to update it by assigning a valid NodePath to BulletMLContext.spawn_parent.")
    spawn_parent_node.add_child(new_shooter) # CHANGE THIS PLEASE
    new_shooter.add_child(new_runner)
    new_shooter._runner.run_specific(top)

func _instance_bullet(type : String) -> BulletMLBulletInstance:
    if not bullet_registry:
        assert(false, "bullet_registry not set! Create a BulletMLBulletRegistry resource and assign it at runtime to BulletMLSpawnManager.bullet_registry.")
    var bullet = bullet_registry.entries.get(type)
    if bullet:
        return bullet.instantiate()
    assert(false, "Bullet type " + type + " not found! Make sure you have registered it in your BulletMLBulletRegistry resource.")
    return null

func _on_bullet_ready(bullet : BulletMLBulletInstance):
    if not bullet._shooter.is_empty():
        var new_shooter = BulletMLBulletEmitter.new()
        new_shooter.script_filename = bullet._shooter
        new_shooter.bullet_ready.connect(_on_bullet_ready)
        bullet.call_deferred("add_child", new_shooter)
    bullet.start()

func _on_bullet_destroyed(bullet_type: String):
    bullet_destroyed.emit(bullet_type)
