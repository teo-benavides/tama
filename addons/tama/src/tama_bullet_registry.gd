## Resource that maps TamaScript bullet type strings to their [PackedScene]s.
##
## You do not need to create this manually. Use [method TamaManager.register_bullet]
## and [method TamaManager.set_default_bullet] to populate it at runtime, or create a
## [code].tres[/code] file and assign it as a [TamaBulletRegistry] resource if you prefer
## asset-based configuration.
@icon("res://addons/tama/icons/tama.svg")
extends Resource
class_name TamaBulletRegistry

## Maps bullet type strings (matching the TamaScript [code]type[/code] statement) to [PackedScene]s.
@export var entries: Dictionary = {}

## Fallback scene used when a bullet omits the [code]type[/code] statement or its type
## string is not found in [member entries].
@export var default_bullet: PackedScene
