extends Node

func _ready() -> void:
	var lexer = TamaLexer.new()
	var parser = TamaParser.new()
	
	var file := FileAccess.open("res://tamascripts/enemies.tam", FileAccess.READ)
	var tokens := lexer.tokenize(file.get_as_text())
	var result := parser.parse(tokens)
	if result.ok():
		print(result.program)
	else:
		print(result.errors)
	file.close()
