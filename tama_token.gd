class_name TamaToken

# --- Token type constants ---
const NUMBER  = "NUMBER"
const WORD    = "WORD"
const LPAREN  = "LPAREN"
const RPAREN     = "RPAREN"
const OP         = "OP"
const COMMA      = "COMMA"
const NEWLINE    = "NEWLINE"
const INDENT     = "INDENT"
const DEDENT     = "DEDENT"
const EOF        = "EOF"
const ERROR      = "ERROR"

# Keywords
const KW_MAIN    = "KW_MAIN"
const KW_FIRE    = "KW_FIRE"
const KW_ACT     = "KW_ACT"
const KW_BULLET  = "KW_BULLET"
const KW_REPEAT  = "KW_REPEAT"
const KW_WAIT    = "KW_WAIT"
const KW_WAITF   = "KW_WAITF"
const KW_VANISH  = "KW_VANISH"
const KW_CHDIR   = "KW_CHDIR"
const KW_CHSPD   = "KW_CHSPD"
const KW_DIR     = "KW_DIR"
const KW_SPEED   = "KW_SPEED"
const KW_OFFSET  = "KW_OFFSET"
const KW_ACCEL   = "KW_ACCEL"
const KW_AIM     = "KW_AIM"
const KW_ABS     = "KW_ABS"
const KW_REL     = "KW_REL"
const KW_SEQ     = "KW_SEQ"
const KW_OVER    = "KW_OVER"
const KW_X       = "KW_X"
const KW_Y       = "KW_Y"
const KW_TYPE    = "KW_TYPE"
const KW_EMITTER = "KW_EMITTER"
const KW_ASYNC   = "KW_ASYNC"
const KW_EXPORT  = "KW_EXPORT"
const KW_INCLUDE = "KW_INCLUDE"

const KEYWORDS: Dictionary = {
	"main":    KW_MAIN,
	"fire":    KW_FIRE,
	"act":     KW_ACT,
	"bullet":  KW_BULLET,
	"bul":     KW_BULLET,
	"repeat":  KW_REPEAT,
	"wait":    KW_WAIT,
	"waitf":   KW_WAITF,
	"vanish":  KW_VANISH,
	"chdir":   KW_CHDIR,
	"chspd":   KW_CHSPD,
	"dir":     KW_DIR,
	"speed":   KW_SPEED,
	"spd":     KW_SPEED,
	"offset":  KW_OFFSET,
	"accel":   KW_ACCEL,
	"aim":     KW_AIM,
	"abs":     KW_ABS,
	"rel":     KW_REL,
	"seq":     KW_SEQ,
	"over":    KW_OVER,
	"x":       KW_X,
	"y":       KW_Y,
	"type":    KW_TYPE,
	"emitter": KW_EMITTER,
	"emt":     KW_EMITTER,
	"async":   KW_ASYNC,
	"export":  KW_EXPORT,
	"include": KW_INCLUDE,
}

# --- Instance fields ---
var type:  String
var value: String
var line:  int
var col:   int

func _init(p_type: String, p_value: String, p_line: int, p_col: int) -> void:
	type  = p_type
	value = p_value
	line  = p_line
	col   = p_col

func _to_string() -> String:
	return "[%s:'%s' L%d C%d]" % [type, value, line, col]
