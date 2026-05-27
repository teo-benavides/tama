class_name TamaLexer

# Precompiled regex rules: [RegEx, token_type]
# Order matters — more specific patterns first.
var _rules: Array = []

func _init() -> void:
	_compile_rules()

func _compile_rules() -> void:
	var patterns := [
		["^\\$[a-zA-Z_][a-zA-Z0-9_]*", TamaToken.DOLLAR_VAR],  # $rand, $rank, $param
		["^\\$\\d+",                    TamaToken.DOLLAR_VAR],  # $1, $2, ...
		["^\\d+\\.\\d+",               TamaToken.NUMBER],       # float before int
		["^\\d+",                       TamaToken.NUMBER],
		["^[a-zA-Z_][a-zA-Z0-9_]*",    TamaToken.WORD],
		["^\\(",                        TamaToken.LPAREN],
		["^\\)",                        TamaToken.RPAREN],
		["^[*/+\\-%]",                  TamaToken.OP],
		["^,",                          TamaToken.COMMA],
	]
	for entry in patterns:
		var rx := RegEx.new()
		rx.compile(entry[0])
		_rules.append([rx, entry[1]])

# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------
func tokenize(source: String) -> Array[TamaToken]:
	var tokens: Array[TamaToken] = []
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
			tokens.append(TamaToken.new(TamaToken.INDENT, "", line_idx, 0))
		elif indent < current_indent:
			while indent_stack.back() > indent:
				indent_stack.pop_back()
				tokens.append(TamaToken.new(TamaToken.DEDENT, "", line_idx, 0))
			if indent_stack.back() != indent:
				tokens.append(TamaToken.new(
					TamaToken.ERROR,
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
					if tok_type == TamaToken.WORD and TamaToken.KEYWORDS.has(val):
						tok_type = TamaToken.KEYWORDS[val]
					tokens.append(TamaToken.new(tok_type, val, line_idx, pos))
					pos += val.length()
					matched = true
					break

			if not matched:
				tokens.append(TamaToken.new(TamaToken.ERROR, ch, line_idx, pos))
				pos += 1

		tokens.append(TamaToken.new(TamaToken.NEWLINE, "", line_idx, raw_line.length()))

	# Close any remaining open indent levels
	while indent_stack.size() > 1:
		indent_stack.pop_back()
		tokens.append(TamaToken.new(TamaToken.DEDENT, "", lines.size() - 1, 0))

	tokens.append(TamaToken.new(TamaToken.EOF, "", lines.size(), 0))
	return tokens
