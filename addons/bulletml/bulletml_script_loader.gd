extends RefCounted
class_name BulletMLScriptLoader
## Internal BulletML class.

static func parse_script(file : String) -> BulletMLParsedScript:
    var xml = XMLParser.new()
    var script = BulletMLParsedScript.new()
    var err = xml.open(file)
    if err:
        return null
    while !xml.read():
        match xml.get_node_name():
            "action":
                var action = create_action(xml)
                script.actions[action.label] = action
                if action.label.left(3) == "top":
                    script.tops.append(action.label)
            "fire":
                var fire = create_fire(xml)
                script.fires[fire.label] = fire
            "bullet":
                var bullet = create_bullet(xml)
                script.bullets[bullet.label] = bullet
    return script
    
static func parse_temp_script(buffer : PackedByteArray) -> BulletMLParsedScript:
    var xml = XMLParser.new()
    var script = BulletMLParsedScript.new()
    var err = xml.open_buffer(buffer)
    if err:
        return null
    while !xml.read():
        match xml.get_node_name():
            "action":
                var action = create_action(xml)
                script.actions[action.label] = action
                if action.label.left(3) == "top":
                    script.tops.append(action.label)
            "fire":
                var fire = create_fire(xml)
                script.fires[fire.label] = fire
            "bullet":
                var bullet = create_bullet(xml)
                script.bullets[bullet.label] = bullet
    return script

static func create_fire(xml : XMLParser) -> BulletMLFireASTNode:
    var fire = BulletMLFireASTNode.new()
    fire.label = xml.get_named_attribute_value_safe("label")
    fire.offset = BulletMLOffsetASTNode.new()
    xml.read()
    while not is_closing("fire", xml):
        match xml.get_node_name():
            "direction":
                fire.direction = create_direction(xml)
            "speed":
                fire.speed = create_speed(xml)
            "offset":
                fire.offset = create_offset(xml)
            "bullet":
                fire.bullet = create_bullet(xml)
            "bulletRef":
                fire.bullet_ref = create_bullet_ref(xml)
        if xml.read():
            break
    return fire
                
static func create_bullet(xml : XMLParser) -> BulletMLBulletASTNode:
    var bullet = BulletMLBulletASTNode.new()
    bullet.type = xml.get_named_attribute_value_safe("type")
    bullet.shooter = xml.get_named_attribute_value_safe("shooter")
    if xml.is_empty():
        return bullet
    bullet.label = xml.get_named_attribute_value_safe("label")
    xml.read()
    while not is_closing("bullet", xml):
        match xml.get_node_name():
            "direction":
                bullet.direction = create_direction(xml)
            "speed":
                bullet.speed = create_speed(xml)
            "action":
                bullet.actions.append(create_action(xml))
            "actionRef":
                bullet.action_refs.append(create_action_ref(xml))
        if xml.read():
            break
    return bullet

static func create_action(xml : XMLParser) -> BulletMLActionASTNode:
    var action = BulletMLActionASTNode.new()
    action.label = xml.get_named_attribute_value_safe("label")
    while not is_closing("action", xml):
        if xml.read():
            break
        match xml.get_node_name():
            "repeat":
                action.commands.append(create_repeat(xml))
            "fire":
                action.commands.append(create_fire(xml))
            "fireRef":
                action.commands.append(create_fire_ref(xml))
            "changeSpeed":
                action.commands.append(create_change_speed(xml))
            "changeDirection":
                action.commands.append(create_change_direction(xml))
            "accel":
                action.commands.append(create_accel(xml))
            "wait":
                action.commands.append(create_wait(xml))
            "vanish":
                action.commands.append(create_vanish(xml))
            "action":
                action.commands.append(create_action(xml))
            "actionRef":
                action.commands.append(create_action_ref(xml))
    return action

static func create_direction(xml : XMLParser) -> BulletMLDirectionASTNode:
    var direction = BulletMLDirectionASTNode.new()
    match xml.get_named_attribute_value("type"):
        "aim":
            direction.type = BulletMLDirectionASTNode.Type.AIM
        "absolute":
            direction.type = BulletMLDirectionASTNode.Type.ABSOLUTE
        "relative":
            direction.type = BulletMLDirectionASTNode.Type.RELATIVE
        "sequence":
            direction.type = BulletMLDirectionASTNode.Type.SEQUENCE
    xml.read() # go to text node
    direction.value = xml.get_node_data()
    skip_to_closing("direction", xml)
    return direction

static func create_speed(xml : XMLParser) -> BulletMLSpeedASTNode:
    var speed = BulletMLSpeedASTNode.new()
    match xml.get_named_attribute_value("type"):
        "absolute":
            speed.type = BulletMLSpeedASTNode.Type.ABSOLUTE
        "relative":
            speed.type = BulletMLSpeedASTNode.Type.RELATIVE
        "sequence":
            speed.type = BulletMLSpeedASTNode.Type.SEQUENCE
    xml.read() # go to text node
    speed.value = xml.get_node_data()
    skip_to_closing("speed", xml)
    return speed

static func create_offset(xml : XMLParser) -> BulletMLOffsetASTNode:
    var offset = BulletMLOffsetASTNode.new()
    offset.value = xml.get_named_attribute_value("value")
    xml.read()
    while not is_closing("offset", xml):
        match xml.get_node_name():
            "horizontal":
                offset.horizontal = create_horizontal(xml)
            "vertical":
                offset.vertical = create_vertical(xml)   
        if xml.read():
            break
    skip_to_closing("offset", xml)
    return offset   

static func create_repeat(xml : XMLParser) -> BulletMLRepeatASTNode:
    var repeat = BulletMLRepeatASTNode.new()
    repeat.times = BulletMLTimesASTNode.new()
    xml.read()
    if xml.get_node_name() == "times":
        xml.read() # advance to text node
        repeat.times.value = xml.get_node_data()
        xml.read()
    else:
        repeat.times.value = "-1"
    while xml.get_node_name() != "action":
        if xml.read():
            break
    repeat.action = create_action(xml)
    skip_to_closing("repeat", xml)
    return repeat

static func create_wait(xml : XMLParser) -> BulletMLWaitASTNode:
    var wait = BulletMLWaitASTNode.new()
    xml.read() # advance to text node
    wait.value = xml.get_node_data()
    skip_to_closing("wait", xml)
    return wait

static func create_fire_ref(xml : XMLParser) -> BulletMLFireRefASTNode:
    var fire_ref = BulletMLFireRefASTNode.new()
    fire_ref.label = xml.get_named_attribute_value("label")
    if not xml.is_empty():
        xml.read()
        while not is_closing("fireRef", xml):
            if xml.get_node_name() == "param" and not is_closing("param", xml):
                xml.read() # advance to text node
                fire_ref.params.append(xml.get_node_data())
            if xml.read():
                break
    skip_to_closing("fireRef", xml)
    return fire_ref

static func create_action_ref(xml : XMLParser) -> BulletMLActionRefASTNode:
    var action_ref = BulletMLActionRefASTNode.new()
    action_ref.label = xml.get_named_attribute_value("label")
    if not xml.is_empty():
        xml.read()
        while not is_closing("actionRef", xml):
            if xml.get_node_name() == "param" and not is_closing("param", xml):
                xml.read() # advance to text node
                action_ref.params.append(xml.get_node_data())
            if xml.read():
                break
    skip_to_closing("actionRef", xml)
    return action_ref

static func create_bullet_ref(xml : XMLParser) -> BulletMLBulletRefASTNode:
    var bullet_ref = BulletMLBulletRefASTNode.new()
    bullet_ref.label = xml.get_named_attribute_value("label")
    bullet_ref.shooter = xml.get_named_attribute_value("shooter")
    if not xml.is_empty():
        xml.read()
        while not is_closing("bulletRef", xml):
            if xml.get_node_name() == "param" and not is_closing("param", xml):
                xml.read() # advance to text node
                bullet_ref.params.append(xml.get_node_data())
            if xml.read():
                break  
    skip_to_closing("bulletRef", xml)
    return bullet_ref

static func create_vanish(xml : XMLParser) -> BulletMLVanishASTNode:
    var vanish = BulletMLVanishASTNode.new()
    skip_to_closing("vanish", xml)
    return vanish

static func create_change_direction(xml : XMLParser) -> BulletMLChangeDirectionASTNode:
    var change_direction = BulletMLChangeDirectionASTNode.new()
    xml.read()
    while not is_closing("changeDirection", xml):
        match xml.get_node_name():
            "direction":
                change_direction.direction = create_direction(xml)
            "term":
                change_direction.term = create_term(xml)
        if xml.read():
            break
    skip_to_closing("changeDirection", xml)
    return change_direction

static func create_change_speed(xml : XMLParser) -> BulletMLChangeSpeedASTNode:
    var change_speed = BulletMLChangeSpeedASTNode.new()
    xml.read()
    while not is_closing("changeSpeed", xml):
        match xml.get_node_name():
            "speed":
                change_speed.speed = create_speed(xml)
            "term":
                change_speed.term = create_term(xml)
        if xml.read():
            break
    skip_to_closing("changeSpeed", xml)
    return change_speed

static func create_accel(xml : XMLParser) -> BulletMLAccelASTNode:
    var accel = BulletMLAccelASTNode.new()
    xml.read()
    while not is_closing("accel", xml):
        match xml.get_node_name():
            "horizontal":
                accel.horizontal = create_horizontal(xml)
            "vertical":
                accel.vertical = create_vertical(xml)   
            "term":
                accel.term = create_term(xml)
        if xml.read():
            break
    skip_to_closing("accel", xml)
    return accel   
    
static func create_horizontal(xml : XMLParser) -> BulletMLHorizontalASTNode:
    var horizontal = BulletMLHorizontalASTNode.new()
    match xml.get_named_attribute_value("type"):
        "absolute":
            horizontal.type = BulletMLHorizontalASTNode.Type.ABSOLUTE
        "relative":
            horizontal.type = BulletMLHorizontalASTNode.Type.RELATIVE
        "sequence":
            horizontal.type = BulletMLHorizontalASTNode.Type.SEQUENCE
    xml.read() # go to text node
    horizontal.value = xml.get_node_data()
    skip_to_closing("horizontal", xml)
    return horizontal

static func create_vertical(xml : XMLParser) -> BulletMLVerticalASTNode:
    var vertical = BulletMLVerticalASTNode.new()
    match xml.get_named_attribute_value("type"):
        "absolute":
            vertical.type = BulletMLVerticalASTNode.Type.ABSOLUTE
        "relative":
            vertical.type = BulletMLVerticalASTNode.Type.RELATIVE
        "sequence":
            vertical.type = BulletMLVerticalASTNode.Type.SEQUENCE
    xml.read() # go to text node
    vertical.value = xml.get_node_data()
    skip_to_closing("vertical", xml)
    return vertical

static func create_term(xml : XMLParser) -> BulletMLTermASTNode:
    var term = BulletMLTermASTNode.new()
    xml.read() # advance to text node
    term.value = xml.get_node_data()
    skip_to_closing("term", xml)
    return term

# Check whether the current XML element is a closing tag for the given name.
static func is_closing(name : String, xml : XMLParser) -> bool:
    return xml.get_node_name() == name and xml.get_node_type() == XMLParser.NODE_ELEMENT_END

# Skips the parser position to the next closing tag for the given name.
# Used at the end of some element parsing functions.
static func skip_to_closing(name : String, xml : XMLParser) -> void:
    while not is_closing(name, xml) and not (xml.is_empty() and xml.get_node_name() == name):
        if xml.read():
            break
