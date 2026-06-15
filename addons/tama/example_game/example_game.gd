extends Node

const SFX_MAP = {
	"shot": preload("res://addons/tama/shared/sfx/shot.wav"),
}

var emitter: TamaEmitter
var _loading_script := false

func _ready() -> void:
	TamaManager.set_registry(load("res://addons/tama/example_game/example_game_bullet_registry.tres"))
	TamaManager.set_context(ExampleTamaContext.new())
	TamaManager.load_scripts("res://addons/tama/example_game/tamascripts")
	#TamaManager.event_fired.connect(_on_tama_event)
	TamaManager.world_rect = $ReferenceRect.get_rect()
	#$TamaEmitter.start()

#func _on_tama_event(name: String, args: Array) -> void:
	#if name == "sfx" and args.size() > 0:
		#if $SFXPlayer.stream != SFX_MAP[args[0]]:
			#$SFXPlayer.stream = SFX_MAP[args[0]]
		#$SFXPlayer.play()


func _on_game_start_timer_timeout() -> void:
	$TamaEmitter.start()
	pass
