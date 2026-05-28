class_name TamaBulletRegistry
extends Resource

## Maps bullet type strings to their PackedScenes.
@export var entries: Dictionary = {}

## Fallback scene used when a bullet has no "type" statement or its type isn't in entries.
@export var default_bullet: PackedScene
