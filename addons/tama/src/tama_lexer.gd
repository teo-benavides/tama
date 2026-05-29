const _Token = preload("res://addons/tama/src/tama_token.gd")

# Precompiled regex rules: [RegEx, token_type]
# Order matters — more specific patterns first.
var _rules: Array = []

func _init() -> void:
	_compile_rules()

func _compile_rules() -> void:
	var patterns := [
		["^\\d+\\.\\d+",            _Token.NUMBER],  # float before int
		["^\\d+",                       _Token.NUMBER],
		["^[a-zA-Z_][a-zA-Z0-9_]*",    _Token.WORD],
		["^\\(",                        _Token.LPAREN],
		["^\\)",                        _Token.RPAREN],
		["^(==|!=|<=|>=|&&|\\|\\|)",    _Token.OP],  # two-char operators before single-char
		["^[*/+\\-<>!&|=%]",            _Token.OP],
		["^,",                          _Token.COMMA],
	]
	for entry in patterns:
		var rx := RegEx.new()
		rx.compile(entry[0])
		_rules.append([rx, entry[1]])

# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------
func tokenize(source: String) -> Array[_Token]:
	var tokens: Array[_Token] = []
	# Normalize line endings
	var lines := source.replace("\r\n", "\n").replace("\r", "\n").split("\n")
	var indent_stack := [0]

	for line_idx in lines.size():
		var raw_line: String = lines[line_idx]

		# Skip blank lines and comment-only lines
		var trimmed := raw_line.strip_edges()
		if trimmed.is_empty() or trimmed.begins_with("#"):
			continue

		# --- Measure indent (tabs = 4 spaces) ---
		var indent := 0
		for ch in raw_line:
			if ch == ' ':
				indent += 1
			elif ch == '\t':
				indent += 4
			else:
				break

		# --- Emit INDENT / DEDENT tokens ---
		var current_indent: int = indent_stack.back()
		if indent > current_indent:
			indent_stack.push_back(indent)
			tokens.append(_Token.new(_Token.INDENT, "", line_idx, 0))
		elif indent < current_indent:
			while indent_stack.back() > indent:
				indent_stack.pop_back()
				tokens.append(_Token.new(_Token.DEDENT, "", line_idx, 0))
			if indent_stack.back() != indent:
				tokens.append(_Token.new(
					_Token.ERROR,
					"Indentation mismatch",
					line_idx, 0
				))

		# --- Lex the line content ---
		var pos := indent
		while pos < raw_line.length():
			var ch := raw_line[pos]

			# Skip inline whitespace
			if ch == ' ' or ch == '\t':
				pos += 1
				continue

			# Rest of line is a comment
			if ch == '#':
				break

			var slice := raw_line.substr(pos)
			var matched := false

			for rule in _rules:
				var rx: RegEx = rule[0]
				var tok_type: String = rule[1]
				var result := rx.search(slice)
				if result and result.get_start() == 0:
					var val := result.get_string()
					# Reclassify WORDs that are keywords
					if tok_type == _Token.WORD and _Token.KEYWORDS.has(val):
						tok_type = _Token.KEYWORDS[val]
					tokens.append(_Token.new(tok_type, val, line_idx, pos))
					pos += val.length()
					matched = true
					break

			if not matched:
				tokens.append(_Token.new(_Token.ERROR, ch, line_idx, pos))
				pos += 1

		tokens.append(_Token.new(_Token.NEWLINE, "", line_idx, raw_line.length()))

	# Close any remaining open indent levels
	while indent_stack.size() > 1:
		indent_stack.pop_back()
		tokens.append(_Token.new(_Token.DEDENT, "", lines.size() - 1, 0))

	tokens.append(_Token.new(_Token.EOF, "", lines.size(), 0))
	return tokens
