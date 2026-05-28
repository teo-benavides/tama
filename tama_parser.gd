class_name TamaParser

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
	var program: TamaAst.ProgramNode
	var errors:  Array[Diagnostic]

	func _init(p_program: TamaAst.ProgramNode, p_errors: Array) -> void:
		program = p_program
		errors  = p_errors

	func ok() -> bool:
		return errors.is_empty()

# ---------------------------------------------------------------------------
# Parser state
# ---------------------------------------------------------------------------
var _tokens: Array[TamaToken]
var _pos:    int
var _errors: Array[Diagnostic]

var _defined_fires:   Dictionary
var _defined_acts:    Dictionary
var _defined_bullets: Dictionary

var _fire_refs:   Array   # {name, line, col}
var _act_refs:    Array   # {name, line, col}
var _bullet_refs: Array   # {name, line, col}

# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------
func parse(tokens: Array) -> ParseResult:
	_tokens          = tokens
	_pos             = 0
	_errors          = []
	_defined_fires   = {}
	_defined_acts    = {}
	_defined_bullets = {}
	_fire_refs   = []
	_act_refs    = []
	_bullet_refs = []

	var program := TamaAst.ProgramNode.new()

	_pre_scan_definitions()
	_skip_newlines()
	while _peek_type() != TamaToken.EOF:
		var tok := _peek()
		match tok.type:
			TamaToken.KW_MAIN:
				program.main = _parse_main()
			TamaToken.KW_FIRE:
				var node := _parse_fire_def()
				if node:
					program.fires.append(node)
			TamaToken.KW_ACT:
				var node := _parse_act_def()
				if node:
					program.acts.append(node)
			TamaToken.KW_BULLET:
				var node := _parse_bullet_def()
				if node:
					program.bullets.append(node)
			TamaToken.ERROR:
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
func _peek() -> TamaToken:
	return _tokens[_pos]

func _peek_type() -> String:
	return _tokens[_pos].type

func _peek_type_at(offset: int) -> String:
	var idx := _pos + offset
	if idx >= _tokens.size():
		return TamaToken.EOF
	return _tokens[idx].type

func _consume(expected_type: String = "") -> TamaToken:
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
	while _peek_type() == TamaToken.NEWLINE:
		_pos += 1

func _tok_display(tok: TamaToken) -> String:
	return tok.value if tok.value != "" else tok.type

func _error_at(tok: TamaToken, message: String) -> void:
	var length := maxi(tok.value.length(), 1)
	_errors.append(Diagnostic.new(tok.line, tok.col, length, message))

# ---------------------------------------------------------------------------
# Qualifier helpers
# ---------------------------------------------------------------------------

# Optional dir qualifier: aim | abs | rel | seq — default AIM
func _peek_dir_qualifier() -> TamaAst.DirType:
	var t := _peek()
	match t.type:
		TamaToken.KW_AIM: _pos += 1; return TamaAst.DirType.AIM
		TamaToken.KW_ABS: _pos += 1; return TamaAst.DirType.ABS
		TamaToken.KW_REL: _pos += 1; return TamaAst.DirType.REL
		TamaToken.KW_SEQ: _pos += 1; return TamaAst.DirType.SEQ
	return TamaAst.DirType.AIM

# Optional value qualifier: abs | rel | seq — caller supplies default
func _peek_value_qualifier(default_val: TamaAst.ValueType) -> TamaAst.ValueType:
	var t := _peek()
	match t.type:
		TamaToken.KW_ABS: _pos += 1; return TamaAst.ValueType.ABS
		TamaToken.KW_REL: _pos += 1; return TamaAst.ValueType.REL
		TamaToken.KW_SEQ: _pos += 1; return TamaAst.ValueType.SEQ
	return default_val

# ---------------------------------------------------------------------------
# Pre-scan: collect all top-level definition names before parsing bodies.
# This lets arg parsing distinguish first-class refs from float expressions.
# ---------------------------------------------------------------------------
func _pre_scan_definitions() -> void:
	var depth := 0
	for i in _tokens.size():
		match _tokens[i].type:
			TamaToken.INDENT:  depth += 1
			TamaToken.DEDENT:  depth -= 1
			TamaToken.KW_FIRE:
				if depth == 0 and i + 1 < _tokens.size() and _tokens[i + 1].type == TamaToken.WORD:
					_defined_fires[_tokens[i + 1].value] = true
			TamaToken.KW_ACT:
				if depth == 0 and i + 1 < _tokens.size() and _tokens[i + 1].type == TamaToken.WORD:
					_defined_acts[_tokens[i + 1].value] = true
			TamaToken.KW_BULLET:
				if depth == 0 and i + 1 < _tokens.size() and _tokens[i + 1].type == TamaToken.WORD:
					_defined_bullets[_tokens[i + 1].value] = true
# ---------------------------------------------------------------------------
# Expression collecting
# ---------------------------------------------------------------------------

func _collect_to_eol() -> String:
	var parts := []
	while _peek_type() != TamaToken.NEWLINE \
	  and _peek_type() != TamaToken.EOF \
	  and _peek_type() != TamaToken.DEDENT:
		parts.append(_peek().value)
		_pos += 1
	return " ".join(parts)

func _collect_to_rparen(open_tok: TamaToken) -> String:
	var parts := []
	var depth := 0
	while true:
		var t := _peek()
		if t.type == TamaToken.EOF:
			_error_at(open_tok, "Unclosed parenthesis")
			break
		if t.type == TamaToken.LPAREN:
			depth += 1
		elif t.type == TamaToken.RPAREN:
			if depth == 0:
				break
			depth -= 1
		elif t.type == TamaToken.COMMA and depth == 0:
			break
		parts.append(t.value)
		_pos += 1
	return " ".join(parts)

# Parses (arg, arg, ...) — each arg is a String expr, RefCallArg, or inline AST node.
func _parse_call_args(caller_tok: TamaToken) -> Array:
	var args: Array = []
	if _peek_type() != TamaToken.LPAREN:
		return args
	var lp := _consume(TamaToken.LPAREN)
	# Skip newline/indent that follow an opening '(' on its own line.
	_skip_newlines()
	_try_consume(TamaToken.INDENT)
	while _peek_type() != TamaToken.RPAREN and _peek_type() != TamaToken.EOF:
		args.append(_parse_single_arg(lp))
		_try_consume(TamaToken.COMMA)
		_skip_newlines()
	if _peek_type() != TamaToken.RPAREN:
		_error_at(caller_tok, "Unclosed argument list")
	else:
		_consume(TamaToken.RPAREN)
	return args

# Parses one argument: inline block node, first-class named ref, or raw expression string.
func _parse_single_arg(open_tok: TamaToken):  # -> String | TamaAst.RefCallArg | TamaAst.ASTNode
	var tok := _peek()
	# Inline block as argument: keyword followed immediately by NEWLINE.
	match tok.type:
		TamaToken.KW_BULLET:
			if _peek_type_at(1) == TamaToken.NEWLINE:
				return _parse_inline_bullet()
		TamaToken.KW_ACT:
			if _peek_type_at(1) == TamaToken.NEWLINE:
				return _parse_inline_act()
		TamaToken.KW_FIRE:
			if _peek_type_at(1) == TamaToken.NEWLINE:
				return _parse_inline_fire()
		TamaToken.KW_EMITTER:
			if _peek_type_at(1) == TamaToken.NEWLINE:
				return _parse_inline_emitter()
	# Qualifier keywords (aim/abs/rel/seq) passed as type values.
	if tok.type == TamaToken.KW_AIM or tok.type == TamaToken.KW_ABS \
	or tok.type == TamaToken.KW_REL or tok.type == TamaToken.KW_SEQ:
		var next := _peek_type_at(1)
		if next == TamaToken.COMMA or next == TamaToken.RPAREN:
			_pos += 1
			return tok.value  # "aim", "abs", "rel", or "seq"
	# Named first-class ref (known definition, optionally pre-applied).
	if tok.type == TamaToken.WORD:
		var is_def := _defined_fires.has(tok.value) or _defined_acts.has(tok.value) \
		           or _defined_bullets.has(tok.value)
		var next := _peek_type_at(1)
		var at_terminator := next == TamaToken.COMMA or next == TamaToken.RPAREN
		var at_call       := next == TamaToken.LPAREN
		if is_def and (at_terminator or at_call):
			var ref_tok := _consume(TamaToken.WORD)
			var ref := TamaAst.RefCallArg.new(ref_tok.value, ref_tok.line, ref_tok.col)
			if at_call:
				ref.args = _parse_call_args(ref_tok)
			return ref
	return _collect_to_rparen(open_tok)

# ---------------------------------------------------------------------------
# Shared sub-parsers
# ---------------------------------------------------------------------------

func _parse_identifier() -> String:
	var tok := _peek()
	if tok.type == TamaToken.WORD:
		_pos += 1
		return tok.value
	_error_at(tok, "Expected identifier, got \"%s\"" % _tok_display(tok))
	return ""

# ([name, name, ...]) — returns Array[String]
func _parse_param_list() -> Array[String]:
	var params: Array[String] = []
	if _peek_type() != TamaToken.LPAREN:
		return params
	var lp := _consume(TamaToken.LPAREN)
	while _peek_type() != TamaToken.RPAREN and _peek_type() != TamaToken.EOF:
		if _peek_type() == TamaToken.WORD:
			params.append(_peek().value)
			_pos += 1
		elif _peek_type() == TamaToken.COMMA:
			_pos += 1
		else:
			_error_at(_peek(), "Expected parameter name")
			break
	if _peek_type() != TamaToken.RPAREN:
		_error_at(lp, "Unclosed parameter list")
	else:
		_consume(TamaToken.RPAREN)
	return params

func _parse_block(parse_statement: Callable) -> Array[TamaAst.ASTNode]:
	var nodes: Array[TamaAst.ASTNode] = []
	if _peek_type() != TamaToken.INDENT:
		_error_at(_peek(), "Expected indented block")
		return nodes
	_consume(TamaToken.INDENT)
	_skip_newlines()
	while _peek_type() != TamaToken.DEDENT and _peek_type() != TamaToken.EOF:
		var node = parse_statement.call()
		if node:
			nodes.append(node)
		_skip_newlines()
	if _peek_type() == TamaToken.DEDENT:
		_consume(TamaToken.DEDENT)
	else:
		_error_at(_peek(), "Unclosed block (missing dedent)")
	return nodes

# ---------------------------------------------------------------------------
# Shared statement parsers
# ---------------------------------------------------------------------------

func _parse_dir() -> TamaAst.DirNode:
	var tok := _consume(TamaToken.KW_DIR)
	var qualifier := TamaAst.DirType.AIM
	var qualifier_var := ""
	if _peek_type() == TamaToken.WORD and _peek_type_at(1) != TamaToken.NEWLINE:
		qualifier_var = _peek().value
		_pos += 1
	else:
		qualifier = _peek_dir_qualifier()
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after dir")
	_consume(TamaToken.NEWLINE)
	var node := TamaAst.DirNode.new(qualifier, expr, tok.line, tok.col)
	node.dir_type_var = qualifier_var
	return node

func _parse_speed() -> TamaAst.SpeedNode:
	var tok := _consume(TamaToken.KW_SPEED)
	var qualifier := TamaAst.ValueType.ABS
	var qualifier_var := ""
	if _peek_type() == TamaToken.WORD and _peek_type_at(1) != TamaToken.NEWLINE:
		qualifier_var = _peek().value
		_pos += 1
	else:
		qualifier = _peek_value_qualifier(TamaAst.ValueType.ABS)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after speed")
	_consume(TamaToken.NEWLINE)
	var node := TamaAst.SpeedNode.new(qualifier, expr, tok.line, tok.col)
	node.speed_type_var = qualifier_var
	return node

func _parse_wait() -> TamaAst.WaitNode:
	var tok := _consume(TamaToken.KW_WAIT)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after wait")
	_consume(TamaToken.NEWLINE)
	return TamaAst.WaitNode.new(expr, tok.line, tok.col)

func _parse_waitf() -> TamaAst.WaitFramesNode:
	var tok := _consume(TamaToken.KW_WAITF)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after waitf")
	_consume(TamaToken.NEWLINE)
	return TamaAst.WaitFramesNode.new(expr, tok.line, tok.col)

func _parse_vanish() -> TamaAst.VanishNode:
	var tok := _consume(TamaToken.KW_VANISH)
	_consume(TamaToken.NEWLINE)
	return TamaAst.VanishNode.new(tok.line, tok.col)

func _parse_over() -> TamaAst.OverNode:
	var tok := _consume(TamaToken.KW_OVER)
	var expr := _collect_to_eol()
	if expr.strip_edges().is_empty():
		_error_at(_peek(), "Expected expression after over")
	_consume(TamaToken.NEWLINE)
	return TamaAst.OverNode.new(expr, tok.line, tok.col)

# Returns OffsetNode (block form) or OffsetInlineNode (inline form).
func _parse_offset() -> TamaAst.ASTNode:
	var tok := _consume(TamaToken.KW_OFFSET)
	if _peek_type() == TamaToken.NEWLINE:
		_consume(TamaToken.NEWLINE)
		var node := TamaAst.OffsetNode.new(tok.line, tok.col)
		_parse_block(func():
			var t := _peek()
			if t.type == TamaToken.KW_X or t.type == TamaToken.KW_Y:
				var axis := t.value
				_pos += 1
				var qualifier := TamaAst.ValueType.REL
				var qualifier_var := ""
				if _peek_type() == TamaToken.WORD and _peek_type_at(1) != TamaToken.NEWLINE:
					qualifier_var = _peek().value; _pos += 1
				else:
					qualifier = _peek_value_qualifier(TamaAst.ValueType.REL)
				var expr := _collect_to_eol()
				_consume(TamaToken.NEWLINE)
				var axis_node := TamaAst.OffsetAxisNode.new(axis, qualifier, expr, t.line, t.col)
				axis_node.axis_type_var = qualifier_var
				if axis == "x":
					node.x = axis_node
				else:
					node.y = axis_node
			else:
				_error_at(t, "Expected 'x' or 'y' in offset block, got '%s'" % _tok_display(t))
				_pos += 1
				_try_consume(TamaToken.NEWLINE)
				return null
		)
		return node
	else:
		# Inline form: offset EXPR
		var expr := _collect_to_eol()
		if expr.strip_edges().is_empty():
			_error_at(_peek(), "Expected expression after offset")
		_consume(TamaToken.NEWLINE)
		return TamaAst.OffsetInlineNode.new(expr, tok.line, tok.col)

func _parse_chdir() -> TamaAst.ChdirNode:
	var tok := _consume(TamaToken.KW_CHDIR)
	var node := TamaAst.ChdirNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	_parse_block(func():
		match _peek_type():
			TamaToken.KW_DIR:
				node.dir = _parse_dir()
			TamaToken.KW_OVER:
				node.over = _parse_over()
			_:
				_error_at(_peek(), "Unexpected token in chdir block: '%s'" % _tok_display(_peek()))
				_pos += 1
				_try_consume(TamaToken.NEWLINE)
				return null
	)
	if not node.dir:
		_error_at(tok, "chdir requires a dir statement")
	if not node.over:
		_error_at(tok, "chdir requires an over statement")
	return node

func _parse_chspd() -> TamaAst.ChspdNode:
	var tok := _consume(TamaToken.KW_CHSPD)
	var node := TamaAst.ChspdNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	_parse_block(func():
		match _peek_type():
			TamaToken.KW_SPEED:
				node.speed = _parse_speed()
			TamaToken.KW_OVER:
				node.over = _parse_over()
			_:
				_error_at(_peek(), "Unexpected token in chspd block: '%s'" % _tok_display(_peek()))
				_pos += 1
				_try_consume(TamaToken.NEWLINE)
				return null
	)
	if not node.speed:
		_error_at(tok, "chspd requires a speed statement")
	if not node.over:
		_error_at(tok, "chspd requires an over statement")
	return node

func _parse_accel() -> TamaAst.AccelNode:
	var tok := _consume(TamaToken.KW_ACCEL)
	var node := TamaAst.AccelNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	_parse_block(func():
		var t := _peek()
		match t.type:
			TamaToken.KW_X, TamaToken.KW_Y:
				var axis := t.value
				_pos += 1
				var qualifier := TamaAst.ValueType.ABS
				var qualifier_var := ""
				if _peek_type() == TamaToken.WORD and _peek_type_at(1) != TamaToken.NEWLINE:
					qualifier_var = _peek().value; _pos += 1
				else:
					qualifier = _peek_value_qualifier(TamaAst.ValueType.ABS)
				var expr := _collect_to_eol()
				_consume(TamaToken.NEWLINE)
				var axis_node := TamaAst.OffsetAxisNode.new(axis, qualifier, expr, t.line, t.col)
				axis_node.axis_type_var = qualifier_var
				if axis == "x":
					node.x = axis_node
				else:
					node.y = axis_node
			TamaToken.KW_OVER:
				node.over = _parse_over()
			_:
				_error_at(t, "Unexpected token in accel block: '%s'" % _tok_display(t))
				_pos += 1
				_try_consume(TamaToken.NEWLINE)
				return null
	)
	if not node.over:
		_error_at(tok, "accel requires an over statement")
	if not node.x and not node.y:
		_error_at(tok, "accel requires at least one of x or y")
	return node

func _parse_repeat() -> TamaAst.RepeatNode:
	var tok := _consume(TamaToken.KW_REPEAT)
	var count := ""
	var index_var := ""
	if _peek_type() != TamaToken.NEWLINE:
		# Find where this line ends
		var end := _pos
		while end < _tokens.size() \
		  and _tokens[end].type != TamaToken.NEWLINE \
		  and _tokens[end].type != TamaToken.EOF:
			end += 1
		# If the last token is a plain WORD and there are tokens before it,
		# treat it as the index variable; otherwise collect everything as count.
		if end - 1 > _pos and _tokens[end - 1].type == TamaToken.WORD:
			index_var = _tokens[end - 1].value
			var parts: Array[String] = []
			while _pos < end - 1:
				parts.append(_tokens[_pos].value)
				_pos += 1
			count = " ".join(parts)
			_pos += 1  # consume the index WORD
		else:
			count = _collect_to_eol()
	_consume(TamaToken.NEWLINE)
	var node := TamaAst.RepeatNode.new(count, tok.line, tok.col)
	node.index_var = index_var
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_inline_act() -> TamaAst.InlineActNode:
	var tok := _consume(TamaToken.KW_ACT)
	var node := TamaAst.InlineActNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_inline_emitter() -> TamaAst.InlineActNode:
	var tok := _consume(TamaToken.KW_EMITTER)
	var node := TamaAst.InlineActNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_inline_fire() -> TamaAst.InlineFireNode:
	var tok := _consume(TamaToken.KW_FIRE)
	var node := TamaAst.InlineFireNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	_parse_fire_block(node, tok)
	return node

func _parse_fire_call() -> TamaAst.FireCallNode:
	_consume(TamaToken.KW_FIRE)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	var args := _parse_call_args(name_tok)
	_fire_refs.append({ "name": name, "line": name_tok.line, "col": name_tok.col })
	_consume(TamaToken.NEWLINE)
	return TamaAst.FireCallNode.new(name, args, name_tok.line, name_tok.col)

func _parse_act_call() -> TamaAst.ActCallNode:
	_consume(TamaToken.KW_ACT)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	var args := _parse_call_args(name_tok)
	_act_refs.append({ "name": name, "line": name_tok.line, "col": name_tok.col })
	_consume(TamaToken.NEWLINE)
	return TamaAst.ActCallNode.new(name, args, name_tok.line, name_tok.col)

func _parse_async_act():  # -> TamaAst.ActCallNode | TamaAst.InlineActNode
	_consume(TamaToken.KW_ASYNC)
	var tok := _peek()
	if tok.type != TamaToken.KW_ACT:
		_error_at(tok, "Expected 'act' after 'async'")
		return null
	var node: TamaAst.ASTNode
	if _peek_type_at(1) == TamaToken.NEWLINE:
		node = _parse_inline_act()
	else:
		node = _parse_act_call()
	if node is TamaAst.InlineActNode:
		(node as TamaAst.InlineActNode).is_async = true
	elif node is TamaAst.ActCallNode:
		(node as TamaAst.ActCallNode).is_async = true
	return node

func _parse_bullet_call() -> TamaAst.BulletCallNode:
	_consume(TamaToken.KW_BULLET)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	var args := _parse_call_args(name_tok)
	_bullet_refs.append({ "name": name, "line": name_tok.line, "col": name_tok.col })
	_consume(TamaToken.NEWLINE)
	return TamaAst.BulletCallNode.new(name, args, name_tok.line, name_tok.col)

func _parse_inline_bullet() -> TamaAst.InlineBulletNode:
	var tok := _consume(TamaToken.KW_BULLET)
	var node := TamaAst.InlineBulletNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	_parse_block(func():
		var t := _peek()
		match t.type:
			TamaToken.KW_TYPE:
				_consume(TamaToken.KW_TYPE)
				if _peek_type() == TamaToken.WORD:
					node.bullet_type = _peek().value
					_pos += 1
				else:
					_error_at(_peek(), "Expected bullet type name after 'type'")
				_consume(TamaToken.NEWLINE)
			TamaToken.KW_EMITTER:
				if _peek_type_at(1) == TamaToken.NEWLINE:
					node.emitter_act = _parse_inline_emitter()
				else:
					_consume(TamaToken.KW_EMITTER)
					var emt_tok := _peek()
					var emt_name := _parse_identifier()
					if emt_name.is_empty():
						_error_at(emt_tok, "Expected act name after 'emt'")
					else:
						var args: Array = []
						if _peek_type() == TamaToken.LPAREN:
							args = _parse_call_args(emt_tok)
						node.emitter_act = TamaAst.ActCallNode.new(emt_name, args, emt_tok.line, emt_tok.col)
						_act_refs.append({ "name": emt_name, "line": emt_tok.line, "col": emt_tok.col })
					_consume(TamaToken.NEWLINE)
			TamaToken.KW_ACT:
				if _peek_type_at(1) == TamaToken.NEWLINE:
					node.act = _parse_inline_act()
				else:
					node.act = _parse_act_call()
			_:
				_error_at(t, "Unexpected token in bullet block: '%s'" % _tok_display(t))
				_pos += 1
				_try_consume(TamaToken.NEWLINE)
		return null
	)
	return node

# ---------------------------------------------------------------------------
# Action block statement dispatcher
# ---------------------------------------------------------------------------
func _parse_action_statement():   # -> TamaAst.ASTNode
	var tok := _peek()
	match tok.type:
		TamaToken.KW_DIR:    return _parse_dir()
		TamaToken.KW_SPEED:  return _parse_speed()
		TamaToken.KW_WAIT:   return _parse_wait()
		TamaToken.KW_WAITF:  return _parse_waitf()
		TamaToken.KW_VANISH: return _parse_vanish()
		TamaToken.KW_OFFSET: return _parse_offset()
		TamaToken.KW_CHDIR:  return _parse_chdir()
		TamaToken.KW_CHSPD:  return _parse_chspd()
		TamaToken.KW_ACCEL:  return _parse_accel()
		TamaToken.KW_REPEAT: return _parse_repeat()
		TamaToken.KW_ACT:
			# act NEWLINE ... = inline act;  act IDENT ... = act call
			if _peek_type_at(1) == TamaToken.NEWLINE:
				return _parse_inline_act()
			else:
				return _parse_act_call()
		TamaToken.KW_ASYNC:
			return _parse_async_act()
		TamaToken.KW_FIRE:
			# fire NEWLINE ... = inline fire;  fire IDENT ... = fire call
			if _peek_type_at(1) == TamaToken.NEWLINE:
				return _parse_inline_fire()
			else:
				return _parse_fire_call()
		_:
			_error_at(tok, "Unexpected token in action block: '%s'" % _tok_display(tok))
			_pos += 1
			_try_consume(TamaToken.NEWLINE)
			return null

# ---------------------------------------------------------------------------
# Fire block parser — fills dir/speed/offset/bullet directly onto node
# ---------------------------------------------------------------------------
func _parse_fire_block(node, open_tok: TamaToken) -> void:
	if _peek_type() != TamaToken.INDENT:
		_error_at(_peek(), "Expected indented block")
		return
	_consume(TamaToken.INDENT)
	_skip_newlines()
	while _peek_type() != TamaToken.DEDENT and _peek_type() != TamaToken.EOF:
		var tok := _peek()
		match tok.type:
			TamaToken.KW_DIR:    node.dir    = _parse_dir()
			TamaToken.KW_SPEED:  node.speed  = _parse_speed()
			TamaToken.KW_OFFSET: node.offset = _parse_offset()
			TamaToken.KW_BULLET:
				if _peek_type_at(1) == TamaToken.NEWLINE:
					node.bullet = _parse_inline_bullet()
				else:
					node.bullet = _parse_bullet_call()
			_:
				_error_at(tok, "Unexpected token in fire block: \"%s\"" % _tok_display(tok))
				_pos += 1
				_try_consume(TamaToken.NEWLINE)
		_skip_newlines()
	if _peek_type() == TamaToken.DEDENT:
		_consume(TamaToken.DEDENT)
	else:
		_error_at(_peek(), "Unclosed block (missing dedent)")

# ---------------------------------------------------------------------------
# Top-level definition parsers
# ---------------------------------------------------------------------------

func _parse_main() -> TamaAst.MainNode:
	var tok := _consume(TamaToken.KW_MAIN)
	var node := TamaAst.MainNode.new(tok.line, tok.col)
	_consume(TamaToken.NEWLINE)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_fire_def() -> TamaAst.FireDefNode:
	_consume(TamaToken.KW_FIRE)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	_defined_fires[name] = true
	var params := _parse_param_list()
	_consume(TamaToken.NEWLINE)
	var node := TamaAst.FireDefNode.new(name, params, name_tok.line, name_tok.col)
	_parse_fire_block(node, name_tok)
	return node

func _parse_act_def() -> TamaAst.ActDefNode:
	_consume(TamaToken.KW_ACT)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	_defined_acts[name] = true
	var params := _parse_param_list()
	_consume(TamaToken.NEWLINE)
	var node := TamaAst.ActDefNode.new(name, params, name_tok.line, name_tok.col)
	node.body = _parse_block(_parse_action_statement)
	return node

func _parse_bullet_def() -> TamaAst.BulletDefNode:
	_consume(TamaToken.KW_BULLET)
	var name_tok := _peek()
	var name := _parse_identifier()
	if name.is_empty():
		return null
	_defined_bullets[name] = true
	var params := _parse_param_list()
	_consume(TamaToken.NEWLINE)
	var node := TamaAst.BulletDefNode.new(name, params, name_tok.line, name_tok.col)
	_parse_block(func():
		var tok := _peek()
		match tok.type:
			TamaToken.KW_TYPE:
				_consume(TamaToken.KW_TYPE)
				if _peek_type() == TamaToken.WORD:
					node.bullet_type = _peek().value
					_pos += 1
				else:
					_error_at(_peek(), "Expected bullet type name after 'type'")
				_consume(TamaToken.NEWLINE)
			TamaToken.KW_EMITTER:
				if _peek_type_at(1) == TamaToken.NEWLINE:
					node.emitter_act = _parse_inline_emitter()
				else:
					_consume(TamaToken.KW_EMITTER)
					var emt_tok := _peek()
					var emt_name := _parse_identifier()
					if emt_name.is_empty():
						_error_at(emt_tok, "Expected act name after 'emt'")
					else:
						var args: Array = []
						if _peek_type() == TamaToken.LPAREN:
							args = _parse_call_args(emt_tok)
						node.emitter_act = TamaAst.ActCallNode.new(emt_name, args, emt_tok.line, emt_tok.col)
						_act_refs.append({ "name": emt_name, "line": emt_tok.line, "col": emt_tok.col })
					_consume(TamaToken.NEWLINE)
			TamaToken.KW_ACT:
				if _peek_type_at(1) == TamaToken.NEWLINE:
					node.act = _parse_inline_act()
				else:
					node.act = _parse_act_call()
			_:
				_error_at(tok, "Unexpected token in bullet block: '%s'" % _tok_display(tok))
				_pos += 1
				_try_consume(TamaToken.NEWLINE)
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
