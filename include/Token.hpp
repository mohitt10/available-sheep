#pragma once
#include <cstdint>
#include <string>
#include "shared_types.hpp"
 
enum class TokenKind {
    Int, Ident, KwInt, KwPrint, KwInput,
    Plus, Minus, Star, Slash, Percent,
    Lt, Le, Gt, Ge, EqEq, Ne, Not,
    Assign, LParen, RParen, Semi,
    End
};
 
struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    std::int32_t value = 0;
    SymbolId symbol = 0;
    int line = 1;
    int column = 1;
};
 