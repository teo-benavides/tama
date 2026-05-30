extends Node

const _Ast = preload("res://addons/tama/src/tama_ast.gd")

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
	var dir_type:      _Ast.DirType    = _Ast.DirType.AIM
	var dir_value:     float

	var speed_type:    _Ast.ValueType  = _Ast.ValueType.ABS
	var speed_value:   float

	var offset_mode:   _Ast.OffsetMode = _Ast.OffsetMode.NONE
	var offset_value:  float                              # inline form only
	var offset_x_type: _Ast.ValueType  = _Ast.ValueType.REL  # block form only
	var offset_x:      float
	var offset_y_type: _Ast.ValueType  = _Ast.ValueType.REL
	var offset_y:      float

	var has_pos:    bool = false
	var pos_x_set:  bool = false
	var pos_x_type: _Ast.ValueType = _Ast.ValueType.ABS
	var pos_x:      float = 0.0
	var pos_y_set:  bool = false
	var pos_y_type: _Ast.ValueType = _Ast.ValueType.ABS
	var pos_y:      float = 0.0

	# Bullet properties
	var bullet_type:           String
	var bullet_emitter_act:    _Ast.ASTNode    # ActCallNode or InlineActNode, null if absent
	var bullet_act:            _Ast.ASTNode    # InlineActNode or ActCallNode, null if none
	var bullet_params:         Array[String] = [] # param names from the bullet def
	var bullet_args:           Array        = [] # evaluated args matching bullet_params (float or String)

	# The program that fired this bullet — needed so the bullet's act can resolve
	# named fires/acts/bullets from the same script even without a spawner file.
	var source_program: _Ast.ProgramNode

class ChdirData:
	var dir_type:  _Ast.DirType
	var dir_value: float
	var over:      float

class ChspdData:
	var speed_type:  _Ast.ValueType
	var speed_value: float
	var over:        float

class ChposData:
	var has_x:  bool = false
	var x_type: _Ast.ValueType = _Ast.ValueType.ABS
	var x:      float = 0.0
	var has_y:  bool = false
	var y_type: _Ast.ValueType = _Ast.ValueType.ABS
	var y:      float = 0.0
	var over:   float = 0.0

class AccelData:
	var has_x:  bool
	var x_type: _Ast.ValueType
	var x:      float
	var has_y:  bool
	var y_type: _Ast.ValueType
	var y:      float
	var over:   float

# ---------------------------------------------------------------------------
# Signals
# ---------------------------------------------------------------------------

signal bullet_fired(data: BulletFireData)
signal vanished
signal changed_direction(data: ChdirData)
signal changed_speed(data: ChspdData)
signal changed_position(data: ChposData)
signal accelerated(data: AccelData)
signal _all_async_done

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------

var _program: _Ast.ProgramNode
var _running: bool = false
var _async_count: int = 0
var context: TamaContext = TamaContext.new()

var _fires_map:   Dictionary = {}
var _acts_map:    Dictionary = {}
var _bullets_map: Dictionary = {}

# Reusable arrays for scope filtering — avoids allocating new Arrays every _eval call.
var _fk: Array[String] = []
var _fv: Array         = []

# Shared signal payload objects — safe to reuse because signal handlers run synchronously
# before emit() returns, so the next bullet's emit never overlaps with the previous handler.
static var _chpos_buf := ChposData.new()
static var _chdir_buf := ChdirData.new()
static var _chspd_buf := ChspdData.new()
static var _accel_buf := AccelData.new()

# ---------------------------------------------------------------------------
# Entry points
# ---------------------------------------------------------------------------

# Run the program from main. The interpreter must be in the scene tree
# (added as a child) so that get_tree() works for wait.
func _build_lookup_tables() -> void:
	for f: _Ast.FireDefNode   in _program.fires:   _fires_map[f.name]   = f
	for a: _Ast.ActDefNode    in _program.acts:    _acts_map[a.name]    = a
	for b: _Ast.BulletDefNode in _program.bullets: _bullets_map[b.name] = b

func start(program: _Ast.ProgramNode, initial_scope: Dictionary = {}) -> void:
	_program = program
	if not _program.main:
		push_error("TamaInterpreter: program has no main block")
		return
	_build_lookup_tables()
	_running = true
	await _exec_action_body(_program.main.body, initial_scope)
	if _async_count > 0:
		await _all_async_done
	_running = false

# Run a bullet's act, called by the spawning system after bullet_fired.
# program: the same program that fired the bullet (needed for fire/act lookups)
# scope:   {param_name: value} built from BulletFireData.bullet_params/args
func start_act(
	program: _Ast.ProgramNode,
	act:     _Ast.ASTNode,
	scope:   Dictionary = {}
) -> void:
	_program = program
	_build_lookup_tables()
	_running = true
	if act is _Ast.InlineActNode:
		await _exec_action_body((act as _Ast.InlineActNode).body, scope)
	elif act is _Ast.ActCallNode:
		await _exec_act_call(act as _Ast.ActCallNode, scope)
	_running = false

func stop() -> void:
	_running = false

# ---------------------------------------------------------------------------
# Action body execution
# ---------------------------------------------------------------------------

func _exec_action_body(body: Array, scope: Dictionary) -> void:
	for node: _Ast.ASTNode in body:
		if not _running: return
		# R3: dispatch synchronous nodes directly — no coroutine overhead
		if node is _Ast.FireCallNode:
			_exec_fire_call(node as _Ast.FireCallNode, scope)
		elif node is _Ast.InlineFireNode:
			_exec_fire_node(node, scope)
		elif node is _Ast.VanishNode:
			_running = false; vanished.emit()
		elif node is _Ast.ChdirNode:
			var cn := node as _Ast.ChdirNode
			if cn.dir and cn.over:
				var ks := _prep_scope(scope)
				_chdir_buf.dir_type  = _get_dir_type(cn.dir, scope)
				_chdir_buf.dir_value = _eval_f(cn.dir.expr, ks)
				_chdir_buf.over      = _eval_f(cn.over.expr, ks)
				changed_direction.emit(_chdir_buf)
		elif node is _Ast.ChspdNode:
			var cn := node as _Ast.ChspdNode
			if cn.speed and cn.over:
				var ks := _prep_scope(scope)
				_chspd_buf.speed_type  = _get_speed_type(cn.speed, scope)
				_chspd_buf.speed_value = _eval_f(cn.speed.expr, ks)
				_chspd_buf.over        = _eval_f(cn.over.expr, ks)
				changed_speed.emit(_chspd_buf)
		elif node is _Ast.ChposNode:
			var cn := node as _Ast.ChposNode
			var ks := _prep_scope(scope)
			_chpos_buf.has_x = false; _chpos_buf.has_y = false; _chpos_buf.over = 0.0
			if cn.x:
				_chpos_buf.has_x  = true
				_chpos_buf.x_type = _get_axis_type(cn.x, scope)
				_chpos_buf.x      = _eval_f(cn.x.expr, ks)
			if cn.y:
				_chpos_buf.has_y  = true
				_chpos_buf.y_type = _get_axis_type(cn.y, scope)
				_chpos_buf.y      = _eval_f(cn.y.expr, ks)
			if cn.over: _chpos_buf.over = _eval_f(cn.over.expr, ks)
			changed_position.emit(_chpos_buf)
		elif node is _Ast.AccelNode:
			var cn := node as _Ast.AccelNode
			if cn.over and (cn.x or cn.y):
				var ks := _prep_scope(scope)
				_accel_buf.has_x = false; _accel_buf.has_y = false
				if cn.x:
					_accel_buf.has_x  = true
					_accel_buf.x_type = _get_axis_type(cn.x, scope)
					_accel_buf.x      = _eval_f(cn.x.expr, ks)
				if cn.y:
					_accel_buf.has_y  = true
					_accel_buf.y_type = _get_axis_type(cn.y, scope)
					_accel_buf.y      = _eval_f(cn.y.expr, ks)
				_accel_buf.over = _eval_f(cn.over.expr, ks)
				accelerated.emit(_accel_buf)
		else:
			await _exec_action_stmt(node, scope)

func _exec_action_stmt(node: _Ast.ASTNode, scope: Dictionary) -> void:
	if node is _Ast.WaitNode:
		var secs := _eval((node as _Ast.WaitNode).expr, scope)
		if secs > 0.0:
			await get_tree().create_timer(secs, false, true).timeout

	elif node is _Ast.WaitFramesNode:
		var frames := int(_eval((node as _Ast.WaitFramesNode).expr, scope))
		for _i in frames:
			await get_tree().physics_frame

	elif node is _Ast.VanishNode:
		_running = false
		vanished.emit()

	elif node is _Ast.RepeatNode:
		await _exec_repeat(node as _Ast.RepeatNode, scope)

	elif node is _Ast.FireCallNode:
		_exec_fire_call(node as _Ast.FireCallNode, scope)

	elif node is _Ast.InlineFireNode:
		_exec_fire_node(node, scope)

	elif node is _Ast.ActCallNode:
		var acn := node as _Ast.ActCallNode
		if acn.is_async:
			_run_async(func(): await _exec_act_call(acn, scope))
		else:
			await _exec_act_call(acn, scope)

	elif node is _Ast.InlineActNode:
		var ian := node as _Ast.InlineActNode
		if ian.is_async:
			_run_async(func(): await _exec_action_body(ian.body, scope))
		else:
			await _exec_action_body(ian.body, scope)

	elif node is _Ast.ChdirNode:
		var cn := node as _Ast.ChdirNode
		if not cn.dir or not cn.over:
			push_error("TamaInterpreter: chdir requires both dir and over (L%d)" % node.line)
		else:
			var d := ChdirData.new()
			d.dir_type  = _get_dir_type(cn.dir, scope)
			d.dir_value = _eval(cn.dir.expr, scope)
			d.over      = _eval(cn.over.expr, scope)
			changed_direction.emit(d)

	elif node is _Ast.ChspdNode:
		var cn := node as _Ast.ChspdNode
		if not cn.speed or not cn.over:
			push_error("TamaInterpreter: chspd requires both speed and over (L%d)" % node.line)
		else:
			var d := ChspdData.new()
			d.speed_type  = _get_speed_type(cn.speed, scope)
			d.speed_value = _eval(cn.speed.expr, scope)
			d.over        = _eval(cn.over.expr, scope)
			changed_speed.emit(d)

	elif node is _Ast.AccelNode:
		var cn := node as _Ast.AccelNode
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

	elif node is _Ast.ChposNode:
		var cn := node as _Ast.ChposNode
		var d := ChposData.new()
		if cn.x:
			d.has_x  = true
			d.x_type = _get_axis_type(cn.x, scope)
			d.x      = _eval(cn.x.expr, scope)
		if cn.y:
			d.has_y  = true
			d.y_type = _get_axis_type(cn.y, scope)
			d.y      = _eval(cn.y.expr, scope)
		d.over = _eval(cn.over.expr, scope) if cn.over else 0.0
		changed_position.emit(d)

func _exec_repeat(node: _Ast.RepeatNode, scope: Dictionary) -> void:
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

func _exec_fire_call(node: _Ast.FireCallNode, scope: Dictionary) -> void:
	if scope.has(node.name) and scope[node.name] is _Ast.InlineFireNode:
		_exec_fire_node(scope[node.name] as _Ast.InlineFireNode, scope)
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

func _exec_act_call(node: _Ast.ActCallNode, scope: Dictionary) -> void:
	if scope.has(node.name) and scope[node.name] is _Ast.InlineActNode:
		await _exec_action_body((scope[node.name] as _Ast.InlineActNode).body, scope)
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
	if node.offset is _Ast.OffsetInlineNode:
		data.offset_mode  = _Ast.OffsetMode.INLINE
		data.offset_value = _eval((node.offset as _Ast.OffsetInlineNode).expr, scope)
	elif node.offset is _Ast.OffsetNode:
		data.offset_mode = _Ast.OffsetMode.BLOCK
		var on := node.offset as _Ast.OffsetNode
		if on.x:
			data.offset_x_type = _get_axis_type(on.x, scope)
			data.offset_x      = _eval(on.x.expr, scope)
		if on.y:
			data.offset_y_type = _get_axis_type(on.y, scope)
			data.offset_y      = _eval(on.y.expr, scope)

	# Pos
	if node.pos is _Ast.PosNode:
		var pn := node.pos as _Ast.PosNode
		data.has_pos = true
		if pn.x:
			data.pos_x_set  = true
			data.pos_x_type = _get_axis_type(pn.x, scope)
			data.pos_x      = _eval(pn.x.expr, scope)
		if pn.y:
			data.pos_y_set  = true
			data.pos_y_type = _get_axis_type(pn.y, scope)
			data.pos_y      = _eval(pn.y.expr, scope)

	# Bullet (optional — omitting uses the registry default)
	if node.bullet is _Ast.InlineBulletNode:
		var ib := node.bullet as _Ast.InlineBulletNode
		data.bullet_type        = scope.get(ib.bullet_type, ib.bullet_type)
		data.bullet_emitter_act = ib.emitter_act
		data.bullet_act         = ib.act
	elif node.bullet != null:
		var bcn      := node.bullet as _Ast.BulletCallNode
		var bul_name := bcn.name
		var pre_bound: Array = []
		if scope.has(bul_name) and scope[bul_name] is _Ast.InlineBulletNode:
			var ib := scope[bul_name] as _Ast.InlineBulletNode
			data.bullet_type        = scope.get(ib.bullet_type, ib.bullet_type)
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
			data.bullet_type        = scope.get(bullet_def.bullet_type, bullet_def.bullet_type)
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

func _find_fire(name: String)   -> _Ast.FireDefNode:   return _fires_map.get(name)
func _find_act(name: String)    -> _Ast.ActDefNode:    return _acts_map.get(name)
func _find_bullet(name: String) -> _Ast.BulletDefNode: return _bullets_map.get(name)

# ---------------------------------------------------------------------------
# Scope helpers
# ---------------------------------------------------------------------------

func _get_dir_type(node: _Ast.DirNode, scope: Dictionary) -> _Ast.DirType:
	if node.dir_type_var.is_empty():
		return node.dir_type
	var val = _eval_arg(node.dir_type_var, scope)
	if val is String:
		match val:
			"aim": return _Ast.DirType.AIM
			"abs": return _Ast.DirType.ABS
			"rel": return _Ast.DirType.REL
			"seq": return _Ast.DirType.SEQ
	push_error("TamaInterpreter: invalid dir type '%s'" % str(val))
	return _Ast.DirType.AIM

func _get_axis_type(node: _Ast.OffsetAxisNode, scope: Dictionary) -> _Ast.ValueType:
	if node.axis_type_var.is_empty():
		return node.axis_type
	var val = _eval_arg(node.axis_type_var, scope)
	if val is String:
		match val:
			"abs": return _Ast.ValueType.ABS
			"rel": return _Ast.ValueType.REL
			"seq": return _Ast.ValueType.SEQ
	push_error("TamaInterpreter: invalid axis type '%s'" % str(val))
	return _Ast.ValueType.REL

func _get_speed_type(node: _Ast.SpeedNode, scope: Dictionary) -> _Ast.ValueType:
	if node.speed_type_var.is_empty():
		return node.speed_type
	var val = _eval_arg(node.speed_type_var, scope)
	if val is String:
		match val:
			"abs": return _Ast.ValueType.ABS
			"rel": return _Ast.ValueType.REL
			"seq": return _Ast.ValueType.SEQ
	push_error("TamaInterpreter: invalid speed type '%s'" % str(val))
	return _Ast.ValueType.ABS

# Returns the canonical definition name, resolving through a TamaRef in scope if present.
func _resolve_ref_name(name: String, scope: Dictionary) -> String:
	if scope.has(name) and scope[name] is TamaRef:
		return (scope[name] as TamaRef).name
	return name

# Resolves a RefCallArg at runtime: evaluates its sub-args and builds a TamaRef.
func _resolve_ref_arg(ref_arg: _Ast.RefCallArg, scope: Dictionary):
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
	if arg is _Ast.InlineBulletNode or arg is _Ast.InlineActNode or arg is _Ast.InlineFireNode:
		return arg
	if arg is _Ast.RefCallArg:
		return _resolve_ref_arg(arg as _Ast.RefCallArg, scope)
	var expr: String = arg as String
	var stripped := expr.strip_edges()
	if stripped.is_valid_identifier():
		if scope.has(stripped):
			var val = scope[stripped]
			if not (val is float or val is int):
				return val
			# float/int in scope — fall through to _eval to retrieve it
		else:
			# Qualifier keywords passed as string values.
			match stripped:
				"aim", "abs", "rel", "seq": return stripped
			# Bare identifier not in scope — treat as a string name (e.g. bullet type).
			return stripped
	return _eval(expr, scope)

func _eval_arg_as_float(arg, scope: Dictionary) -> float:
	var val = _eval_arg(arg, scope)
	return float(val) if (val is float or val is int) else 0.0

# Binds already-evaluated values to param names, merging into a copy of outer_scope.
func _bind_args_from_values(params: Array[String], values: Array, outer_scope: Dictionary) -> Dictionary:
	if params.is_empty(): return outer_scope
	var new_scope := outer_scope.duplicate()
	for i in mini(params.size(), values.size()):
		new_scope[params[i]] = values[i]
	return new_scope

# ---------------------------------------------------------------------------
# Expression evaluation
# ---------------------------------------------------------------------------

static var _expr_cache: Dictionary = {}

# Filter scope into the reusable _fk / _fv arrays and return the key-signature string.
# Call this once before a batch of _eval_f calls that share the same scope.
func _prep_scope(scope: Dictionary) -> String:
	_fk.clear()
	_fv.clear()
	for key in scope:
		var val = scope[key]
		if val is float or val is int:
			_fk.append(key)
			_fv.append(val)
	return ",".join(_fk)

# Evaluate using pre-filtered _fk / _fv — call _prep_scope first.
func _eval_f(expr: String, keys_sig: String) -> float:
	if expr.is_valid_float(): return float(expr)
	var ki := _fk.find(expr)
	if ki >= 0: return float(_fv[ki])
	var cache_key := expr + "|" + keys_sig
	var expression: Expression
	if _expr_cache.has(cache_key):
		expression = _expr_cache[cache_key]
	else:
		expression = Expression.new()
		if expression.parse(expr, PackedStringArray(_fk)) != OK:
			push_error("TamaScript expression parse error '%s': %s" % [expr, expression.get_error_text()])
			return 0.0
		_expr_cache[cache_key] = expression
	var result = expression.execute(_fv, context)
	if expression.has_execute_failed():
		push_error("TamaScript expression execute error '%s': %s" % [expr, expression.get_error_text()])
		return 0.0
	return float(result)

func _eval(expr: String, scope: Dictionary) -> float:
	expr = expr.strip_edges()
	if expr.is_empty(): return 0.0
	# R1: literal fast-paths — no Expression needed
	if expr.is_valid_float(): return float(expr)
	if scope.has(expr):
		var sv = scope[expr]
		if sv is float or sv is int: return float(sv)
	# Filter scope into reusable arrays (no allocation)
	var keys_sig := _prep_scope(scope)
	return _eval_f(expr, keys_sig)
