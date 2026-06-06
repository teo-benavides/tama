#include "tama_parser.h"
#include "tama_interpreter.h" // for TamaNodeType enum

#include <algorithm>
#include <sstream>

using namespace godot;

// NodeType shorthand
using NT = TamaNodeType;

// DirType  : AIM=0 ABS=1 REL=2 SEQ=3
// ValueType: ABS=0 REL=1 SEQ=2

// ===========================================================================
// Cursor helpers
// ===========================================================================

TT TamaParserCpp::peek_type_at(int offset) {
    int idx = _pos + offset;
    if (idx >= (int)_tokens.size()) return TT::EOF_;
    return _tokens[idx].type;
}

TamaToken TamaParserCpp::consume(TT expected) {
    TamaToken tok = _tokens[_pos];
    if (expected != TT::EOF_ && tok.type != expected) {
        std::ostringstream oss;
        oss << "Expected " << tama_token_name(expected) << ", got '"
            << (tok.value.empty() ? tama_token_name(tok.type) : tok.value) << "'";
        error_at(tok, oss.str());
        return tok;
    }
    _pos++;
    return tok;
}

bool TamaParserCpp::try_consume(TT t) {
    if (_tokens[_pos].type == t) { _pos++; return true; }
    return false;
}

void TamaParserCpp::skip_newlines() {
    while (_tokens[_pos].type == TT::NEWLINE) _pos++;
}

void TamaParserCpp::error_at(const TamaToken &tok, const std::string &msg) {
    int length = std::max((int)tok.value.size(), 1);
    _errors.push_back({tok.line, tok.col, length, msg});
}

std::string TamaParserCpp::tok_display(const TamaToken &tok) {
    return tok.value.empty() ? tama_token_name(tok.type) : tok.value;
}

// ===========================================================================
// Pre-scan: collect all top-level definition names.
// ===========================================================================

void TamaParserCpp::pre_scan_definitions() {
    int depth = 0;
    for (int i = 0; i < (int)_tokens.size(); ++i) {
        switch (_tokens[i].type) {
            case TT::INDENT: depth++; break;
            case TT::DEDENT: depth--; break;
            case TT::KW_INCLUDE:
                if (depth == 0 && i + 1 < (int)_tokens.size() &&
                    _tokens[i + 1].type == TT::WORD) {
                    std::string inc_name = _tokens[i + 1].value;
                    if (!_resolved_includes.count(inc_name)) {
                        std::shared_ptr<_TamaASTNode> included;
                        if (_resolver) included = _resolver(inc_name);
                        _resolved_includes[inc_name] = included;
                        if (included) {
                            for (const auto &sp : included->fires)
                                if (sp) _defined_fires[sp->name] = true;
                            for (const auto &sp : included->acts)
                                if (sp) _defined_acts[sp->name] = true;
                            for (const auto &sp : included->bullets)
                                if (sp) _defined_bullets[sp->name] = true;
                        }
                    }
                }
                break;
            case TT::KW_FIRE:
                if (depth == 0 && i + 1 < (int)_tokens.size() &&
                    _tokens[i + 1].type == TT::WORD)
                    _defined_fires[_tokens[i + 1].value] = true;
                break;
            case TT::KW_ACT:
                if (depth == 0 && i + 1 < (int)_tokens.size() &&
                    _tokens[i + 1].type == TT::WORD)
                    _defined_acts[_tokens[i + 1].value] = true;
                break;
            case TT::KW_BULLET:
                if (depth == 0 && i + 1 < (int)_tokens.size() &&
                    _tokens[i + 1].type == TT::WORD)
                    _defined_bullets[_tokens[i + 1].value] = true;
                break;
            default: break;
        }
    }
}

// ===========================================================================
// Expression / arg helpers
// ===========================================================================

std::string TamaParserCpp::collect_to_eol() {
    std::string parts;
    while (peek_type() != TT::NEWLINE && peek_type() != TT::EOF_ &&
           peek_type() != TT::DEDENT) {
        if (!parts.empty()) parts += ' ';
        parts += _tokens[_pos].value;
        _pos++;
    }
    return parts;
}

std::string TamaParserCpp::collect_to_rparen(const TamaToken &open_tok) {
    std::string parts;
    int depth = 0;
    while (true) {
        TamaToken &t = peek();
        if (t.type == TT::EOF_) { error_at(open_tok, "Unclosed parenthesis"); break; }
        if (t.type == TT::LPAREN) depth++;
        else if (t.type == TT::RPAREN) { if (depth == 0) break; depth--; }
        else if (t.type == TT::COMMA && depth == 0) break;
        if (!parts.empty()) parts += ' ';
        parts += t.value;
        _pos++;
    }
    return parts;
}

std::vector<TamaArgVal> TamaParserCpp::parse_call_args(const TamaToken &caller_tok) {
    std::vector<TamaArgVal> args;
    if (peek_type() != TT::LPAREN) return args;
    TamaToken lp = consume(TT::LPAREN);
    skip_newlines();
    try_consume(TT::INDENT);
    while (peek_type() != TT::RPAREN && peek_type() != TT::EOF_) {
        args.push_back(parse_single_arg(lp));
        try_consume(TT::COMMA);
        skip_newlines();
    }
    if (peek_type() != TT::RPAREN) {
        error_at(caller_tok, "Unclosed argument list");
    } else {
        consume(TT::RPAREN);
    }
    return args;
}

TamaArgVal TamaParserCpp::parse_single_arg(const TamaToken &open_tok) {
    TamaToken &tok = peek();
    // Inline block as argument (keyword followed by NEWLINE).
    switch (tok.type) {
        case TT::KW_BULLET:
            if (peek_type_at(1) == TT::NEWLINE)
                return TamaArgVal(parse_inline_bullet());
            break;
        case TT::KW_ACT:
            if (peek_type_at(1) == TT::NEWLINE)
                return TamaArgVal(parse_inline_act());
            break;
        case TT::KW_FIRE:
            if (peek_type_at(1) == TT::NEWLINE)
                return TamaArgVal(parse_inline_fire());
            break;
        case TT::KW_EMITTER:
            if (peek_type_at(1) == TT::NEWLINE)
                return TamaArgVal(parse_inline_emitter());
            break;
        default: break;
    }
    // Qualifier keyword as value (aim/abs/rel/seq followed by , or ))
    if (tok.type == TT::KW_AIM || tok.type == TT::KW_ABS ||
        tok.type == TT::KW_REL || tok.type == TT::KW_SEQ) {
        TT next = peek_type_at(1);
        if (next == TT::COMMA || next == TT::RPAREN) {
            std::string val = tok.value; _pos++;
            return TamaArgVal(Variant(String(val.c_str())));
        }
    }
    // Named first-class ref (known definition, optionally pre-applied).
    if (tok.type == TT::WORD) {
        bool is_def = _defined_fires.count(tok.value) ||
                      _defined_acts.count(tok.value) ||
                      _defined_bullets.count(tok.value);
        TT next = peek_type_at(1);
        bool at_term = (next == TT::COMMA || next == TT::RPAREN);
        bool at_call = (next == TT::LPAREN);
        if (is_def && (at_term || at_call)) {
            TamaToken ref_tok = consume(TT::WORD);
            auto ref = tama_make_node((int)NT::REF_CALL_ARG);
            ref->name = ref_tok.value;
            if (at_call) ref->args = parse_call_args(ref_tok);
            return TamaArgVal(std::move(ref)); // owning constructor keeps node alive
        }
    }
    return TamaArgVal(Variant(String(collect_to_rparen(open_tok).c_str())));
}

std::string TamaParserCpp::parse_identifier() {
    TamaToken &tok = peek();
    if (tok.type == TT::WORD) {
        std::string v = tok.value; _pos++; return v;
    }
    std::string disp = tok_display(tok);
    error_at(tok, "Expected identifier, got \"" + disp + "\"");
    return "";
}

std::vector<std::string> TamaParserCpp::parse_param_list() {
    std::vector<std::string> params;
    if (peek_type() != TT::LPAREN) return params;
    TamaToken lp = consume(TT::LPAREN);
    while (peek_type() != TT::RPAREN && peek_type() != TT::EOF_) {
        if (peek_type() == TT::WORD) {
            params.push_back(_tokens[_pos].value);
            _pos++;
        } else if (peek_type() == TT::COMMA) {
            _pos++;
        } else {
            error_at(peek(), "Expected parameter name"); break;
        }
    }
    if (peek_type() != TT::RPAREN) error_at(lp, "Unclosed parameter list");
    else consume(TT::RPAREN);
    return params;
}

// ===========================================================================
// Qualifier helpers
// ===========================================================================

int TamaParserCpp::peek_dir_qualifier() {
    switch (peek_type()) {
        case TT::KW_AIM: _pos++; return 0; // DirType::AIM
        case TT::KW_ABS: _pos++; return 1;
        case TT::KW_REL: _pos++; return 2;
        case TT::KW_SEQ: _pos++; return 3;
        default: return 0; // AIM
    }
}

int TamaParserCpp::peek_value_qualifier(int default_val) {
    switch (peek_type()) {
        case TT::KW_ABS: _pos++; return 0;
        case TT::KW_REL: _pos++; return 1;
        case TT::KW_SEQ: _pos++; return 2;
        default: return default_val;
    }
}

// ===========================================================================
// Block helper
// ===========================================================================

std::vector<std::shared_ptr<_TamaASTNode>> TamaParserCpp::parse_block(StmtFn stmt_fn) {
    std::vector<std::shared_ptr<_TamaASTNode>> nodes;
    if (peek_type() != TT::INDENT) {
        error_at(peek(), "Expected indented block");
        return nodes;
    }
    consume(TT::INDENT);
    skip_newlines();
    while (peek_type() != TT::DEDENT && peek_type() != TT::EOF_) {
        auto node = stmt_fn();
        if (node) nodes.push_back(node);
        skip_newlines();
    }
    if (peek_type() == TT::DEDENT) consume(TT::DEDENT);
    else error_at(peek(), "Unclosed block (missing dedent)");
    return nodes;
}

// ===========================================================================
// Axis sub-parser (x/y lines with optional qualifier + expr)
// ===========================================================================

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_axis_node(int default_qt) {
    TamaToken tok = peek();
    std::string axis = tok.value; _pos++; // consume x or y
    int qt = default_qt;
    std::string qt_var;
    if (peek_type() == TT::WORD && peek_type_at(1) != TT::NEWLINE) {
        qt_var = _tokens[_pos].value; _pos++;
    } else {
        qt = peek_value_qualifier(default_qt);
    }
    std::string expr = collect_to_eol();
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::OFFSET_AXIS);
    n->axis          = axis;
    n->axis_type     = qt;
    n->axis_type_var = qt_var;
    n->expr          = expr;
    return n;
}

// ===========================================================================
// Shared sub-parsers
// ===========================================================================

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_dir() {
    TamaToken tok = consume(TT::KW_DIR);
    int qt = 0; std::string qt_var;
    if (peek_type() == TT::WORD && peek_type_at(1) != TT::NEWLINE) {
        qt_var = _tokens[_pos].value; _pos++;
    } else {
        qt = peek_dir_qualifier();
    }
    std::string expr = collect_to_eol();
    if (expr.empty()) error_at(peek(), "Expected expression after dir");
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::DIR);
    n->dir_type     = qt;
    n->dir_type_var = qt_var;
    n->expr         = expr;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_speed() {
    TamaToken tok = consume(TT::KW_SPEED);
    int qt = 0; std::string qt_var;
    if (peek_type() == TT::WORD && peek_type_at(1) != TT::NEWLINE) {
        qt_var = _tokens[_pos].value; _pos++;
    } else {
        qt = peek_value_qualifier(0); // ABS default
    }
    std::string expr = collect_to_eol();
    if (expr.empty()) error_at(peek(), "Expected expression after speed");
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::SPEED);
    n->speed_type     = qt;
    n->speed_type_var = qt_var;
    n->expr           = expr;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_wait() {
    TamaToken tok = consume(TT::KW_WAIT);
    std::string expr = collect_to_eol();
    if (expr.empty()) error_at(peek(), "Expected expression after wait");
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::WAIT);
    n->expr = expr;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_waitf() {
    consume(TT::KW_WAITF);
    std::string expr = collect_to_eol();
    if (expr.empty()) error_at(peek(), "Expected expression after waitf");
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::WAIT_FRAMES);
    n->expr = expr;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_vanish() {
    consume(TT::KW_VANISH);
    consume(TT::NEWLINE);
    return tama_make_node((int)NT::VANISH);
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_break() {
    consume(TT::KW_BREAK);
    consume(TT::NEWLINE);
    return tama_make_node((int)NT::BREAK);
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_over() {
    consume(TT::KW_OVER);
    std::string expr = collect_to_eol();
    if (expr.empty()) error_at(peek(), "Expected expression after over");
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::OVER);
    n->expr = expr;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_offset() {
    TamaToken tok = consume(TT::KW_OFFSET);
    if (peek_type() == TT::NEWLINE) {
        // Block form
        consume(TT::NEWLINE);
        auto node = tama_make_node((int)NT::OFFSET);
        parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
            TamaToken &t = peek();
            if (t.type == TT::KW_X || t.type == TT::KW_Y) {
                auto ax = parse_axis_node(1); // REL default
                if (ax->axis == "x") node->x = ax;
                else                 node->y = ax;
            } else {
                error_at(t, "Expected 'x' or 'y' in offset block");
                _pos++; try_consume(TT::NEWLINE);
            }
            return nullptr;
        });
        return node;
    } else {
        // Inline form
        std::string expr = collect_to_eol();
        if (expr.empty()) error_at(peek(), "Expected expression after offset");
        consume(TT::NEWLINE);
        auto n = tama_make_node((int)NT::OFFSET_INLINE);
        n->expr = expr;
        return n;
    }
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_pos() {
    consume(TT::KW_POS);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::POS);
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        TamaToken &t = peek();
        if (t.type == TT::KW_X || t.type == TT::KW_Y) {
            auto ax = parse_axis_node(0); // ABS default
            if (ax->axis == "x") node->x = ax;
            else                 node->y = ax;
        } else {
            error_at(t, "Expected 'x' or 'y' in pos block");
            _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_chdir() {
    TamaToken tok = consume(TT::KW_CHDIR);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::CHDIR);
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        switch (peek_type()) {
            case TT::KW_DIR:  node->dir  = parse_dir(); break;
            case TT::KW_OVER: node->over = parse_over(); break;
            default:
                error_at(peek(), "Unexpected token in chdir block: '" + tok_display(peek()) + "'");
                _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    if (!node->dir)
        error_at(tok, "chdir requires a dir statement");
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_chspd() {
    TamaToken tok = consume(TT::KW_CHSPD);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::CHSPD);
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        switch (peek_type()) {
            case TT::KW_SPEED: node->speed = parse_speed(); break;
            case TT::KW_OVER:  node->over  = parse_over(); break;
            default:
                error_at(peek(), "Unexpected token in chspd block");
                _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    if (!node->speed)
        error_at(tok, "chspd requires a speed statement");
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_chpos() {
    TamaToken tok = consume(TT::KW_CHPOS);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::CHPOS);
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        if (peek_type() == TT::KW_X || peek_type() == TT::KW_Y) {
            auto ax = parse_axis_node(0); // ABS default
            if (ax->axis == "x") node->x = ax;
            else                 node->y = ax;
        } else if (peek_type() == TT::KW_OVER) {
            node->over = parse_over();
        } else {
            error_at(peek(), "Unexpected token in chpos block");
            _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    if (!node->x && !node->y)
        error_at(tok, "chpos requires at least one of x/y");
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_accel() {
    TamaToken tok = consume(TT::KW_ACCEL);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::ACCEL);
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        if (peek_type() == TT::KW_X || peek_type() == TT::KW_Y) {
            auto ax = parse_axis_node(0);
            if (ax->axis == "x") node->x = ax;
            else                 node->y = ax;
        } else if (peek_type() == TT::KW_OVER) {
            node->over = parse_over();
        } else {
            error_at(peek(), "Unexpected token in accel block");
            _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    if (!node->x && !node->y)
        error_at(tok, "accel requires at least one of x or y");
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_mvmt() {
    consume(TT::KW_MVMT);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::MVMT);
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        if (peek_type() == TT::KW_X || peek_type() == TT::KW_Y) {
            auto ax = parse_axis_node(0);
            if (ax->axis == "x") node->x = ax;
            else                 node->y = ax;
        } else {
            error_at(peek(), "Expected 'x' or 'y' in mvmt block");
            _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_repeatf() {
    consume(TT::KW_REPEATF);
    std::string count_str, idx_var;
    if (peek_type() != TT::NEWLINE) {
        // Find line end
        int end = _pos;
        while (end < (int)_tokens.size() &&
               _tokens[end].type != TT::NEWLINE &&
               _tokens[end].type != TT::EOF_) end++;
        // Last word = index_var if there are tokens before it
        if (end - 1 > _pos && _tokens[end - 1].type == TT::WORD) {
            idx_var = _tokens[end - 1].value;
            while (_pos < end - 1) {
                if (!count_str.empty()) count_str += ' ';
                count_str += _tokens[_pos].value;
                _pos++;
            }
            _pos++; // consume index word
        } else {
            count_str = collect_to_eol();
        }
    }
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::REPEAT_FRAME);
    node->count     = count_str;
    node->index_var = idx_var;
    node->body = parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        auto s = parse_action_statement();
        try_consume(TT::NEWLINE);
        return s;
    });
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_repeat() {
    consume(TT::KW_REPEAT);
    std::string count_str, idx_var;
    if (peek_type() != TT::NEWLINE) {
        int end = _pos;
        while (end < (int)_tokens.size() &&
               _tokens[end].type != TT::NEWLINE &&
               _tokens[end].type != TT::EOF_) end++;
        if (end - 1 > _pos && _tokens[end - 1].type == TT::WORD) {
            idx_var = _tokens[end - 1].value;
            while (_pos < end - 1) {
                if (!count_str.empty()) count_str += ' ';
                count_str += _tokens[_pos].value;
                _pos++;
            }
            _pos++;
        } else {
            count_str = collect_to_eol();
        }
    }
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::REPEAT);
    node->count     = count_str;
    node->index_var = idx_var;
    node->body = parse_block([this]() { return parse_action_statement(); });
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_while() {
    consume(TT::KW_WHILE);
    std::string cond = collect_to_eol();
    if (cond.empty()) error_at(peek(), "Expected condition after 'while'");
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::WHILE);
    node->condition = cond;
    node->body = parse_block([this]() { return parse_action_statement(); });
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_if() {
    consume(TT::KW_IF);
    std::string cond = collect_to_eol();
    if (cond.empty()) error_at(peek(), "Expected condition after 'if'");
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::IF);
    node->conditions.push_back(cond);
    node->bodies.push_back(parse_block([this]() { return parse_action_statement(); }));
    skip_newlines();
    while (peek_type() == TT::KW_ELIF) {
        consume(TT::KW_ELIF);
        cond = collect_to_eol();
        consume(TT::NEWLINE);
        node->conditions.push_back(cond);
        node->bodies.push_back(parse_block([this]() { return parse_action_statement(); }));
        skip_newlines();
    }
    if (peek_type() == TT::KW_ELSE) {
        consume(TT::KW_ELSE);
        consume(TT::NEWLINE);
        node->else_body = parse_block([this]() { return parse_action_statement(); });
    }
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_var_decl() {
    TamaToken tok = consume(TT::KW_VAR);
    std::string name = parse_identifier();
    if (name.empty()) { try_consume(TT::NEWLINE); return nullptr; }
    std::string expr = collect_to_eol();
    if (expr.empty()) error_at(peek(), "Expected expression after var name");
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::VAR_DECL);
    n->var_name = name;
    n->expr     = expr;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_var_assign() {
    TamaToken tok = consume(TT::WORD);
    std::string expr = collect_to_eol();
    if (expr.empty()) error_at(peek(), "Expected expression after variable name");
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::VAR_DECL);
    n->var_name = tok.value;
    n->expr     = expr;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_inline_act() {
    consume(TT::KW_ACT);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::INLINE_ACT);
    node->body     = parse_block([this]() { return parse_action_statement(); });
    node->is_async = false;
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_inline_emitter() {
    consume(TT::KW_EMITTER);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::INLINE_ACT);
    node->body     = parse_block([this]() { return parse_action_statement(); });
    node->is_async = false;
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_inline_fire() {
    consume(TT::KW_FIRE);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::INLINE_FIRE);
    parse_fire_block(node, _tokens[_pos - 1]);
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_fire_call() {
    consume(TT::KW_FIRE);
    TamaToken name_tok = peek();
    std::string name = parse_identifier();
    if (name.empty()) return nullptr;
    auto args = parse_call_args(name_tok);
    if (!_current_params.count(name))
        _fire_refs.push_back({name, name_tok.line, name_tok.col});
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::FIRE_CALL);
    n->name = name;
    n->args = std::move(args);
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_act_call() {
    consume(TT::KW_ACT);
    TamaToken name_tok = peek();
    std::string name = parse_identifier();
    if (name.empty()) return nullptr;
    auto args = parse_call_args(name_tok);
    if (!_current_params.count(name))
        _act_refs.push_back({name, name_tok.line, name_tok.col});
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::ACT_CALL);
    n->name     = name;
    n->args     = std::move(args);
    n->is_async = false;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_async_act() {
    consume(TT::KW_ASYNC);
    if (peek_type() != TT::KW_ACT) {
        error_at(peek(), "Expected 'act' after 'async'");
        return nullptr;
    }
    std::shared_ptr<_TamaASTNode> node;
    if (peek_type_at(1) == TT::NEWLINE) {
        node = parse_inline_act();
    } else {
        node = parse_act_call();
    }
    if (node) node->is_async = true;
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_bullet_call() {
    consume(TT::KW_BULLET);
    TamaToken name_tok = peek();
    std::string name = parse_identifier();
    if (name.empty()) return nullptr;
    auto args = parse_call_args(name_tok);
    if (!_current_params.count(name))
        _bullet_refs.push_back({name, name_tok.line, name_tok.col});
    consume(TT::NEWLINE);
    auto n = tama_make_node((int)NT::BULLET_CALL);
    n->name = name;
    n->args = std::move(args);
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_inline_bullet() {
    consume(TT::KW_BULLET);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::INLINE_BULLET);
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        TamaToken &t = peek();
        switch (t.type) {
            case TT::KW_TYPE:
                consume(TT::KW_TYPE);
                if (peek_type() == TT::WORD) {
                    node->bullet_type = _tokens[_pos].value;
                    _pos++;
                } else {
                    error_at(peek(), "Expected bullet type name after 'type'");
                }
                consume(TT::NEWLINE);
                break;
            case TT::KW_EMITTER:
                if (peek_type_at(1) == TT::NEWLINE) {
                    node->emitter_act = parse_inline_emitter();
                } else {
                    consume(TT::KW_EMITTER);
                    TamaToken emt_tok = peek();
                    std::string emt_name = parse_identifier();
                    if (emt_name.empty()) {
                        error_at(emt_tok, "Expected act name after 'emt'");
                    } else {
                        std::vector<TamaArgVal> emt_args;
                        if (peek_type() == TT::LPAREN) emt_args = parse_call_args(emt_tok);
                        auto ac = tama_make_node((int)NT::ACT_CALL);
                        ac->name     = emt_name;
                        ac->args     = std::move(emt_args);
                        ac->is_async = false;
                        node->emitter_act = ac;
                        if (!_current_params.count(emt_name))
                            _act_refs.push_back({emt_name, emt_tok.line, emt_tok.col});
                    }
                    consume(TT::NEWLINE);
                }
                break;
            case TT::KW_ACT:
                if (peek_type_at(1) == TT::NEWLINE) {
                    node->act = parse_inline_act();
                } else {
                    node->act = parse_act_call();
                }
                break;
            case TT::KW_MVMT:
                node->mvmt = parse_mvmt();
                break;
            default:
                error_at(t, "Unexpected token in bullet block: '" + tok_display(t) + "'");
                _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    return node;
}

// ===========================================================================
// Action-block dispatcher
// ===========================================================================

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_action_statement() {
    TamaToken &tok = peek();
    switch (tok.type) {
        case TT::KW_DIR:    return parse_dir();
        case TT::KW_SPEED:  return parse_speed();
        case TT::KW_WAIT:   return parse_wait();
        case TT::KW_WAITF:  return parse_waitf();
        case TT::KW_VANISH: return parse_vanish();
        case TT::KW_BREAK:  return parse_break();
        case TT::KW_OFFSET: return parse_offset();
        case TT::KW_CHDIR:  return parse_chdir();
        case TT::KW_CHSPD:  return parse_chspd();
        case TT::KW_CHPOS:  return parse_chpos();
        case TT::KW_ACCEL:  return parse_accel();
        case TT::KW_REPEAT: return parse_repeat();
        case TT::KW_REPEATF:return parse_repeatf();
        case TT::KW_ACT:
            if (peek_type_at(1) == TT::NEWLINE) return parse_inline_act();
            else                                 return parse_act_call();
        case TT::KW_ASYNC:  return parse_async_act();
        case TT::KW_FIRE:
            if (peek_type_at(1) == TT::NEWLINE) return parse_inline_fire();
            else                                 return parse_fire_call();
        case TT::KW_WHILE:  return parse_while();
        case TT::KW_IF:     return parse_if();
        case TT::KW_VAR:    return parse_var_decl();
        case TT::WORD:      return parse_var_assign();
        default:
            error_at(tok, "Unexpected token in action block: '" + tok_display(tok) + "'");
            _pos++; try_consume(TT::NEWLINE);
            return nullptr;
    }
}

// ===========================================================================
// Fire-block parser
// ===========================================================================

void TamaParserCpp::parse_fire_block(const std::shared_ptr<_TamaASTNode> &node, const TamaToken &open_tok) {
    if (peek_type() != TT::INDENT) {
        error_at(peek(), "Expected indented block");
        return;
    }
    consume(TT::INDENT);
    skip_newlines();
    while (peek_type() != TT::DEDENT && peek_type() != TT::EOF_) {
        TamaToken &t = peek();
        switch (t.type) {
            case TT::KW_DIR:    node->dir    = parse_dir(); break;
            case TT::KW_SPEED:  node->speed  = parse_speed(); break;
            case TT::KW_OFFSET: node->offset = parse_offset(); break;
            case TT::KW_POS:    node->pos    = parse_pos(); break;
            case TT::KW_BULLET:
                if (peek_type_at(1) == TT::NEWLINE)
                    node->bullet = parse_inline_bullet();
                else
                    node->bullet = parse_bullet_call();
                break;
            default:
                error_at(t, "Unexpected token in fire block: '" + tok_display(t) + "'");
                _pos++; try_consume(TT::NEWLINE);
        }
        skip_newlines();
    }
    if (peek_type() == TT::DEDENT) consume(TT::DEDENT);
    else error_at(peek(), "Unclosed block (missing dedent)");
}

// ===========================================================================
// Top-level definition parsers
// ===========================================================================

void TamaParserCpp::parse_include(const std::shared_ptr<_TamaASTNode> &program) {
    consume(TT::KW_INCLUDE);
    TamaToken name_tok = peek();
    std::string name = parse_identifier();
    if (name.empty()) { try_consume(TT::NEWLINE); return; }
    consume(TT::NEWLINE);
    auto it = _resolved_includes.find(name);
    if (it == _resolved_includes.end() || !it->second) {
        error_at(name_tok, "Cannot resolve include '" + name + "'");
        return;
    }
    const auto &inc = it->second;
    for (const auto &sp : inc->fires)   program->fires.push_back(sp);
    for (const auto &sp : inc->acts)    program->acts.push_back(sp);
    for (const auto &sp : inc->bullets) program->bullets.push_back(sp);
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_export() {
    consume(TT::KW_EXPORT);
    TamaToken type_tok = peek();
    static const std::unordered_map<std::string,int> valid = {
        {"num",0},{"str",1},{"bool",2}
    };
    if (type_tok.type != TT::WORD || !valid.count(type_tok.value)) {
        error_at(type_tok, "Expected 'num', 'str', or 'bool' after 'export'");
        try_consume(TT::NEWLINE);
        return nullptr;
    }
    std::string export_type = type_tok.value; _pos++;
    TamaToken name_tok = peek();
    std::string name = parse_identifier();
    if (name.empty()) { error_at(name_tok, "Expected identifier"); return nullptr; }

    Variant default_value;
    if (peek_type() != TT::NEWLINE && peek_type() != TT::EOF_) {
        std::string raw = collect_to_eol();
        // Trim
        size_t s = raw.find_first_not_of(' ');
        size_t e = raw.find_last_not_of(' ');
        if (s != std::string::npos) raw = raw.substr(s, e - s + 1);
        if (!raw.empty()) {
            if (export_type == "num") {
                default_value = Variant(std::stof(raw));
            } else if (export_type == "bool") {
                default_value = Variant(raw == "true");
            } else {
                default_value = Variant(String(raw.c_str()));
            }
        }
    }
    consume(TT::NEWLINE);

    auto n = tama_make_node((int)NT::EXPORT_VAR);
    n->name          = name;
    n->export_type   = export_type;
    n->default_value = default_value;
    return n;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_main() {
    consume(TT::KW_MAIN);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::MAIN);
    node->body = parse_block([this]() { return parse_action_statement(); });
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_fire_def() {
    consume(TT::KW_FIRE);
    TamaToken name_tok = peek();
    std::string name = parse_identifier();
    if (name.empty()) return nullptr;
    _defined_fires[name] = true;
    auto params = parse_param_list();
    _current_params.clear();
    for (const auto &p : params)
        _current_params.insert(p);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::FIRE_DEF);
    node->name   = name;
    node->params = params;
    parse_fire_block(node, name_tok);
    _current_params.clear();
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_act_def() {
    consume(TT::KW_ACT);
    std::string name = parse_identifier();
    if (name.empty()) return nullptr;
    _defined_acts[name] = true;
    auto params = parse_param_list();
    _current_params.clear();
    for (const auto &p : params)
        _current_params.insert(p);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::ACT_DEF);
    node->name   = name;
    node->params = params;
    node->body   = parse_block([this]() { return parse_action_statement(); });
    _current_params.clear();
    return node;
}

std::shared_ptr<_TamaASTNode> TamaParserCpp::parse_bullet_def() {
    consume(TT::KW_BULLET);
    std::string name = parse_identifier();
    if (name.empty()) return nullptr;
    _defined_bullets[name] = true;
    auto params = parse_param_list();
    _current_params.clear();
    for (const auto &p : params)
        _current_params.insert(p);
    consume(TT::NEWLINE);
    auto node = tama_make_node((int)NT::BULLET_DEF);
    node->name   = name;
    node->params = params;
    parse_block([&]() -> std::shared_ptr<_TamaASTNode> {
        TamaToken &t = peek();
        switch (t.type) {
            case TT::KW_TYPE:
                consume(TT::KW_TYPE);
                if (peek_type() == TT::WORD) {
                    node->bullet_type = _tokens[_pos].value;
                    _pos++;
                } else {
                    error_at(peek(), "Expected bullet type name after 'type'");
                }
                consume(TT::NEWLINE);
                break;
            case TT::KW_EMITTER:
                if (peek_type_at(1) == TT::NEWLINE) {
                    node->emitter_act = parse_inline_emitter();
                } else {
                    consume(TT::KW_EMITTER);
                    TamaToken emt_tok = peek();
                    std::string emt_name = parse_identifier();
                    if (emt_name.empty()) {
                        error_at(emt_tok, "Expected act name after 'emt'");
                    } else {
                        std::vector<TamaArgVal> emt_args;
                        if (peek_type() == TT::LPAREN) emt_args = parse_call_args(emt_tok);
                        auto ac = tama_make_node((int)NT::ACT_CALL);
                        ac->name     = emt_name;
                        ac->args     = std::move(emt_args);
                        ac->is_async = false;
                        node->emitter_act = ac;
                        if (!_current_params.count(emt_name))
                            _act_refs.push_back({emt_name, emt_tok.line, emt_tok.col});
                    }
                    consume(TT::NEWLINE);
                }
                break;
            case TT::KW_ACT:
                if (peek_type_at(1) == TT::NEWLINE)
                    node->act = parse_inline_act();
                else
                    node->act = parse_act_call();
                break;
            case TT::KW_MVMT:
                node->mvmt = parse_mvmt();
                break;
            default:
                error_at(t, "Unexpected token in bullet block: '" + tok_display(t) + "'");
                _pos++; try_consume(TT::NEWLINE);
        }
        return nullptr;
    });
    _current_params.clear();
    return node;
}

// ===========================================================================
// Post-parse reference validation
// ===========================================================================

void TamaParserCpp::resolve_references() {
    for (auto &ref : _fire_refs) {
        if (!_defined_fires.count(ref.name))
            _errors.push_back({ref.line, ref.col, (int)ref.name.size(),
                               "Unknown fire reference '" + ref.name + "'"});
    }
    for (auto &ref : _act_refs) {
        if (!_defined_acts.count(ref.name))
            _errors.push_back({ref.line, ref.col, (int)ref.name.size(),
                               "Unknown act reference '" + ref.name + "'"});
    }
    for (auto &ref : _bullet_refs) {
        if (!_defined_bullets.count(ref.name))
            _errors.push_back({ref.line, ref.col, (int)ref.name.size(),
                               "Unknown bullet reference '" + ref.name + "'"});
    }
}

// ===========================================================================
// Entry point
// ===========================================================================

TamaParseResult TamaParserCpp::parse(const std::vector<TamaToken> &tokens,
                                     TamaResolver resolver) {
    _tokens    = tokens;
    _pos       = 0;
    _errors.clear();
    _defined_fires.clear();
    _defined_acts.clear();
    _defined_bullets.clear();
    _current_params.clear();
    _fire_refs.clear();
    _act_refs.clear();
    _bullet_refs.clear();
    _resolver  = resolver;
    _resolved_includes.clear();

    auto program = tama_make_node((int)NT::PROGRAM);

    pre_scan_definitions();
    skip_newlines();

    while (peek_type() != TT::EOF_) {
        TamaToken &tok = peek();
        switch (tok.type) {
            case TT::KW_INCLUDE: parse_include(program); break;
            case TT::KW_EXPORT: {
                auto n = parse_export();
                if (n) program->exports.push_back(n);
                break;
            }
            case TT::KW_MAIN:
                program->main = parse_main();
                break;
            case TT::KW_FIRE: {
                auto n = parse_fire_def();
                if (n) program->fires.push_back(n);
                break;
            }
            case TT::KW_ACT: {
                auto n = parse_act_def();
                if (n) program->acts.push_back(n);
                break;
            }
            case TT::KW_BULLET: {
                auto n = parse_bullet_def();
                if (n) program->bullets.push_back(n);
                break;
            }
            case TT::ERROR:
                error_at(tok, "Unexpected character: '" + tok.value + "'");
                _pos++; break;
            default:
                error_at(tok, "Unexpected top-level token: '" + tok_display(tok) + "'");
                _pos++; break;
        }
        skip_newlines();
    }

    resolve_references();

    TamaParseResult result;
    result.program = program;
    result.errors  = _errors;
    return result;
}
