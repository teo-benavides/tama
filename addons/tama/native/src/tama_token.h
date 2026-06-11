#pragma once
#include <string>
#include <unordered_map>

// Token type discriminant.
enum class TT {
    NUMBER, WORD, STRING, LPAREN, RPAREN, OP, COMMA,
    NEWLINE, INDENT, DEDENT, EOF_, ERROR,
    // Keywords
    KW_MAIN, KW_FIRE, KW_ACT, KW_BULLET, KW_REPEAT, KW_WAIT, KW_WAITF,
    KW_VANISH, KW_CHDIR, KW_CHSPD, KW_DIR, KW_SPEED, KW_OFFSET, KW_ACCEL,
    KW_AIM, KW_ABS, KW_REL, KW_SEQ, KW_OVER, KW_X, KW_Y, KW_TYPE, KW_EMITTER,
    KW_ASYNC, KW_EXPORT, KW_INCLUDE, KW_POS, KW_CHPOS, KW_REPEATF, KW_MVMT,
    KW_VAR, KW_IF, KW_ELIF, KW_ELSE, KW_TRUE, KW_FALSE, KW_WHILE, KW_BREAK,
    KW_BOUNCES, KW_CHROTSPD, KW_ROTSPD,
};

struct TamaToken {
    TT          type  = TT::EOF_;
    std::string value;
    int         line  = 0;
    int         col   = 0;
};

// Returns keyword TT if word is a keyword, else TT::WORD.
TT tama_keyword_type(const std::string &word);
// Returns a short name for debug printing.
const char *tama_token_name(TT t);
