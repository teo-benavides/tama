class_name TamaAst

enum DirType   { AIM, ABS, REL, SEQ }
enum ValueType { ABS, REL, SEQ }
enum OffsetMode { NONE, INLINE, BLOCK }

# ---------------------------------------------------------------------------
# Base node — all AST nodes extend this
# ---------------------------------------------------------------------------
class ASTNode:
	var line: int
	var col:  int

	func _init(p_line: int, p_col: int) -> void:
		line = p_line
		col  = p_col

# ---------------------------------------------------------------------------
# Top-level program
# ---------------------------------------------------------------------------

class ProgramNode extends ASTNode:
	var main:    MainNode
	var fires:   Array[FireDefNode]
	var acts:    Array[ActDefNode]
	var bullets: Array[BulletDefNode]

	func _init() -> void:
		super(0, 0)
		fires   = []
		acts    = []
		bullets = []

# ---------------------------------------------------------------------------
# Top-level definitions
# ---------------------------------------------------------------------------

# main
class MainNode extends ASTNode:
	var body: Array[ASTNode]

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		body = []

# fire <name>([params...])
class FireDefNode extends ASTNode:
	var name:   String
	var params: Array[String]
	var dir:    DirNode  # optional
	var speed:  SpeedNode  # optional
	var offset: ASTNode  # optional — OffsetNode or OffsetInlineNode
	var bullet: ASTNode  # mandatory — BulletCallNode or InlineBulletNode

	func _init(p_name: String, p_params: Array[String], p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name   = p_name
		params = p_params

# act <name>([params...])
class ActDefNode extends ASTNode:
	var name:   String
	var params: Array[String]
	var body:   Array[ASTNode]

	func _init(p_name: String, p_params: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name   = p_name
		params = p_params
		body   = []

# bullet <name>([params...])
class BulletDefNode extends ASTNode:
	var name:         String
	var params:       Array[String]
	var bullet_type:  String
	var emitter_act:  ASTNode   # ActCallNode or InlineActNode, null if absent
	var act:          ASTNode   # InlineActNode or ActCallNode

	func _init(p_name: String, p_params: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name        = p_name
		params      = p_params
		bullet_type = ""

# ---------------------------------------------------------------------------
# Fire body statements
# ---------------------------------------------------------------------------

# dir [aim|abs|rel|seq] <expr>   — default qualifier: aim
class DirNode extends ASTNode:
	var dir_type:     DirType
	var dir_type_var: String = ""  # non-empty: resolve qualifier from scope at runtime
	var expr:         String

	func _init(p_type: DirType, p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		dir_type = p_type
		expr     = p_expr

# speed [abs|rel|seq] <expr>   — default qualifier: abs
class SpeedNode extends ASTNode:
	var speed_type:     ValueType
	var speed_type_var: String = ""  # non-empty: resolve qualifier from scope at runtime
	var expr:           String

	func _init(p_type: ValueType, p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		speed_type = p_type
		expr       = p_expr

# offset NEWLINE offset_block  — block form with x/y axes
class OffsetNode extends ASTNode:
	var x: OffsetAxisNode
	var y: OffsetAxisNode

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# offset <expr>  — inline form; offset along the spawner's local up direction
class OffsetInlineNode extends ASTNode:
	var expr: String

	func _init(p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		expr = p_expr

# x|y [abs|rel|seq] <expr>  — axis sub-node used by offset block and accel
class OffsetAxisNode extends ASTNode:
	var axis:          String
	var axis_type:     ValueType
	var axis_type_var: String = ""  # non-empty: resolve qualifier from scope at runtime
	var expr:          String

	func _init(p_axis: String, p_type: ValueType, p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		axis      = p_axis
		axis_type = p_type
		expr      = p_expr

# bullet <name>([args...])  — named bullet reference inside a fire block
class BulletCallNode extends ASTNode:
	var name: String
	var args: Array  # Array[String | RefCallArg]

	func _init(p_name: String, p_args: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name = p_name
		args = p_args

# A first-class definition reference used as an argument value.
# name is the definition identifier; args are the pre-bound sub-args (String | RefCallArg).
class RefCallArg extends ASTNode:
	var name: String
	var args: Array

	func _init(p_name: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name = p_name
		args = []

# bullet NEWLINE <block>  — anonymous inline bullet definition inside a fire block
class InlineBulletNode extends ASTNode:
	var bullet_type: String = ""
	var emitter_act: ASTNode  # ActCallNode or InlineActNode, null if absent
	var act:         ASTNode  # InlineActNode or ActCallNode, may be null

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# ---------------------------------------------------------------------------
# Action statements
# ---------------------------------------------------------------------------

# wait <expr>
class WaitNode extends ASTNode:
	var expr: String

	func _init(p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		expr = p_expr

# waitf <expr>  — wait for a number of physics frames
class WaitFramesNode extends ASTNode:
	var expr: String

	func _init(p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		expr = p_expr

# vanish
class VanishNode extends ASTNode:
	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# repeat [count_expr] [index_var]  — omitting count means infinite; index_var is 1-based
class RepeatNode extends ASTNode:
	var count:     String
	var index_var: String
	var body:      Array

	func _init(p_count: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		count     = p_count
		index_var = ""
		body      = []

# chdir
#   dir:  DirNode | null
#   over: OverNode | null
class ChdirNode extends ASTNode:
	var dir:  DirNode
	var over: OverNode

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# chspd
#   speed: SpeedNode | null
#   over:  OverNode | null
class ChspdNode extends ASTNode:
	var speed: SpeedNode
	var over:  OverNode

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# accel
#   x:    OffsetAxisNode | null
#   y:    OffsetAxisNode | null
#   over: OverNode | null
class AccelNode extends ASTNode:
	var x:    OffsetAxisNode
	var y:    OffsetAxisNode
	var over: OverNode

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# over <expr>  — duration sub-node used by chdir, chspd, and accel
class OverNode extends ASTNode:
	var expr: String

	func _init(p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		expr = p_expr

# act (anonymous inline block)
class InlineActNode extends ASTNode:
	var body:     Array
	var is_async: bool = false

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		body = []

# fire (anonymous inline block inside an action block)
class InlineFireNode extends ASTNode:
	var dir:    DirNode       # optional
	var speed:  SpeedNode     # optional
	var offset: ASTNode       # optional — OffsetNode or OffsetInlineNode
	var bullet: ASTNode  # mandatory — BulletCallNode or InlineBulletNode

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# fire <name>([args...])  — named fire call inside an action block
class FireCallNode extends ASTNode:
	var name: String
	var args: Array  # Array[String | RefCallArg]

	func _init(p_name: String, p_args: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name = p_name
		args = p_args

# act <name>([args...])  — named act call inside an action block
class ActCallNode extends ASTNode:
	var name:     String
	var args:     Array  # Array[String | RefCallArg]
	var is_async: bool = false

	func _init(p_name: String, p_args: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name = p_name
		args = p_args
