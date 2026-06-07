#include "tama_lexer.h"
#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, TT> KEYWORDS = {
    {"main",    TT::KW_MAIN},    {"fire",    TT::KW_FIRE},
    {"act",     TT::KW_ACT},     {"bullet",  TT::KW_BULLET},
    {"bul",     TT::KW_BULLET},  {"repeat",  TT::KW_REPEAT},
    {"wait",    TT::KW_WAIT},    {"waitf",   TT::KW_WAITF},
    {"vanish",  TT::KW_VANISH},  {"chdir",   TT::KW_CHDIR},
    {"chspd",   TT::KW_CHSPD},   {"dir",     TT::KW_DIR},
    {"speed",   TT::KW_SPEED},   {"spd",     TT::KW_SPEED},
    {"offset",  TT::KW_OFFSET},  {"accel",   TT::KW_ACCEL},
    {"aim",     TT::KW_AIM},     {"abs",     TT::KW_ABS},
    {"rel",     TT::KW_REL},     {"seq",     TT::KW_SEQ},
    {"over",    TT::KW_OVER},    {"x",       TT::KW_X},
    {"y",       TT::KW_Y},       {"type",    TT::KW_TYPE},
    {"emitter", TT::KW_EMITTER}, {"emt",     TT::KW_EMITTER},
    {"async",   TT::KW_ASYNC},   {"export",  TT::KW_EXPORT},
    {"include", TT::KW_INCLUDE}, {"pos",     TT::KW_POS},
    {"chpos",   TT::KW_CHPOS},   {"repeatf", TT::KW_REPEATF},
    {"mvmt",    TT::KW_MVMT},    {"var",     TT::KW_VAR},
    {"if",      TT::KW_IF},      {"elif",    TT::KW_ELIF},
    {"else",    TT::KW_ELSE},    {"true",    TT::KW_TRUE},
    {"false",   TT::KW_FALSE},   {"while",   TT::KW_WHILE},
    {"break",   TT::KW_BREAK},   {"bounces", TT::KW_BOUNCES},
    {"chrotspd", TT::KW_CHROTSPD}, {"rotspd",   TT::KW_ROTSPD},
};

TT tama_keyword_type(const std::string &word) {
    auto it = KEYWORDS.find(word);
    return (it != KEYWORDS.end()) ? it->second : TT::WORD;
}

const char *tama_token_name(TT t) {
    switch (t) {
        case TT::NUMBER:  return "NUMBER";
        case TT::WORD:    return "WORD";
        case TT::LPAREN:  return "LPAREN";
        case TT::RPAREN:  return "RPAREN";
        case TT::OP:      return "OP";
        case TT::COMMA:   return "COMMA";
        case TT::NEWLINE: return "NEWLINE";
        case TT::INDENT:  return "INDENT";
        case TT::DEDENT:  return "DEDENT";
        case TT::EOF_:    return "EOF";
        case TT::ERROR:   return "ERROR";
        default:          return "KW";
    }
}

// Split source into lines (handles \r\n and \r).
static std::vector<std::string> split_lines(const std::string &src) {
    std::vector<std::string> lines;
    std::string cur;
    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (c == '\r') {
            if (i + 1 < src.size() && src[i + 1] == '\n') ++i;
            lines.push_back(cur);
            cur.clear();
        } else if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    lines.push_back(cur);
    return lines;
}

std::vector<TamaToken> TamaLexerCpp::tokenize(const std::string &source) const {
    std::vector<TamaToken> tokens;
    auto lines = split_lines(source);
    std::vector<int> indent_stack = {0};

    for (int line_idx = 0; line_idx < (int)lines.size(); ++line_idx) {
        const std::string &raw = lines[line_idx];

        // Skip blank / comment-only lines.
        size_t first = raw.find_first_not_of(" \t");
        if (first == std::string::npos || raw[first] == '#') continue;

        // Measure indent (tabs = 4 spaces).
        int indent = 0;
        for (char c : raw) {
            if (c == ' ')       indent += 1;
            else if (c == '\t') indent += 4;
            else                break;
        }

        // Emit INDENT / DEDENT.
        int cur_indent = indent_stack.back();
        if (indent > cur_indent) {
            indent_stack.push_back(indent);
            tokens.push_back({TT::INDENT, "", line_idx, 0});
        } else if (indent < cur_indent) {
            while (indent_stack.back() > indent) {
                indent_stack.pop_back();
                tokens.push_back({TT::DEDENT, "", line_idx, 0});
            }
            if (indent_stack.back() != indent) {
                tokens.push_back({TT::ERROR, "Indentation mismatch", line_idx, 0});
            }
        }

        // Lex the content of this line.
        int pos = indent;
        int len = (int)raw.size();
        while (pos < len) {
            char c = raw[pos];

            // Skip inline whitespace.
            if (c == ' ' || c == '\t') { ++pos; continue; }

            // Comment — rest of line.
            if (c == '#') break;

            // Number: float before int.
            if (std::isdigit((unsigned char)c)) {
                int start = pos;
                while (pos < len && std::isdigit((unsigned char)raw[pos])) ++pos;
                bool is_float = (pos < len && raw[pos] == '.' &&
                                 pos + 1 < len && std::isdigit((unsigned char)raw[pos + 1]));
                if (is_float) {
                    ++pos; // consume '.'
                    while (pos < len && std::isdigit((unsigned char)raw[pos])) ++pos;
                }
                tokens.push_back({TT::NUMBER, raw.substr(start, pos - start), line_idx, start});
                continue;
            }

            // Word / keyword.
            if (std::isalpha((unsigned char)c) || c == '_') {
                int start = pos;
                while (pos < len && (std::isalnum((unsigned char)raw[pos]) || raw[pos] == '_')) ++pos;
                std::string word = raw.substr(start, pos - start);
                TT kw = tama_keyword_type(word);
                tokens.push_back({kw, word, line_idx, start});
                continue;
            }

            // Two-character operators.
            if (pos + 1 < len) {
                char n = raw[pos + 1];
                if ((c == '=' && n == '=') || (c == '!' && n == '=') ||
                    (c == '<' && n == '=') || (c == '>' && n == '=') ||
                    (c == '&' && n == '&') || (c == '|' && n == '|')) {
                    tokens.push_back({TT::OP, std::string(1, c) + n, line_idx, pos});
                    pos += 2;
                    continue;
                }
            }

            // Single-character operators.
            if (c == '*' || c == '/' || c == '+' || c == '-' || c == '<' ||
                c == '>' || c == '!' || c == '&' || c == '|' || c == '=' || c == '%') {
                tokens.push_back({TT::OP, std::string(1, c), line_idx, pos});
                ++pos; continue;
            }

            if (c == '(') { tokens.push_back({TT::LPAREN, "(", line_idx, pos}); ++pos; continue; }
            if (c == ')') { tokens.push_back({TT::RPAREN, ")", line_idx, pos}); ++pos; continue; }
            if (c == ',') { tokens.push_back({TT::COMMA,  ",", line_idx, pos}); ++pos; continue; }

            // Unrecognised character.
            tokens.push_back({TT::ERROR, std::string(1, c), line_idx, pos});
            ++pos;
        }

        tokens.push_back({TT::NEWLINE, "", line_idx, len});
    }

    // Close remaining indent levels.
    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        tokens.push_back({TT::DEDENT, "", (int)lines.size() - 1, 0});
    }

    tokens.push_back({TT::EOF_, "", (int)lines.size(), 0});
    return tokens;
}
