extends Node2D

enum {
    BARRIER,
    BOWAP,
    WHIP,
    ENEMIES,
    BOSS
}

const SCRIPTS_PATH = "res://addons/bulletml/example/bulletml_scripts/"

@export var code_highlighter: CodeHighlighter

func _ready():
    BulletMLScriptRepository.load_scripts(SCRIPTS_PATH)
    BulletMLSpawnManager.bullet_registry = load("res://addons/bulletml/example/other/bullet_registry.tres")
    BulletMLContext.spawn_parent = get_path()

    $BossEnemy.visible = false

    $UI/Container/OptionButton.add_item("barriers.xml", BARRIER)
    $UI/Container/OptionButton.add_item("bowap.xml", BOWAP)
    $UI/Container/OptionButton.add_item("whip-3x.xml", WHIP)
    $UI/Container/OptionButton.add_item("enemies.xml", ENEMIES)
    $UI/Container/OptionButton.add_item("boss.xml", BOSS)
    
    _configure_syntax_highlighting()
    
    var emitter = create_emitter()
    $UI/Container/Input.text = get_script_file_contents("barriers.xml")
    emitter.script_filename = "barriers.xml"
    emitter.start()

func create_emitter():
    var emitter = BulletMLBulletEmitter.new()
    emitter.initialize_position = false
    emitter.global_position = $EmitterPosition.global_position
    add_child(emitter)
    return emitter

func open_script(filename: String):
    get_tree().call_group("bulletml_bullet_instances", "queue_free")
    var emitter = create_emitter()
    emitter.script_filename = filename
    emitter.start()

func open_temp_script(script: String):
    get_tree().call_group("bulletml_bullet_instances", "queue_free")
    BulletMLScriptRepository.load_temp_script($UI/Container/Input.text)
    var emitter = create_emitter()
    emitter.use_temp_script = true
    emitter.start()

func get_script_file_contents(filename: String):
    var file = FileAccess.open(SCRIPTS_PATH + filename, FileAccess.READ)
    var content = file.get_as_text()
    file.close()
    return content

func _on_input_text_changed():
    $InputTimer.start()

func _on_input_timer_timeout():
    $BossEnemy.visible = false
    $BossEnemy.stop()
    open_temp_script($UI/Container/Input.text)

func _on_option_button_item_selected(index):
    $BossEnemy.visible = false
    $BossEnemy.stop()
    match index:
        BARRIER:
            $UI/Container/Input.text = get_script_file_contents("barriers.xml")
            open_script("barriers.xml")
        BOWAP:
            $UI/Container/Input.text = get_script_file_contents("bowap.xml")
            open_script("bowap.xml")
        WHIP:
            $UI/Container/Input.text = get_script_file_contents("whip-3x.xml")
            open_script("whip-3x.xml")
        ENEMIES:
            $UI/Container/Input.text = get_script_file_contents("enemies.xml")
            open_script("enemies.xml")
        BOSS:
            get_tree().call_group("bulletml_bullet_instances", "queue_free")
            $BossEnemy.visible = true
            $BossEnemy.start()
            $UI/Container/Input.text = get_script_file_contents("boss.xml")

func _configure_syntax_highlighting():
    code_highlighter.add_keyword_color("accel", Color.ORANGE)
    code_highlighter.add_keyword_color("action", Color.ORANGE)
    code_highlighter.add_keyword_color("actionRef", Color.ORANGE)
    code_highlighter.add_keyword_color("bullet", Color.ORANGE)
    code_highlighter.add_keyword_color("bulletRef", Color.ORANGE)
    code_highlighter.add_keyword_color("changeDirection", Color.ORANGE)
    code_highlighter.add_keyword_color("changeSpeed", Color.ORANGE)
    code_highlighter.add_keyword_color("direction", Color.ORANGE)
    code_highlighter.add_keyword_color("fire", Color.ORANGE)
    code_highlighter.add_keyword_color("fireRef", Color.ORANGE)
    code_highlighter.add_keyword_color("horizontal", Color.ORANGE)
    code_highlighter.add_keyword_color("offset", Color.ORANGE)
    code_highlighter.add_keyword_color("param", Color.ORANGE)
    code_highlighter.add_keyword_color("repeat", Color.ORANGE)
    code_highlighter.add_keyword_color("speed", Color.ORANGE)
    code_highlighter.add_keyword_color("term", Color.ORANGE)
    code_highlighter.add_keyword_color("vanish", Color.ORANGE)
    code_highlighter.add_keyword_color("vertical", Color.ORANGE)
    code_highlighter.add_keyword_color("wait", Color.ORANGE)

    code_highlighter.add_keyword_color("type", Color.CRIMSON)
    code_highlighter.add_keyword_color("label", Color.CRIMSON)
    code_highlighter.add_keyword_color("shooter", Color.CRIMSON)

    code_highlighter.add_keyword_color("times", Color.HOT_PINK)
