#pragma once
#include "token.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace sk {

class Lexer {
public:
    explicit Lexer(std::string_view source) : src_(source) {}

    // Scans the whole input. The last token is always Eof.
    std::vector<Token> tokenize();

private:
    Token next();
    Token make(TokenType type, size_t start, int line, int col) const;
    Token error(const std::string& msg, int line, int col) const;

    char peek(size_t ahead = 0) const;
    char advance();
    bool match(char expected);
    bool atEnd() const { return pos_ >= src_.size(); }
    void skipWhitespaceAndComments();

    Token number(size_t start, int line, int col);
    Token identifier(size_t start, int line, int col);
    Token string(size_t start, int line, int col);

    std::string_view src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
};

} // namespace sk
