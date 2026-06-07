#pragma once
#include "tama_token.h"
#include "tama_ast_nodes.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Called by the parser to resolve 'include' directives.
using TamaResolver = std::function<std::shared_ptr<_TamaASTNode>(const std::string &)>;

struct TamaParseError {
    int         line, col, length;
    std::string message;
};

struct TamaParseResult {
    std::shared_ptr<_TamaASTNode>  program; // NodeType::PROGRAM, or null on total failure
    std::vector<TamaParseError>   errors;
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
    std::unordered_map<std::string, std::shared_ptr<_TamaASTNode>> _resolved_includes;

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
    std::vector<TamaArgVal>    parse_call_args(const TamaToken &caller_tok);
    TamaArgVal                 parse_single_arg(const TamaToken &open_tok);
    std::string                parse_identifier();
    std::vector<std::string>   parse_param_list();

    // -----------------------------------------------------------------------
    // Qualifier helpers
    // -----------------------------------------------------------------------
    int peek_dir_qualifier();    // DirType int
    int peek_value_qualifier(int default_val); // ValueType int

    // -----------------------------------------------------------------------
    // Shared sub-parsers
    // -----------------------------------------------------------------------
    std::shared_ptr<_TamaASTNode> parse_dir();
    std::shared_ptr<_TamaASTNode> parse_speed();
    std::shared_ptr<_TamaASTNode> parse_wait();
    std::shared_ptr<_TamaASTNode> parse_waitf();
    std::shared_ptr<_TamaASTNode> parse_vanish();
    std::shared_ptr<_TamaASTNode> parse_break();
    std::shared_ptr<_TamaASTNode> parse_over();
    std::shared_ptr<_TamaASTNode> parse_offset();
    std::shared_ptr<_TamaASTNode> parse_pos();
    std::shared_ptr<_TamaASTNode> parse_chdir();
    std::shared_ptr<_TamaASTNode> parse_chspd();
    std::shared_ptr<_TamaASTNode> parse_chrotspd();
    std::shared_ptr<_TamaASTNode> parse_rotspd();
    std::shared_ptr<_TamaASTNode> parse_delay();
    std::shared_ptr<_TamaASTNode> parse_chpos();
    std::shared_ptr<_TamaASTNode> parse_accel();
    std::shared_ptr<_TamaASTNode> parse_repeatf();
    std::shared_ptr<_TamaASTNode> parse_mvmt();
    std::shared_ptr<_TamaASTNode> parse_while();
    std::shared_ptr<_TamaASTNode> parse_if();
    std::shared_ptr<_TamaASTNode> parse_var_decl();
    std::shared_ptr<_TamaASTNode> parse_var_assign();
    std::shared_ptr<_TamaASTNode> parse_repeat();
    std::shared_ptr<_TamaASTNode> parse_inline_act();
    std::shared_ptr<_TamaASTNode> parse_inline_emitter();
    std::shared_ptr<_TamaASTNode> parse_inline_fire();
    std::shared_ptr<_TamaASTNode> parse_fire_call();
    std::shared_ptr<_TamaASTNode> parse_act_call();
    std::shared_ptr<_TamaASTNode> parse_async_act();
    std::shared_ptr<_TamaASTNode> parse_bullet_call();
    std::shared_ptr<_TamaASTNode> parse_inline_bullet();

    // -----------------------------------------------------------------------
    // Action-block dispatcher
    // -----------------------------------------------------------------------
    std::shared_ptr<_TamaASTNode> parse_action_statement();

    // -----------------------------------------------------------------------
    // Fire-block parser (fills dir/speed/offset/pos/bullet onto node)
    // -----------------------------------------------------------------------
    void parse_fire_block(const std::shared_ptr<_TamaASTNode> &node, const TamaToken &open_tok);

    // -----------------------------------------------------------------------
    // Axis sub-parser (used by offset/pos/chpos/accel/mvmt blocks)
    // -----------------------------------------------------------------------
    std::shared_ptr<_TamaASTNode> parse_axis_node(int default_qt); // ValueType default

    // -----------------------------------------------------------------------
    // Block helper: consumes INDENT … DEDENT, calling stmt_fn for each stmt.
    // -----------------------------------------------------------------------
    using StmtFn = std::function<std::shared_ptr<_TamaASTNode>()>;
    std::vector<std::shared_ptr<_TamaASTNode>> parse_block(StmtFn stmt_fn);

    // -----------------------------------------------------------------------
    // Top-level definition parsers
    // -----------------------------------------------------------------------
    void                          parse_include(const std::shared_ptr<_TamaASTNode> &program);
    std::shared_ptr<_TamaASTNode> parse_export();
    std::shared_ptr<_TamaASTNode> parse_main();
    std::shared_ptr<_TamaASTNode> parse_fire_def();
    std::shared_ptr<_TamaASTNode> parse_act_def();
    std::shared_ptr<_TamaASTNode> parse_bullet_def();

    // -----------------------------------------------------------------------
    // Post-parse reference validation
    // -----------------------------------------------------------------------
    void resolve_references();
};
