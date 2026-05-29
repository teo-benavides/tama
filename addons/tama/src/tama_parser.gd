const _Token = preload("res://addons/tama/src/tama_token.gd")
const _Ast   = preload("res://addons/tama/src/tama_ast.gd")

# ---------------------------------------------------------------------------
# Diagnostic (error) structure
# ---------------------------------------------------------------------------
class Diagnostic:
	var line:    int
	var col:     int
	var length:  int
	var message: String

	func _init(p_line: int, p_col: int, p_length: int, p_message: String) -> void:
		line    = p_line
		col     = p_col
		length  = p_length
		message = p_message

	func _to_string() -> String:
		return "L%d C%d (+%d): %s" % [line, col, length, message]

# ---------------------------------------------------------------------------
# Parse result
# ---------------------------------------------------------------------------
class ParseResult:
	var program: _Ast.ProgramNode
	var errors:  Array[Diagnostic]

	func _init(p_program: _Ast.ProgramNode, p_errors: Array) -> void:
		program = p_program
		errors  = p_errors

	func ok() -> bool:
		return errors.is_empty()

# ---------------------------------------------------------------------------
# Parser state
# ---------------------------------------------------------------------------
var _tokens: Array[_Token]
var _pos:    int
var _errors: Array[Diagnostic]

var _defined_fires:   Dictionary
var _defined_acts:    Dictionary
var _defined_bullets: Dictionary

var _fire_refs:   Array   # {name, line, col}
var _act_refs:    Array   # {name, line, col}
var _bullet_refs: Array   # {name, line, col}

var _resolver:         Callable    # optional; (name: String) -> _Ast.ProgramNode
var _resolved_includes: Dictionary # name -> _Ast.ProgramNode (cached)

# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------
func parse(tokens: Array, resolver: Callable = Callable()) -> ParseResult:
	_tokens            = tokens
	_pos               = 0
	_errors            = []
	_defined_fires     = {}
	_defined_acts      = {}
	_defined_bullets   = {}
	_fire_refs         = []
	_act_refs          = []
	_bullet_refs       = []
	_resolver          = resolver
	_resolved_includes = {}

	var program := _Ast.ProgramNode.new()

	_pre_scan_definitions()
	_skip_newlines()
	while _peek_type() != _Token.EOF:
		var tok := _peek()
		match tok.type:
			_Token.KW_INCLUDE:
				_parse_include(program)
			_Token.KW_EXPORT:
				var node := _parse_export()
				if node:
					program.exports.append(node)
			_Token.KW_MAIN:
				program.main = _parse_main()
			_Token.KW_FIRE:
				var node := _parse_fire_def()
				if node:
					program.fires.append(node)
			_Token.KW_ACT:
				var node := _parse_act_def()
				if node:
					program.acts.append(node)
			_Token.KW_BULLET:
				var node := _parse_bullet_def()
				if node:
					program.bullets.append(node)
			_Token.ERROR:
				_error_at(tok, "Unexpected character: '%s'" % tok.value)
				_pos += 1
			_:
				_error_at(tok, "Unexpected top-level token: '%s'" % _tok_display(tok))
				_pos += 1
		_skip_newlines()

	_resolve_references()
	return ParseResult.new(program, _errors)

# ---------------------------------------------------------------------------
# Cursor helpers
# ---------------------------------------------------------------------------
func _peek() -> _Token:
	return _tokens[_pos]

func _peek_type() -> String:
	return _tokens[_pos].type

func _peek_type_at(offset: int) -> String:
	var idx := _pos + offset
	if idx >= _tokens.size():
		return _Token.EOF
	return _tokens[idx].type

func _consume(expected_type: String = "") -> _Token:
	var tok := _tokens[_pos]
	if expected_type != "" and tok.type != expected_type:
		_error_at(tok, "Expected %s, got '%s'" % [expected_type, _tok_display(tok)])
		return tok
	_pos += 1
	return tok

func _try_consume(type: String) -> bool:
	if _peek_type() == type:
		_pos += 1
		return true
	return false

func _skip_newlines() -> void:
	while _peek_type() == _Token.NEWLINE:
		_pos += 1

func _tok_display(tok: _Token) -> String:
	return tok.value if tok.value != "" else tok.type

func _error_at(tok: _Token, message: String) -> void:
	var length := maxi(tok.value.length(), 1)
	_errors.append(Diagnostic.new(tok.line, tok.col, length, message))

# ---------------------------------------------------------------------------
# Qualifier helpers
# ---------------------------------------------------------------------------

# Optional dir qualifier: aim | abs | rel | seq — default AIM
func _peek_dir_qualifier() -> _Ast.DirType:
	var t := _peek()
	match t.type:
		_Token.KW_AIM: _pos += 1; return _Ast.DirType.AIM
		_Token.KW_ABS: _pos += 1; return _Ast.DirType.ABS
		_Token.KW_REL: _pos += 1; return _Ast.DirType.REL
		_Token.KW_SEQ: _pos += 1; return _Ast.DirType.SEQ
	return _Ast.DirType.AIM

# Optional value qualifier: abs | rel | seq — caller supplies default
func _peek_value_qualifier(default_val: _Ast.ValueType) -> _Ast.ValueType:
	var t := _peek()
	match t.type:
		_Token.KW_ABS: _pos += 1; return _Ast.ValueType.ABS
		_Token.KW_REL: _pos += 1; return _Ast.ValueType.REL
		_Token.KW_SEQ: _pos += 1; return _Ast.ValueType.SEQ
	return default_val

# ---------------------------------------------------------------------------
# Pre-scan: collect all top-level definition names before parsing bodies.
# This lets arg parsing distinguish first-class refs from float expressions.
# ---------------------------------------------------------------------------
func _pre_scan_definitions() -> void:
	var depth := 0
	for i in _tokens.size():
		match _tokens[i].type:
			_Token.INDENT:  depth += 1
			_Token.DEDENT:  depth -= 1
			_Token.KW_INCLUDE:
				if depth == 0 and i + 1 < _tokens.size() and _tokens[i + 1].type == _Token.WORD:
					var inc_name := _tokens[i + 1].value
					if not _resolved_includes.has(inc_name):
						var included: _Ast.ProgramNode = null
						if _resolver.is_valid():
							included = _resolver.call(inc_name)
						_resolved_includes[inc_name] = included
						if included:
							for f in included.fires:   _defined_fires[f.name]   = true
							for a in included.acts:    _defined_acts[a.name]    = true
							for b in included.bullets: _defined_bullets[b.name] = true
			_Token.KW_FIRE:
				if depth == 0 and i + 1 < _tokens.size() and _tokens[i + 1].type == _Token.WORD:
					_defined_fires[_tokens[i + 1].value] = true
			_Token.KW_ACT:
				if depth == 0 and i + 1 < _tokens.size() and _tokens[i + 1].type == _Token.WORD:
					_defined_acts[_tokens[i + 1].value] = true
			_Token.KW_BULLET:
				if depth == 0 and i + 1 < _tokens.size() and _tokens[i + 1].type == _Token.WORD:
					_defined_bullets[_tokens[i + 1].value] = true
# ---------------------------------------------------------------------------
# Expression collecting
# ---------------------------------------------------------------------------

func _collect_to_eol() -> String:
	var parts := []
	while _peek_type() != _Token.NEWLINE \
	  and _peek_type() != _Token.EOF \
	  and _peek_type() != _Token.DEDENT:
		parts.append(_peek().value)
		_pos += 1
	return " ".join(parts)

func _collect_to_rparen(open_tok: _Token) -> String:
	var parts := []
	var depth := 0
	while true:
		var t := _peek()
		if t.type == _Token.EOF:
			_error_at(open_tok, "Unclosed parenthesis")
			break
		if t.type == _Token.LPAREN:
			depth += 1
		elif t.type == _Token.RPAREN:
			if depth == 0:
				break
			depth -= 1
		elif t.type == _Token.COMMA and depth == 0:
			break
		parts.append(t.value)
		_pos += 1
	return " ".join(parts)

# Parses (arg, arg, ...) — each arg is a String expr, RefCallArg, or inline AST node.
func _parse_call_args(caller_tok: _Token) -> Array:
	var args: Array = []
	if _peek_type() != _Token.LPAREN:
		return args
	var lp := _consume(_Token.LPAREN)
	# Skip newline/indent that follow an opening '(' on its own line.
	_skip_newlines()
	_try_consume(_Token.INDENT)
	while _peek_type() != _Token.RPAREN and _peek_type() != _Token.EOF:
		args.append(_parse_single_arg(lp))
		_try_consume(_Token.COMMA)
		_skip_newlines()
	if _peek_type() != _Token.RPAREN:
		_error_at(caller_tok, "Unclosed argument list")
	else:
		_consume(_Token.RPAREN)
	return args

# Parses one argument: inline block node, first-class named ref, or raw expression string.
func _parse_single_arg(open_tok: _Token):  # -> String | _Ast.RefCallArg | _Ast.ASTNode
	var tok := _peek()
	# Inline block as argument: keyword followed immediately by NEWLINE.
	match tok.type:
		_Token.KW_BULLET:
			if _peek_type_at(1) == _Token.NEWLINE:
				return _parse_inline_bullet()
		_Token.KW_ACT:
			if _peek_type_at(1) == _Token.NEWLINE:
				return _parse_inline_act()
		_Token.KW_FIRE:
			if _peek_type_at(1) == _Token.NEWLINE:
				return _parse_inline_fire()
		_Token.KW_EMITTER:
			if _peek_type_at(1) == _Token.NEWLINE:
				return _parse_inline_emitter()
	# Qualifier keywords (aim/abs/rel/seq) passed as type values.
	if tok.type == _Token.KW_AIM or tok.type == _Token.KW_ABS \
	or tok.type == _Token.KW_REL or tok.type == _Token.KW_SEQ:
		var next := _peek_type_at(1)
		if next == _Token.COMMA or next == _Token.RPAREN:
			_pos += 1
			return tok.value  # "aim", "abs", "rel", or "seq"
	# Named first-class ref (known definition, optionally pre-applied).
	if tok.type == _Token.WORD:
		var is_def := _defined_fires.has(tok.value) or _defined_acts.has(tok.value) \
				   or _defined_bullets.has(tok.value)
		var next := _peek_type_at(1)
		var at_terminator := next == _Token.COMMA or next == _Token.RPAREN
		var at_call       := next == _Token.LPAREN
		if is_def and (at_terminator or at_call):
			var ref_tok := _consume(_Token.WORD)
			var ref := _Ast.RefCallArg.new(ref_tok.value, ref_tok.line, ref_tok.col)
			if at_call:
				ref.args = _parse_call_args(ref_tok)
			return ref
	return _collect_to_rparen(open_tok)

# ---------------------------------------------------------------------------
# Shared sub-parsers
# ---------------------------------------------------------------------------

func _parse_identifier() -> String:
	var tok := _peek()
	if tok.type == _Token.WORD:
		_pos += 1
		return tok.value
	_error_at(tok, "Expected identifier, got \"%s\"" % _tok_display(tok))
	return ""

# ([name, name, ...]) — returns Array[String]
func _parse_param_list() -> Array[String]:
	var params: Array[String] = []
	if _peek_type() != _Token.LPAREN:
		return params
	var lp := _consume(_Token.LPAREN)
	while _peek_type() != _Token.RPAREN and _peek_type() != _Token.EOF:
		if _peek_type() == _Token.WORD:
			params.append(_peek().value)
			_pos += 1
		elif _peek_type() == _Token.COMMA:
			_pos += 1
		else:
			_error_at(_peek(), "Expected parameter name")
			break
	if _peek_type() != _Token.RPAREN:
		_error_at(lp, "Unclosed parameter list")
	else:
		_consume(_Token.RPAREN)
	return params

func _parse_block(parse_statement: Callable) -> Array[_Ast.ASTNode]:
	var nodes: Array[_Ast.ASTNode] = []
	if _peek_type() != _Token.INDENT:
		_error_at(_peek(), "Expected indented block")
		return nodes
	_consume(_Token.INDENT)
	_skip_newlines()
	while _peek_type() != _Token.DEDENT and _peek_type() != _Token.EOF:
		var node = parse_statement.call()
		if node:
			nodes.append(node)
		_skip_newlines()
	if _peek_type() == _Token.DEDENT:
		_consume(_Token.DEDENT)
	else:
		_error_at(_peek(), "Unclosed block (missing dedent)")
	return nodes

# ---------------------------------------------------------------------------
# Shared statement parsers
# ---------------------------------------------------------------------------

func _parse_dir() -> _Ast.DirNode:
	var tok := _consume(_Token.KW_DIR)
	var qualifier := _Ast.DirType.AIM
	var qualifier_var := ""
	if _peek_type() == _Token.WORD and _peek_type_at(1) != _Token.NEWLINE:
		qualifier_var = _peek().value
		_pos += 1
	else:
		qualifier = _peek_dir_qualifier()
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after dir")
	_consume(_Token.NEWLINE)
	var node := _Ast.DirNode.new(qualifier, expr, tok.line, tok.col)
	node.dir_type_var = qualifier_var
	return node

func _parse_speed() -> _Ast.SpeedNode:
	var tok := _consume(_Token.KW_SPEED)
	var qualifier := _Ast.ValueType.ABS
	var qualifier_var := ""
	if _peek_type() == _Token.WORD and _peek_type_at(1) != _Token.NEWLINE:
		qualifier_var = _peek().value
		_pos += 1
	else:
		qualifier = _peek_value_qualifier(_Ast.ValueType.ABS)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after speed")
	_consume(_Token.NEWLINE)
	var node := _Ast.SpeedNode.new(qualifier, expr, tok.line, tok.col)
	node.speed_type_var = qualifier_var
	return node

func _parse_wait() -> _Ast.WaitNode:
	var tok := _consume(_Token.KW_WAIT)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after wait")
	_consume(_Token.NEWLINE)
	return _Ast.WaitNode.new(expr, tok.line, tok.col)

func _parse_waitf() -> _Ast.WaitFramesNode:
	var tok := _consume(_Token.KW_WAITF)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after waitf")
	_consume(_Token.NEWLINE)
	return _Ast.WaitFramesNode.new(expr, tok.line, tok.col)

func _parse_vanish() -> _Ast.VanishNode:
	var tok := _consume(_Token.KW_VANISH)
	_consume(_Token.NEWLINE)
	return _Ast.VanishNode.new(tok.line, tok.col)

func _parse_over() -> _Ast.OverNode:
	var tok := _consume(_Token.KW_OVER)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after over")
	_consume(_Token.NEWLINE)
	return _Ast.OverNode.new(expr, tok.line, tok.col)

# Returns OffsetNode (block form) or OffsetInlineNode (inline form).
func _parse_offset() -> _Ast.ASTNode:
	var tok := _consume(_Token.KW_OFFSET)
	if _peek_type() == _Token.NEWLINE:
		_consume(_Token.NEWLINE)
		var node := _Ast.OffsetNode.new(tok.line, tok.col)
		_parse_block(func():
			var t := _peek()
			if t.type == _Token.KW_X or t.type == _Token.KW_Y:
				var axis := t.value
				_pos += 1
				var qualifier := _Ast.ValueType.REL
				var qualifier_var := ""
				if _peek_type() == _Token.WORD and _peek_type_at(1) != _Token.NEWLINE:
					qualifier_var = _peek().value; _pos += 1
				else:
					qualifier = _peek_value_qualifier(_Ast.ValueType.REL)
				var expr := _collect_to_eol()
				_consume(_Token.NEWLINE)
				var axis_node := _Ast.OffsetAxisNode.new(axis, qualifier, expr, t.line, t.col)
				axis_node.axis_type_var = qualifier_var
				if axis == "x":
					node.x = axis_node
				else:
					node.y = axis_node
			else:
				_error_at(t, "Expected 'x' or 'y' in offset block, got '%s'" % _tok_display(t))
				_pos += 1
				_try_consume(_Token.NEWLINE)
				return null
		)
		return node
	else:
		# Inline form: offset EXPR
		var expr := _collect_to_eol()
		if expr.strip_edges().is_empty():
			_error_at(_peek(), "Expected expression after offset")
		_consume(_Token.NEWLINE)
		return _Ast.OffsetInlineNode.new(expr, tok.line, tok.col)

func _parse_chdir() -> _Ast.ChdirNode:
	var tok := _consume(_Token.KW_CHDIR)
	var node := _Ast.ChdirNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	_parse_block(func():
		match _peek_type():
			_Token.KW_DIR:
				node.dir = _parse_dir()
			_Token.KW_OVER:
				node.over = _parse_over()
			_:
				_error_at(_peek(), "Unexpected token in chdir block: '%s'" % _tok_display(_peek()))
				_pos += 1
				_try_consume(_Token.NEWLINE)
				return null
	)
	if not node.dir:
		_error_at(tok, "chdir requires a dir statement")
	if not node.over:
		_error_at(tok, "chdir requires an over statement")
	return node

func _parse_chspd() -> _Ast.ChspdNode:
	var tok := _consume(_Token.KW_CHSPD)
	var node := _Ast.ChspdNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	_parse_block(func():
		match _peek_type():
			_Token.KW_SPEED:
				node.speed = _parse_speed()
			_Token.KW_OVER:
				node.over = _parse_over()
			_:
				_error_at(_peek(), "Unexpected token in chspd block: '%s'" % _tok_display(_peek()))
				_pos += 1
				_try_consume(_Token.NEWLINE)
				return null
	)
	if not node.speed:
		_error_at(tok, "chspd requires a speed statement")
	if not node.over:
		_error_at(tok, "chspd requires an over statement")
	return node

func _parse_accel() -> _Ast.AccelNode:
	var tok := _consume(_Token.KW_ACCEL)
	var node := _Ast.AccelNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	_parse_block(func():
		var t := _peek()
		match t.type:
			_Token.KW_X, _Token.KW_Y:
				var axis := t.value
				_pos += 1
				var qualifier := _Ast.ValueType.ABS
				var qualifier_var := ""
				if _peek_type() == _Token.WORD and _peek_type_at(1) != _Token.NEWLINE:
					qualifier_var = _peek().value; _pos += 1
				else:
					qualifier = _peek_value_qualifier(_Ast.ValueType.ABS)
				var expr := _collect_to_eol()
				_consume(_Token.NEWLINE)
				var axis_node := _Ast.OffsetAxisNode.new(axis, qualifier, expr, t.line, t.col)
				axis_node.axis_type_var = qualifier_var
				if axis == "x":
					node.x = axis_node
				else:
					node.y = axis_node
			_Token.KW_OVER:
				node.over = _parse_over()
			_:
				_error_at(t, "Unexpected token in accel block: '%s'" % _tok_display(t))
				_pos += 1
				_try_consume(_Token.NEWLINE)
				return null
	)
	if not node.over:
		_error_at(tok, "accel requires an over statement")
	if not node.x and not node.y:
		_error_at(tok, "accel requires at least one of x or y")
	return node

func _parse_repeat() -> _Ast.RepeatNode:
	var tok := _consume(_Token.KW_REPEAT)
	var count := ""
	var index_var := ""
	if _peek_type() != _Token.NEWLINE:
		# Find where this line ends
		var end := _pos
		while end < _tokens.size() \
		  and _tokens[end].type != _Token.NEWLINE \
		  and _tokens[end].type != _Token.EOF:
			end += 1
		# If the last token is a plain WORD and there are tokens before it,
		# treat it as the index variable; otherwise collect everything as count.
		if end - 1 > _pos and _tokens[end - 1].type == _Token.WORD:
			index_var = _tokens[end - 1].value
			var parts: Array[String] = []
			while _pos < end - 1:
				parts.append(_tokens[_pos].value)
				_pos += 1
			count = " ".join(parts)
			_pos += 1  # consume the index WORD
		else:
			count = _collect_to_eol()
	_consume(_Token.NEWLINE)
	var node := _Ast.RepeatNode.new(count, tok.line, tok.col)
	node.index_var = index_var
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_inline_act() -> _Ast.InlineActNode:
	var tok := _consume(_Token.KW_ACT)
	var node := _Ast.InlineActNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_inline_emitter() -> _Ast.InlineActNode:
	var tok := _consume(_Token.KW_EMITTER)
	var node := _Ast.InlineActNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_inline_fire() -> _Ast.InlineFireNode:
	var tok := _consume(_Token.KW_FIRE)
	var node := _Ast.InlineFireNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	_parse_fire_block(node, tok)
	return node

func _parse_fire_call() -> _Ast.FireCallNode:
	_consume(_Token.KW_FIRE)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	var args := _parse_call_args(name_tok)
	_fire_refs.append({ "name": name, "line": name_tok.line, "col": name_tok.col })
	_consume(_Token.NEWLINE)
	return _Ast.FireCallNode.new(name, args, name_tok.line, name_tok.col)

func _parse_act_call() -> _Ast.ActCallNode:
	_consume(_Token.KW_ACT)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	var args := _parse_call_args(name_tok)
	_act_refs.append({ "name": name, "line": name_tok.line, "col": name_tok.col })
	_consume(_Token.NEWLINE)
	return _Ast.ActCallNode.new(name, args, name_tok.line, name_tok.col)

func _parse_async_act():  # -> _Ast.ActCallNode | _Ast.InlineActNode
	_consume(_Token.KW_ASYNC)
	var tok := _peek()
	if tok.type != _Token.KW_ACT:
		_error_at(tok, "Expected 'act' after 'async'")
		return null
	var node: _Ast.ASTNode
	if _peek_type_at(1) == _Token.NEWLINE:
		node = _parse_inline_act()
	else:
		node = _parse_act_call()
	if node is _Ast.InlineActNode:
		(node as _Ast.InlineActNode).is_async = true
	elif node is _Ast.ActCallNode:
		(node as _Ast.ActCallNode).is_async = true
	return node

func _parse_bullet_call() -> _Ast.BulletCallNode:
	_consume(_Token.KW_BULLET)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	var args := _parse_call_args(name_tok)
	_bullet_refs.append({ "name": name, "line": name_tok.line, "col": name_tok.col })
	_consume(_Token.NEWLINE)
	return _Ast.BulletCallNode.new(name, args, name_tok.line, name_tok.col)

func _parse_inline_bullet() -> _Ast.InlineBulletNode:
	var tok := _consume(_Token.KW_BULLET)
	var node := _Ast.InlineBulletNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	_parse_block(func():
		var t := _peek()
		match t.type:
			_Token.KW_TYPE:
				_consume(_Token.KW_TYPE)
				if _peek_type() == _Token.WORD:
					node.bullet_type = _peek().value
					_pos += 1
				else:
					_error_at(_peek(), "Expected bullet type name after 'type'")
				_consume(_Token.NEWLINE)
			_Token.KW_EMITTER:
				if _peek_type_at(1) == _Token.NEWLINE:
					node.emitter_act = _parse_inline_emitter()
				else:
					_consume(_Token.KW_EMITTER)
					var emt_tok := _peek()
					var emt_name := _parse_identifier()
					if emt_name.is_empty():
						_error_at(emt_tok, "Expected act name after 'emt'")
					else:
						var args: Array = []
						if _peek_type() == _Token.LPAREN:
							args = _parse_call_args(emt_tok)
						node.emitter_act = _Ast.ActCallNode.new(emt_name, args, emt_tok.line, emt_tok.col)
						_act_refs.append({ "name": emt_name, "line": emt_tok.line, "col": emt_tok.col })
					_consume(_Token.NEWLINE)
			_Token.KW_ACT:
				if _peek_type_at(1) == _Token.NEWLINE:
					node.act = _parse_inline_act()
				else:
					node.act = _parse_act_call()
			_:
				_error_at(t, "Unexpected token in bullet block: '%s'" % _tok_display(t))
				_pos += 1
				_try_consume(_Token.NEWLINE)
		return null
	)
	return node

# ---------------------------------------------------------------------------
# Action block statement dispatcher
# ---------------------------------------------------------------------------
func _parse_action_statement():   # -> _Ast.ASTNode
	var tok := _peek()
	match tok.type:
		_Token.KW_DIR:    return _parse_dir()
		_Token.KW_SPEED:  return _parse_speed()
		_Token.KW_WAIT:   return _parse_wait()
		_Token.KW_WAITF:  return _parse_waitf()
		_Token.KW_VANISH: return _parse_vanish()
		_Token.KW_OFFSET: return _parse_offset()
		_Token.KW_CHDIR:  return _parse_chdir()
		_Token.KW_CHSPD:  return _parse_chspd()
		_Token.KW_ACCEL:  return _parse_accel()
		_Token.KW_REPEAT: return _parse_repeat()
		_Token.KW_ACT:
			# act NEWLINE ... = inline act;  act IDENT ... = act call
			if _peek_type_at(1) == _Token.NEWLINE:
				return _parse_inline_act()
			else:
				return _parse_act_call()
		_Token.KW_ASYNC:
			return _parse_async_act()
		_Token.KW_FIRE:
			# fire NEWLINE ... = inline fire;  fire IDENT ... = fire call
			if _peek_type_at(1) == _Token.NEWLINE:
				return _parse_inline_fire()
			else:
				return _parse_fire_call()
		_:
			_error_at(tok, "Unexpected token in action block: '%s'" % _tok_display(tok))
			_pos += 1
			_try_consume(_Token.NEWLINE)
			return null

# ---------------------------------------------------------------------------
# Fire block parser — fills dir/speed/offset/bullet directly onto node
# ---------------------------------------------------------------------------
func _parse_fire_block(node, _open_tok: _Token) -> void:
	if _peek_type() != _Token.INDENT:
		_error_at(_peek(), "Expected indented block")
		return
	_consume(_Token.INDENT)
	_skip_newlines()
	while _peek_type() != _Token.DEDENT and _peek_type() != _Token.EOF:
		var tok := _peek()
		match tok.type:
			_Token.KW_DIR:    node.dir    = _parse_dir()
			_Token.KW_SPEED:  node.speed  = _parse_speed()
			_Token.KW_OFFSET: node.offset = _parse_offset()
			_Token.KW_BULLET:
				if _peek_type_at(1) == _Token.NEWLINE:
					node.bullet = _parse_inline_bullet()
				else:
					node.bullet = _parse_bullet_call()
			_:
				_error_at(tok, "Unexpected token in fire block: \"%s\"" % _tok_display(tok))
				_pos += 1
				_try_consume(_Token.NEWLINE)
		_skip_newlines()
	if _peek_type() == _Token.DEDENT:
		_consume(_Token.DEDENT)
	else:
		_error_at(_peek(), "Unclosed block (missing dedent)")

# ---------------------------------------------------------------------------
# Top-level definition parsers
# ---------------------------------------------------------------------------

func _parse_include(program: _Ast.ProgramNode) -> void:
	_consume(_Token.KW_INCLUDE)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		_try_consume(_Token.NEWLINE)
		return
	_consume(_Token.NEWLINE)
	var included: _Ast.ProgramNode = _resolved_includes.get(name)
	if not included:
		_error_at(name_tok, "Cannot resolve include '%s'" % name)
		return
	program.fires.append_array(included.fires)
	program.acts.append_array(included.acts)
	program.bullets.append_array(included.bullets)

func _parse_export() -> _Ast.ExportVarNode:
	var tok := _consume(_Token.KW_EXPORT)
	var type_tok := _peek()
	if type_tok.type != _Token.WORD or (type_tok.value != "num" and type_tok.value != "str"):
		_error_at(type_tok, "Expected 'num' or 'str' after 'export'")
		_try_consume(_Token.NEWLINE)
		return null
	var export_type := type_tok.value
	_pos += 1
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		_error_at(name_tok, "Expected identifier after export type")
		return null
	var default_value: Variant = null
	if _peek_type() != _Token.NEWLINE and _peek_type() != _Token.EOF:
		var raw := _collect_to_eol().strip_edges()
		if not raw.is_empty():
			default_value = float(raw) if export_type == "num" else raw
	_consume(_Token.NEWLINE)
	return _Ast.ExportVarNode.new(name, export_type, default_value, tok.line, tok.col)

func _parse_main() -> _Ast.MainNode:
	var tok := _consume(_Token.KW_MAIN)
	var node := _Ast.MainNode.new(tok.line, tok.col)
	_consume(_Token.NEWLINE)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_fire_def() -> _Ast.FireDefNode:
	_consume(_Token.KW_FIRE)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	_defined_fires[name] = true
	var params := _parse_param_list()
	_consume(_Token.NEWLINE)
	var node := _Ast.FireDefNode.new(name, params, name_tok.line, name_tok.col)
	_parse_fire_block(node, name_tok)
	return node

func _parse_act_def() -> _Ast.ActDefNode:
	_consume(_Token.KW_ACT)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	_defined_acts[name] = true
	var params := _parse_param_list()
	_consume(_Token.NEWLINE)
	var node := _Ast.ActDefNode.new(name, params, name_tok.line, name_tok.col)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_bullet_def() -> _Ast.BulletDefNode:
	_consume(_Token.KW_BULLET)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	_defined_bullets[name] = true
	var params := _parse_param_list()
	_consume(_Token.NEWLINE)
	var node := _Ast.BulletDefNode.new(name, params, name_tok.line, name_tok.col)
	_parse_block(func():
		var tok := _peek()
		match tok.type:
			_Token.KW_TYPE:
				_consume(_Token.KW_TYPE)
				if _peek_type() == _Token.WORD:
					node.bullet_type = _peek().value
					_pos += 1
				else:
					_error_at(_peek(), "Expected bullet type name after 'type'")
				_consume(_Token.NEWLINE)
			_Token.KW_EMITTER:
				if _peek_type_at(1) == _Token.NEWLINE:
					node.emitter_act = _parse_inline_emitter()
				else:
					_consume(_Token.KW_EMITTER)
					var emt_tok := _peek()
					var emt_name := _parse_identifier()
					if emt_name.is_empty():
						_error_at(emt_tok, "Expected act name after 'emt'")
					else:
						var args: Array = []
						if _peek_type() == _Token.LPAREN:
							args = _parse_call_args(emt_tok)
						node.emitter_act = _Ast.ActCallNode.new(emt_name, args, emt_tok.line, emt_tok.col)
						_act_refs.append({ "name": emt_name, "line": emt_tok.line, "col": emt_tok.col })
					_consume(_Token.NEWLINE)
			_Token.KW_ACT:
				if _peek_type_at(1) == _Token.NEWLINE:
					node.act = _parse_inline_act()
				else:
					node.act = _parse_act_call()
			_:
				_error_at(tok, "Unexpected token in bullet block: '%s'" % _tok_display(tok))
				_pos += 1
				_try_consume(_Token.NEWLINE)
		return null   # items written directly into node, not collected
	)
	return node

# ---------------------------------------------------------------------------
# Post-parse reference resolution
# ---------------------------------------------------------------------------
func _resolve_references() -> void:
	for ref in _fire_refs:
		var name: String = ref["name"]
		if not _defined_fires.has(name):
			_errors.append(Diagnostic.new(
				ref["line"], ref["col"], name.length(),
				"Unknown fire reference '%s'" % name
			))

	for ref in _act_refs:
		var name: String = ref["name"]
		if not _defined_acts.has(name):
			_errors.append(Diagnostic.new(
				ref["line"], ref["col"], name.length(),
				"Unknown act reference '%s'" % name
			))

	for ref in _bullet_refs:
		var name: String = ref["name"]
		if not _defined_bullets.has(name):
			_errors.append(Diagnostic.new(
				ref["line"], ref["col"], name.length(),
				"Unknown bullet reference '%s'" % name
			))
