#pragma once
#include "tama_token.h"
#include <string>
#include <vector>

// Pure C++ lexer — no Godot binding needed (used only from TamaParserCpp /
// TamaScriptRepository). Matches the behaviour of tama_lexer.gd exactly.
class TamaLexerCpp {
public:
    std::vector<TamaToken> tokenize(const std::string &source) const;
};
