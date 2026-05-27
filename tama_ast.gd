class_name TamaAst

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
	var body:   Array[ASTNode]

	func _init(p_name: String, p_params: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name   = p_name
		params = p_params
		body   = []

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
	var spawner_name: String
	var act:          InlineActNode

	func _init(p_name: String, p_params: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name         = p_name
		params       = p_params
		bullet_type  = ""
		spawner_name = ""

# ---------------------------------------------------------------------------
# Fire body statements
# ---------------------------------------------------------------------------

# dir [aim|abs|rel|seq] <expr>   — default qualifier: aim
class DirNode extends ASTNode:
	var dir_type: String
	var expr:     String

	func _init(p_type: String, p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		dir_type = p_type
		expr     = p_expr

# speed [abs|rel|seq] <expr>   — default qualifier: abs
class SpeedNode extends ASTNode:
	var speed_type: String
	var expr:       String

	func _init(p_type: String, p_expr: String, p_line: int, p_col: int) -> void:
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
	var axis:      String
	var axis_type: String
	var expr:      String

	func _init(p_axis: String, p_type: String, p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		axis      = p_axis
		axis_type = p_type
		expr      = p_expr

# bullet <name>([args...])  — bullet reference inside a fire block
class BulletCallNode extends ASTNode:
	var name: String
	var args: Array[String]

	func _init(p_name: String, p_args: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name = p_name
		args = p_args

# spawner <name>  — inside a bullet def, names the spawner bullet
class FireSpawnerNode extends ASTNode:
	var bullet_name: String

	func _init(p_name: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		bullet_name = p_name

# ---------------------------------------------------------------------------
# Action statements
# ---------------------------------------------------------------------------

# wait <expr>
class WaitNode extends ASTNode:
	var expr: String

	func _init(p_expr: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		expr = p_expr

# vanish
class VanishNode extends ASTNode:
	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)

# repeat [count_expr]  — omitting count means infinite
class RepeatNode extends ASTNode:
	var count: String
	var body:  Array

	func _init(p_count: String, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		count = p_count
		body  = []

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
	var body: Array

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		body = []

# fire (anonymous inline block inside an action block)
class InlineFireNode extends ASTNode:
	var body: Array

	func _init(p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		body = []

# fire <name>([args...])  — named fire call inside an action block
class FireCallNode extends ASTNode:
	var name: String
	var args: Array[String]

	func _init(p_name: String, p_args: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name = p_name
		args = p_args

# act <name>([args...])  — named act call inside an action block
class ActCallNode extends ASTNode:
	var name: String
	var args: Array[String]

	func _init(p_name: String, p_args: Array, p_line: int, p_col: int) -> void:
		super(p_line, p_col)
		name = p_name
		args = p_args
