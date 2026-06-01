## Lightweight state container for a single server-managed bullet.
## Not a node — owned and driven entirely by [TamaServerBulletPool].
extends RefCounted
class_name TamaServerBullet

var _mm_index:     int     = -1        # slot in this type's multimesh / area shape list
var _multimesh:    RID                  # cached multimesh RID for per-frame update
var _area:         RID                  # cached area RID for per-frame physics update
var _type_data                          # ref to pool's _TypeData, for free_list push on recycle
var _texture_scale: Vector2 = Vector2.ONE  # cached from config at spawn time

var active: bool = false
var _active_index: int = -1  # index in pool's _active array, enables O(1) removal

var position:         Vector2 = Vector2.ZERO
var angle:            float   = 0.0
var speed:            float   = 0.0
var speed_x:          float   = 0.0
var speed_y:          float   = 0.0
var rotates:          bool    = true
var initial_position: Vector2 = Vector2.ZERO
var _last_angle:      float   = 0.0
var _last_speed:      float   = 0.0

# TamaInterpreter reference — set when an act or mvmt expression is present
var _runner = null

# mvmt block: per-axis position expressions evaluated every physics frame
var mvmt_x_set:  bool   = false
var mvmt_x_type: int    = 0
var mvmt_x_expr: String = ""
var mvmt_y_set:  bool   = false
var mvmt_y_type: int    = 0
var mvmt_y_expr: String = ""
var mvmt_scope:  Dictionary = {}

# --- tween state (linear, matches TamaBullet's Tween.TRANS_LINEAR behaviour) ---

var _angle_tweening: bool  = false
var _angle_from:     float = 0.0
var _angle_to:       float = 0.0
var _angle_elapsed:  float = 0.0
var _angle_duration: float = 0.0

var _speed_tweening: bool  = false
var _speed_from:     float = 0.0
var _speed_to:       float = 0.0
var _speed_elapsed:  float = 0.0
var _speed_duration: float = 0.0

var _pos_tweening: bool    = false
var _pos_from:     Vector2 = Vector2.ZERO
var _pos_to:       Vector2 = Vector2.ZERO
var _pos_elapsed:  float   = 0.0
var _pos_duration: float   = 0.0

var _sx_tweening: bool  = false
var _sx_from:     float = 0.0
var _sx_to:       float = 0.0
var _sx_elapsed:  float = 0.0
var _sx_duration: float = 0.0

var _sy_tweening: bool  = false
var _sy_from:     float = 0.0
var _sy_to:       float = 0.0
var _sy_elapsed:  float = 0.0
var _sy_duration: float = 0.0
