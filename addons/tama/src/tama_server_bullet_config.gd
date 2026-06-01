## Configuration resource for a server-based bullet type.
## Assign instances to [member TamaBulletRegistry.server_configs] and call
## [method TamaManager.register_server_bullet] to enable low-overhead rendering
## and collision via [RenderingServer] and [PhysicsServer2D] for that bullet type.
@icon("res://addons/tama/icons/tama.svg")
extends Resource
class_name TamaServerBulletConfig

## Texture drawn for each bullet.
@export var texture: Texture2D

## Draw rect in local canvas-item space. Center it at the origin (e.g. Rect2(-8,-8,16,16)).
@export var rect: Rect2 = Rect2(-8.0, -8.0, 16.0, 16.0)

## Multiplies the draw rect's position and size at spawn time. Use (2,2) to double the
## rendered size without changing the base rect. Does not affect the collision shape.
@export var texture_scale: Vector2 = Vector2.ONE

## Radius of the circular collision area.
@export var shape_radius: float = 6.0

## Physics layer the bullet area lives on.
@export_flags_2d_physics var collision_layer: int = 1

## Physics layers the bullet area monitors for overlapping bodies.
@export_flags_2d_physics var collision_mask: int = 2

## When true the canvas item rotates to follow the bullet's travel direction.
@export var rotates: bool = true

## Number of bullet slots pre-allocated for this type.
## Set before the first bullet of this type is fired; cannot be changed at runtime.
@export var pool_size: int = 1000
