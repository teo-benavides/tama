## Shared runtime context that exposes built-in functions to TamaScript expressions.
##
## A single [TamaContext] instance is created by [TamaManager] and shared across all
## interpreters. Its methods are available as global functions inside any TamaScript
## expression (e.g. [code]wait time() % 2[/code]).
extends RefCounted
class_name TamaContext

## Returns the current physics time in seconds since the game started.
func time() -> float:
	return float(Engine.get_physics_frames()) / float(Engine.physics_ticks_per_second)

## Returns [code]1[/code] with a 1-in-[param number] chance, otherwise returns [code]0[/code].
## Useful for randomising fire patterns (e.g. [code]wait 0.1 + get_1_in(3) * 0.5[/code]).
func get_1_in(number: int) -> int:
	return 1 if randi() % number == 0 else 0
