class_name TamaInterpreter
extends Node

# ---------------------------------------------------------------------------
# Signal payload — carries everything the spawning system needs
# ---------------------------------------------------------------------------
# Represents a first-class act/fire/bullet/emitter value held in a scope variable.
# name is the definition identifier; bound_args are pre-applied (float or TamaRef).
class TamaRef:
	var name:       String
	var bound_args: Array

	func _init(p_name: String = "", p_args: Array = []) -> void:
		name       = p_name
		bound_args = p_args

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
	var bullet_type:           String
	var bullet_emitter_act:    TamaAst.ASTNode    # ActCallNode or InlineActNode, null if absent
	var bullet_act:            TamaAst.ASTNode    # InlineActNode or ActCallNode, null if none
	var bullet_params:         Array[String] = [] # param names from the bullet def
	var bullet_args:           Array        = [] # evaluated args matching bullet_params (float or String)

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

	elif node is TamaAst.WaitFramesNode:
		var frames := int(_eval((node as TamaAst.WaitFramesNode).expr, scope))
		for _i in frames:
			await get_tree().physics_frame

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
			d.dir_type  = _get_dir_type(cn.dir, scope)
			d.dir_value = _eval(cn.dir.expr, scope)
			d.over      = _eval(cn.over.expr, scope)
			changed_direction.emit(d)

	elif node is TamaAst.ChspdNode:
		var cn := node as TamaAst.ChspdNode
		if not cn.speed or not cn.over:
			push_error("TamaInterpreter: chspd requires both speed and over (L%d)" % node.line)
		else:
			var d := ChspdData.new()
			d.speed_type  = _get_speed_type(cn.speed, scope)
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
				d.x_type = _get_axis_type(cn.x, scope)
				d.x      = _eval(cn.x.expr, scope)
			if cn.y:
				d.has_y  = true
				d.y_type = _get_axis_type(cn.y, scope)
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
	if scope.has(node.name) and scope[node.name] is TamaAst.InlineFireNode:
		_exec_fire_node(scope[node.name] as TamaAst.InlineFireNode, scope)
		return
	var ref_name := node.name
	var pre_bound: Array = []
	if scope.has(node.name) and scope[node.name] is TamaRef:
		var r: TamaRef = scope[node.name]
		ref_name  = r.name
		pre_bound = r.bound_args
	var fire_def := _find_fire(ref_name)
	if not fire_def:
		push_error("TamaInterpreter: unknown fire '%s'" % ref_name)
		return
	var extra := node.args.map(func(a): return _eval_arg(a, scope))
	_exec_fire_node(fire_def, _bind_args_from_values(fire_def.params, pre_bound + extra, scope))

func _exec_act_call(node: TamaAst.ActCallNode, scope: Dictionary) -> void:
	if scope.has(node.name) and scope[node.name] is TamaAst.InlineActNode:
		await _exec_action_body((scope[node.name] as TamaAst.InlineActNode).body, scope)
		return
	var ref_name := node.name
	var pre_bound: Array = []
	if scope.has(node.name) and scope[node.name] is TamaRef:
		var r: TamaRef = scope[node.name]
		ref_name  = r.name
		pre_bound = r.bound_args
	var act_def := _find_act(ref_name)
	if not act_def:
		push_error("TamaInterpreter: unknown act '%s'" % ref_name)
		return
	var extra := node.args.map(func(a): return _eval_arg(a, scope))
	await _exec_action_body(act_def.body, _bind_args_from_values(act_def.params, pre_bound + extra, scope))

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
		data.dir_type  = _get_dir_type(node.dir, scope)
		data.dir_value = _eval(node.dir.expr, scope)

	# Speed (default: ABS 0)
	if node.speed:
		data.speed_type  = _get_speed_type(node.speed, scope)
		data.speed_value = _eval(node.speed.expr, scope)

	# Offset
	if node.offset is TamaAst.OffsetInlineNode:
		data.offset_mode  = TamaAst.OffsetMode.INLINE
		data.offset_value = _eval((node.offset as TamaAst.OffsetInlineNode).expr, scope)
	elif node.offset is TamaAst.OffsetNode:
		data.offset_mode = TamaAst.OffsetMode.BLOCK
		var on := node.offset as TamaAst.OffsetNode
		if on.x:
			data.offset_x_type = _get_axis_type(on.x, scope)
			data.offset_x      = _eval(on.x.expr, scope)
		if on.y:
			data.offset_y_type = _get_axis_type(on.y, scope)
			data.offset_y      = _eval(on.y.expr, scope)

	# Bullet
	if node.bullet is TamaAst.InlineBulletNode:
		var ib := node.bullet as TamaAst.InlineBulletNode
		data.bullet_type        = ib.bullet_type
		data.bullet_emitter_act = ib.emitter_act
		data.bullet_act         = ib.act
	else:
		var bcn      := node.bullet as TamaAst.BulletCallNode
		var bul_name := bcn.name
		var pre_bound: Array = []
		if scope.has(bul_name) and scope[bul_name] is TamaAst.InlineBulletNode:
			var ib := scope[bul_name] as TamaAst.InlineBulletNode
			data.bullet_type        = ib.bullet_type
			data.bullet_emitter_act = ib.emitter_act
			data.bullet_act         = ib.act
		else:
			if scope.has(bul_name) and scope[bul_name] is TamaRef:
				var r: TamaRef = scope[bul_name]
				pre_bound = r.bound_args
				bul_name  = r.name
			var bullet_def := _find_bullet(bul_name)
			if not bullet_def:
				push_error("TamaInterpreter: unknown bullet '%s'" % bul_name)
				return
			var extra: Array = []
			for arg in bcn.args:
				extra.append(_eval_arg(arg, scope))
			var all_bullet_args := pre_bound + extra
			data.bullet_type        = bullet_def.bullet_type
			data.bullet_emitter_act = bullet_def.emitter_act
			data.bullet_act         = bullet_def.act
			data.bullet_params      = bullet_def.params.duplicate()
			for val in all_bullet_args:
				data.bullet_args.append(val)

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

func _get_dir_type(node: TamaAst.DirNode, scope: Dictionary) -> TamaAst.DirType:
	if node.dir_type_var.is_empty():
		return node.dir_type
	var val = _eval_arg(node.dir_type_var, scope)
	if val is String:
		match val:
			"aim": return TamaAst.DirType.AIM
			"abs": return TamaAst.DirType.ABS
			"rel": return TamaAst.DirType.REL
			"seq": return TamaAst.DirType.SEQ
	push_error("TamaInterpreter: invalid dir type '%s'" % str(val))
	return TamaAst.DirType.AIM

func _get_axis_type(node: TamaAst.OffsetAxisNode, scope: Dictionary) -> TamaAst.ValueType:
	if node.axis_type_var.is_empty():
		return node.axis_type
	var val = _eval_arg(node.axis_type_var, scope)
	if val is String:
		match val:
			"abs": return TamaAst.ValueType.ABS
			"rel": return TamaAst.ValueType.REL
			"seq": return TamaAst.ValueType.SEQ
	push_error("TamaInterpreter: invalid axis type '%s'" % str(val))
	return TamaAst.ValueType.REL

func _get_speed_type(node: TamaAst.SpeedNode, scope: Dictionary) -> TamaAst.ValueType:
	if node.speed_type_var.is_empty():
		return node.speed_type
	var val = _eval_arg(node.speed_type_var, scope)
	if val is String:
		match val:
			"abs": return TamaAst.ValueType.ABS
			"rel": return TamaAst.ValueType.REL
			"seq": return TamaAst.ValueType.SEQ
	push_error("TamaInterpreter: invalid speed type '%s'" % str(val))
	return TamaAst.ValueType.ABS

# Returns the canonical definition name, resolving through a TamaRef in scope if present.
func _resolve_ref_name(name: String, scope: Dictionary) -> String:
	if scope.has(name) and scope[name] is TamaRef:
		return (scope[name] as TamaRef).name
	return name

# Resolves a RefCallArg at runtime: evaluates its sub-args and builds a TamaRef.
func _resolve_ref_arg(ref_arg: TamaAst.RefCallArg, scope: Dictionary):
	var bound: Array = []
	for sub_arg in ref_arg.args:
		bound.append(_eval_arg(sub_arg, scope))
	if scope.has(ref_arg.name) and scope[ref_arg.name] is TamaRef:
		var v: TamaRef = scope[ref_arg.name]
		var merged := TamaRef.new()
		merged.name       = v.name
		merged.bound_args = v.bound_args + bound
		return merged
	var ref := TamaRef.new()
	ref.name       = ref_arg.name
	ref.bound_args = bound
	return ref

# Evaluates a single argument: inline AST nodes pass through as-is, RefCallArg becomes a
# TamaRef, a plain String identifier holding a non-numeric scope value passes through,
# and everything else is evaluated as a float.
func _eval_arg(arg, scope: Dictionary):
	if arg is TamaAst.InlineBulletNode or arg is TamaAst.InlineActNode or arg is TamaAst.InlineFireNode:
		return arg
	if arg is TamaAst.RefCallArg:
		return _resolve_ref_arg(arg as TamaAst.RefCallArg, scope)
	var expr: String = arg as String
	var stripped := expr.strip_edges()
	if stripped.is_valid_identifier() and scope.has(stripped):
		var val = scope[stripped]
		if not (val is float or val is int):
			return val
	# Qualifier keywords passed as string values — never evaluate them as expressions.
	match stripped:
		"aim", "abs", "rel", "seq": return stripped
	return _eval(expr, scope)

func _eval_arg_as_float(arg, scope: Dictionary) -> float:
	var val = _eval_arg(arg, scope)
	return float(val) if (val is float or val is int) else 0.0

# Binds already-evaluated values to param names, merging into a copy of outer_scope.
func _bind_args_from_values(params: Array[String], values: Array, outer_scope: Dictionary) -> Dictionary:
	var new_scope := outer_scope.duplicate()
	for i in mini(params.size(), values.size()):
		new_scope[params[i]] = values[i]
	return new_scope

# ---------------------------------------------------------------------------
# Expression evaluation
# ---------------------------------------------------------------------------

func _eval(expr: String, scope: Dictionary) -> float:
	expr = expr.strip_edges()
	if expr.is_empty():
		return 0.0
	var expression := Expression.new()
	# Filter scope to only float-compatible values so Expression doesn't choke on TamaRefs.
	var float_keys:   Array[String] = []
	var float_values: Array         = []
	for key in scope.keys():
		var val = scope[key]
		if val is float or val is int:
			float_keys.append(key)
			float_values.append(val)
	var names := PackedStringArray(float_keys)
	if expression.parse(expr, names) != OK:
		push_error("TamaScript expression parse error '%s': %s" % [expr, expression.get_error_text()])
		return 0.0
	var result = expression.execute(float_values, context)
	if expression.has_execute_failed():
		push_error("TamaScript expression execute error '%s': %s" % [expr, expression.get_error_text()])
		return 0.0
	return float(result)
