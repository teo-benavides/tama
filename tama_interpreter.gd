class_name TamaInterpreter
extends Node

# ---------------------------------------------------------------------------
# Signal payload — carries everything the spawning system needs
# ---------------------------------------------------------------------------
class BulletFireData:
	# Fire statement properties
	var dir_type:      TamaAst.DirType    = TamaAst.DirType.AIM
	var dir_value:     float

	var speed_type:    TamaAst.ValueType  = TamaAst.ValueType.ABS
	var speed_value:   float

	var offset_mode:   TamaAst.OffsetMode = TamaAst.OffsetMode.NONE
	var offset_value:  float                              # inline form only
	var offset_x_type: TamaAst.ValueType  = TamaAst.ValueType.REL  # block form only
	var offset_x:      float
	var offset_y_type: TamaAst.ValueType  = TamaAst.ValueType.REL
	var offset_y:      float

	# Bullet properties
	var bullet_type:    String
	var bullet_spawner: String
	var bullet_act:     TamaAst.ASTNode    # InlineActNode or ActCallNode, null if none
	var bullet_params:  Array[String] = [] # param names from the bullet def
	var bullet_args:    Array[float]  = [] # evaluated args matching bullet_params

	# The program that fired this bullet — needed so the bullet's act can resolve
	# named fires/acts/bullets from the same script even without a spawner file.
	var source_program: TamaAst.ProgramNode

class ChdirData:
	var dir_type:  TamaAst.DirType
	var dir_value: float
	var over:      float

class ChspdData:
	var speed_type:  TamaAst.ValueType
	var speed_value: float
	var over:        float

class AccelData:
	var has_x:  bool
	var x_type: TamaAst.ValueType
	var x:      float
	var has_y:  bool
	var y_type: TamaAst.ValueType
	var y:      float
	var over:   float

# ---------------------------------------------------------------------------
# Signals
# ---------------------------------------------------------------------------

signal bullet_fired(data: BulletFireData)
signal vanished
signal changed_direction(data: ChdirData)
signal changed_speed(data: ChspdData)
signal accelerated(data: AccelData)
signal _all_async_done

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------

var _program: TamaAst.ProgramNode
var _running: bool = false
var _async_count: int = 0
var context: TamaContext = TamaContext.new()

# ---------------------------------------------------------------------------
# Entry points
# ---------------------------------------------------------------------------

# Run the program from main. The interpreter must be in the scene tree
# (added as a child) so that get_tree() works for wait.
func start(program: TamaAst.ProgramNode) -> void:
	_program = program
	if not _program.main:
		push_error("TamaInterpreter: program has no main block")
		return
	_running = true
	await _exec_action_body(_program.main.body, {})
	if _async_count > 0:
		await _all_async_done
	_running = false

# Run a bullet's act, called by the spawning system after bullet_fired.
# program: the same program that fired the bullet (needed for fire/act lookups)
# scope:   {param_name: value} built from BulletFireData.bullet_params/args
func start_act(
	program: TamaAst.ProgramNode,
	act:     TamaAst.ASTNode,
	scope:   Dictionary = {}
) -> void:
	_program = program
	_running = true
	if act is TamaAst.InlineActNode:
		await _exec_action_body((act as TamaAst.InlineActNode).body, scope)
	elif act is TamaAst.ActCallNode:
		await _exec_act_call(act as TamaAst.ActCallNode, scope)
	_running = false

func stop() -> void:
	_running = false

# ---------------------------------------------------------------------------
# Action body execution
# ---------------------------------------------------------------------------

func _exec_action_body(body: Array, scope: Dictionary) -> void:
	for node: TamaAst.ASTNode in body:
		if not _running:
			return
		await _exec_action_stmt(node, scope)

func _exec_action_stmt(node: TamaAst.ASTNode, scope: Dictionary) -> void:
	if node is TamaAst.WaitNode:
		var secs := _eval((node as TamaAst.WaitNode).expr, scope)
		if secs > 0.0:
			await get_tree().create_timer(secs, false, true).timeout

	elif node is TamaAst.VanishNode:
		_running = false
		vanished.emit()

	elif node is TamaAst.RepeatNode:
		await _exec_repeat(node as TamaAst.RepeatNode, scope)

	elif node is TamaAst.FireCallNode:
		_exec_fire_call(node as TamaAst.FireCallNode, scope)

	elif node is TamaAst.InlineFireNode:
		_exec_fire_node(node, scope)

	elif node is TamaAst.ActCallNode:
		var acn := node as TamaAst.ActCallNode
		if acn.is_async:
			_run_async(func(): await _exec_act_call(acn, scope))
		else:
			await _exec_act_call(acn, scope)

	elif node is TamaAst.InlineActNode:
		var ian := node as TamaAst.InlineActNode
		if ian.is_async:
			_run_async(func(): await _exec_action_body(ian.body, scope))
		else:
			await _exec_action_body(ian.body, scope)

	elif node is TamaAst.ChdirNode:
		var cn := node as TamaAst.ChdirNode
		if not cn.dir or not cn.over:
			push_error("TamaInterpreter: chdir requires both dir and over (L%d)" % node.line)
		else:
			var d := ChdirData.new()
			d.dir_type  = cn.dir.dir_type
			d.dir_value = _eval(cn.dir.expr, scope)
			d.over      = _eval(cn.over.expr, scope)
			changed_direction.emit(d)

	elif node is TamaAst.ChspdNode:
		var cn := node as TamaAst.ChspdNode
		if not cn.speed or not cn.over:
			push_error("TamaInterpreter: chspd requires both speed and over (L%d)" % node.line)
		else:
			var d := ChspdData.new()
			d.speed_type  = cn.speed.speed_type
			d.speed_value = _eval(cn.speed.expr, scope)
			d.over        = _eval(cn.over.expr, scope)
			changed_speed.emit(d)

	elif node is TamaAst.AccelNode:
		var cn := node as TamaAst.AccelNode
		if not cn.over or (not cn.x and not cn.y):
			push_error("TamaInterpreter: accel requires over and at least one of x/y (L%d)" % node.line)
		else:
			var d := AccelData.new()
			if cn.x:
				d.has_x  = true
				d.x_type = cn.x.axis_type
				d.x      = _eval(cn.x.expr, scope)
			if cn.y:
				d.has_y  = true
				d.y_type = cn.y.axis_type
				d.y      = _eval(cn.y.expr, scope)
			d.over = _eval(cn.over.expr, scope)
			accelerated.emit(d)

func _exec_repeat(node: TamaAst.RepeatNode, scope: Dictionary) -> void:
	var count := -1
	if not node.count.strip_edges().is_empty():
		count = roundi(_eval(node.count, scope))
	var i := 0
	while _running and (count < 0 or i < count):
		var iter_scope := scope
		if not node.index_var.is_empty():
			iter_scope = scope.duplicate()
			iter_scope[node.index_var] = float(i + 1)
		await _exec_action_body(node.body, iter_scope)
		i += 1

func _exec_fire_call(node: TamaAst.FireCallNode, scope: Dictionary) -> void:
	var fire_def := _find_fire(node.name)
	if not fire_def:
		push_error("TamaInterpreter: unknown fire '%s'" % node.name)
		return
	_exec_fire_node(fire_def, _bind_args(fire_def.params, node.args, scope))

func _exec_act_call(node: TamaAst.ActCallNode, scope: Dictionary) -> void:
	var act_def := _find_act(node.name)
	if not act_def:
		push_error("TamaInterpreter: unknown act '%s'" % node.name)
		return
	await _exec_action_body(act_def.body, _bind_args(act_def.params, node.args, scope))

func _run_async(fn: Callable) -> void:
	_async_count += 1
	await fn.call()
	_async_count -= 1
	if _async_count == 0:
		_all_async_done.emit()

# Duck-typed: accepts FireDefNode or InlineFireNode — both share dir/speed/offset/bullet.
func _exec_fire_node(node, scope: Dictionary) -> void:
	if not node.bullet:
		push_error("TamaInterpreter: fire block has no bullet")
		return

	var data := BulletFireData.new()

	# Direction (default: AIM 0)
	if node.dir:
		data.dir_type  = node.dir.dir_type
		data.dir_value = _eval(node.dir.expr, scope)

	# Speed (default: ABS 0)
	if node.speed:
		data.speed_type  = node.speed.speed_type
		data.speed_value = _eval(node.speed.expr, scope)

	# Offset
	if node.offset is TamaAst.OffsetInlineNode:
		data.offset_mode  = TamaAst.OffsetMode.INLINE
		data.offset_value = _eval((node.offset as TamaAst.OffsetInlineNode).expr, scope)
	elif node.offset is TamaAst.OffsetNode:
		data.offset_mode = TamaAst.OffsetMode.BLOCK
		var on := node.offset as TamaAst.OffsetNode
		if on.x:
			data.offset_x_type = on.x.axis_type
			data.offset_x      = _eval(on.x.expr, scope)
		if on.y:
			data.offset_y_type = on.y.axis_type
			data.offset_y      = _eval(on.y.expr, scope)

	# Bullet
	if node.bullet is TamaAst.InlineBulletNode:
		var ib := node.bullet as TamaAst.InlineBulletNode
		data.bullet_type    = ib.bullet_type
		data.bullet_spawner = ib.spawner_name
		data.bullet_act     = ib.act
	else:
		var bullet_def := _find_bullet((node.bullet as TamaAst.BulletCallNode).name)
		if not bullet_def:
			push_error("TamaInterpreter: unknown bullet '%s'" % (node.bullet as TamaAst.BulletCallNode).name)
			return
		data.bullet_type    = bullet_def.bullet_type
		data.bullet_spawner = bullet_def.spawner_name
		data.bullet_act     = bullet_def.act
		data.bullet_params  = bullet_def.params.duplicate()
		for arg_expr: String in (node.bullet as TamaAst.BulletCallNode).args:
			data.bullet_args.append(_eval(arg_expr, scope))

	data.source_program = _program
	bullet_fired.emit(data)

# ---------------------------------------------------------------------------
# Definition lookups
# ---------------------------------------------------------------------------

func _find_fire(name: String) -> TamaAst.FireDefNode:
	for f: TamaAst.FireDefNode in _program.fires:
		if f.name == name:
			return f
	return null

func _find_act(name: String) -> TamaAst.ActDefNode:
	for a: TamaAst.ActDefNode in _program.acts:
		if a.name == name:
			return a
	return null

func _find_bullet(name: String) -> TamaAst.BulletDefNode:
	for b: TamaAst.BulletDefNode in _program.bullets:
		if b.name == name:
			return b
	return null

# ---------------------------------------------------------------------------
# Scope helpers
# ---------------------------------------------------------------------------

# Evaluates each arg expression in the caller's scope and binds to param names.
func _bind_args(params: Array[String], arg_exprs: Array[String], scope: Dictionary) -> Dictionary:
	var new_scope := scope.duplicate()
	for i in mini(params.size(), arg_exprs.size()):
		new_scope[params[i]] = _eval(arg_exprs[i], scope)
	return new_scope

# ---------------------------------------------------------------------------
# Expression evaluation
# ---------------------------------------------------------------------------

func _eval(expr: String, scope: Dictionary) -> float:
	expr = expr.strip_edges()
	if expr.is_empty():
		return 0.0
	var expression := Expression.new()
	var names := PackedStringArray(scope.keys())
	if expression.parse(expr, names) != OK:
		push_error("TamaScript expression parse error '%s': %s" % [expr, expression.get_error_text()])
		return 0.0
	var result = expression.execute(Array(scope.values()), context)
	if expression.has_execute_failed():
		push_error("TamaScript expression execute error '%s': %s" % [expr, expression.get_error_text()])
		return 0.0
	return float(result)
