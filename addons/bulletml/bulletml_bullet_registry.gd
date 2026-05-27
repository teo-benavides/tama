extends Resource
class_name BulletMLBulletRegistry
## Register bullets here and assign this resource to
## [member BulletMLSpawnManager.bullet_registry].

## Key indicates the bullet type,
## value is the bullet scene extending [BulletMLBulletInstance].
@export var entries: Dictionary[String, PackedScene]
