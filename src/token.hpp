#pragma once
#include <string>

namespace sk {

enum class TokenType {
    // literals
    Number, Identifier, String,
    // keywords
    Let, Repeat, While, If, Else, Fn, Return, True, False,
    // operators
    Plus, Minus, Star, Slash, Percent,
    Assign, Eq, NotEq, Less, LessEq, Greater, GreaterEq, Bang,
    // punctuation
    LParen, RParen, LBrace, RBrace, Comma,
    // special
    Eof, Error
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int col;
};

const char* tokenTypeName(TokenType t);

} // namespace sk
