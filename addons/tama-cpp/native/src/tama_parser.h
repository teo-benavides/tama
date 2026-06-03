#pragma once
#include "tama_token.h"
#include "tama_ast_nodes.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Called by the parser to resolve 'include' directives.
// Should return the ProgramNode for the named script, or an empty Ref on failure.
using TamaResolver = std::function<godot::Ref<_TamaASTNode>(const std::string &)>;

struct TamaParseError {
    int         line, col, length;
    std::string message;
};

struct TamaParseResult {
    godot::Ref<_TamaASTNode>      program; // NodeType::PROGRAM, or empty on total failure
    std::vector<TamaParseError>  errors;
    bool ok() const { return errors.empty(); }
};

// Pure C++ recursive-descent parser. Not exposed to Godot directly;
// _TamaScriptRepository calls it internally.
class TamaParserCpp {
public:
    TamaParseResult parse(const std::vector<TamaToken> &tokens,
                          TamaResolver resolver = nullptr);

private:
    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    std::vector<TamaToken>  _tokens;
    int                     _pos = 0;
    std::vector<TamaParseError> _errors;

    std::unordered_map<std::string, bool> _defined_fires;
    std::unordered_map<std::string, bool> _defined_acts;
    std::unordered_map<std::string, bool> _defined_bullets;

    // Parameters of the definition currently being parsed — excluded from ref validation.
    std::unordered_set<std::string> _current_params;

    struct Ref3 { std::string name; int line, col; };
    std::vector<Ref3> _fire_refs, _act_refs, _bullet_refs;

    TamaResolver _resolver;
    std::unordered_map<std::string, godot::Ref<_TamaASTNode>> _resolved_includes;

    // -----------------------------------------------------------------------
    // Cursor helpers
    // -----------------------------------------------------------------------
    TamaToken &peek()           { return _tokens[_pos]; }
    TT         peek_type()      { return _tokens[_pos].type; }
    TT         peek_type_at(int offset);
    TamaToken  consume(TT expected = TT::EOF_);
    bool       try_consume(TT t);
    void       skip_newlines();
    void       error_at(const TamaToken &tok, const std::string &msg);
    std::string tok_display(const TamaToken &tok);

    // -----------------------------------------------------------------------
    // Pre-scan
    // -----------------------------------------------------------------------
    void pre_scan_definitions();

    // -----------------------------------------------------------------------
    // Expression / arg helpers
    // -----------------------------------------------------------------------
    std::string collect_to_eol();
    std::string collect_to_rparen(const TamaToken &open_tok);
    godot::Array parse_call_args(const TamaToken &caller_tok);
    godot::Variant parse_single_arg(const TamaToken &open_tok);
    std::string parse_identifier();
    godot::Array parse_param_list();   // Array[String]

    // -----------------------------------------------------------------------
    // Qualifier helpers
    // -----------------------------------------------------------------------
    int peek_dir_qualifier();    // DirType int
    int peek_value_qualifier(int default_val); // ValueType int

    // -----------------------------------------------------------------------
    // Shared sub-parsers
    // -----------------------------------------------------------------------
    godot::Ref<_TamaASTNode> parse_dir();
    godot::Ref<_TamaASTNode> parse_speed();
    godot::Ref<_TamaASTNode> parse_wait();
    godot::Ref<_TamaASTNode> parse_waitf();
    godot::Ref<_TamaASTNode> parse_vanish();
    godot::Ref<_TamaASTNode> parse_break();
    godot::Ref<_TamaASTNode> parse_over();
    godot::Ref<_TamaASTNode> parse_offset();
    godot::Ref<_TamaASTNode> parse_pos();
    godot::Ref<_TamaASTNode> parse_chdir();
    godot::Ref<_TamaASTNode> parse_chspd();
    godot::Ref<_TamaASTNode> parse_chpos();
    godot::Ref<_TamaASTNode> parse_accel();
    godot::Ref<_TamaASTNode> parse_repeatf();
    godot::Ref<_TamaASTNode> parse_mvmt();
    godot::Ref<_TamaASTNode> parse_while();
    godot::Ref<_TamaASTNode> parse_if();
    godot::Ref<_TamaASTNode> parse_var_decl();
    godot::Ref<_TamaASTNode> parse_var_assign();
    godot::Ref<_TamaASTNode> parse_repeat();
    godot::Ref<_TamaASTNode> parse_inline_act();
    godot::Ref<_TamaASTNode> parse_inline_emitter();
    godot::Ref<_TamaASTNode> parse_inline_fire();
    godot::Ref<_TamaASTNode> parse_fire_call();
    godot::Ref<_TamaASTNode> parse_act_call();
    godot::Ref<_TamaASTNode> parse_async_act();
    godot::Ref<_TamaASTNode> parse_bullet_call();
    godot::Ref<_TamaASTNode> parse_inline_bullet();

    // -----------------------------------------------------------------------
    // Action-block dispatcher
    // -----------------------------------------------------------------------
    godot::Ref<_TamaASTNode> parse_action_statement();

    // -----------------------------------------------------------------------
    // Fire-block parser (fills dir/speed/offset/pos/bullet onto node)
    // -----------------------------------------------------------------------
    void parse_fire_block(godot::Ref<_TamaASTNode> node, const TamaToken &open_tok);

    // -----------------------------------------------------------------------
    // Axis sub-parser (used by offset/pos/chpos/accel/mvmt blocks)
    // -----------------------------------------------------------------------
    godot::Ref<_TamaASTNode> parse_axis_node(int default_qt); // ValueType default

    // -----------------------------------------------------------------------
    // Block helper: consumes INDENT … DEDENT, calling stmt_fn for each stmt.
    // -----------------------------------------------------------------------
    using StmtFn = std::function<godot::Ref<_TamaASTNode>()>;
    godot::Array parse_block(StmtFn stmt_fn);

    // -----------------------------------------------------------------------
    // Top-level definition parsers
    // -----------------------------------------------------------------------
    void                    parse_include(godot::Ref<_TamaASTNode> program);
    godot::Ref<_TamaASTNode> parse_export();
    godot::Ref<_TamaASTNode> parse_main();
    godot::Ref<_TamaASTNode> parse_fire_def();
    godot::Ref<_TamaASTNode> parse_act_def();
    godot::Ref<_TamaASTNode> parse_bullet_def();

    // -----------------------------------------------------------------------
    // Post-parse reference validation
    // -----------------------------------------------------------------------
    void resolve_references();
};
