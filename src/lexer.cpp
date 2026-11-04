#include "lexer.hpp"
#include <cctype>
#include <unordered_map>

namespace sk {

const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::Number: return "Number";
        case TokenType::Identifier: return "Identifier";
        case TokenType::String: return "String";
        case TokenType::Let: return "Let";
        case TokenType::Repeat: return "Repeat";
        case TokenType::While: return "While";
        case TokenType::If: return "If";
        case TokenType::Else: return "Else";
        case TokenType::Fn: return "Fn";
        case TokenType::Return: return "Return";
        case TokenType::True: return "True";
        case TokenType::False: return "False";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Star: return "Star";
        case TokenType::Slash: return "Slash";
        case TokenType::Percent: return "Percent";
        case TokenType::Assign: return "Assign";
        case TokenType::Eq: return "Eq";
        case TokenType::NotEq: return "NotEq";
        case TokenType::Less: return "Less";
        case TokenType::LessEq: return "LessEq";
        case TokenType::Greater: return "Greater";
        case TokenType::GreaterEq: return "GreaterEq";
        case TokenType::Bang: return "Bang";
        case TokenType::LParen: return "LParen";
        case TokenType::RParen: return "RParen";
        case TokenType::LBrace: return "LBrace";
        case TokenType::RBrace: return "RBrace";
        case TokenType::Comma: return "Comma";
        case TokenType::Eof: return "Eof";
        case TokenType::Error: return "Error";
    }
    return "?";
}

static const std::unordered_map<std::string, TokenType>& keywords() {
    static const std::unordered_map<std::string, TokenType> k = {
        {"let", TokenType::Let},       {"repeat", TokenType::Repeat},
        {"while", TokenType::While},   {"if", TokenType::If},
        {"else", TokenType::Else},     {"fn", TokenType::Fn},
        {"return", TokenType::Return}, {"true", TokenType::True},
        {"false", TokenType::False},
    };
    return k;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    for (;;) {
        Token t = next();
        out.push_back(t);
        if (t.type == TokenType::Eof) break;
    }
    return out;
}

char Lexer::peek(size_t ahead) const {
    return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; } else { col_++; }
    return c;
}

bool Lexer::match(char expected) {
    if (atEnd() || src_[pos_] != expected) return false;
    advance();
    return true;
}

Token Lexer::make(TokenType type, size_t start, int line, int col) const {
    return Token{type, std::string(src_.substr(start, pos_ - start)), line, col};
}

Token Lexer::error(const std::string& msg, int line, int col) const {
    return Token{TokenType::Error, msg, line, col};
}

void Lexer::skipWhitespaceAndComments() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!atEnd() && peek() != '\n') advance();
        } else {
            return;
        }
    }
}

Token Lexer::next() {
    skipWhitespaceAndComments();
    int line = line_, col = col_;
    size_t start = pos_;
    if (atEnd()) return Token{TokenType::Eof, "", line, col};

    char c = advance();
    if (std::isdigit(static_cast<unsigned char>(c))) return number(start, line, col);
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return identifier(start, line, col);

    switch (c) {
        case '+': return make(TokenType::Plus, start, line, col);
        case '-': return make(TokenType::Minus, start, line, col);
        case '*': return make(TokenType::Star, start, line, col);
        case '/': return make(TokenType::Slash, start, line, col);
        case '%': return make(TokenType::Percent, start, line, col);
        case '(': return make(TokenType::LParen, start, line, col);
        case ')': return make(TokenType::RParen, start, line, col);
        case '{': return make(TokenType::LBrace, start, line, col);
        case '}': return make(TokenType::RBrace, start, line, col);
        case ',': return make(TokenType::Comma, start, line, col);
        case '=': return make(match('=') ? TokenType::Eq : TokenType::Assign, start, line, col);
        case '!': return make(match('=') ? TokenType::NotEq : TokenType::Bang, start, line, col);
        case '<': return make(match('=') ? TokenType::LessEq : TokenType::Less, start, line, col);
        case '>': return make(match('=') ? TokenType::GreaterEq : TokenType::Greater, start, line, col);
        case '"': return string(start, line, col);
    }
    return error(std::string("Unexpected character '") + c + "'", line, col);
}

Token Lexer::number(size_t start, int line, int col) {
    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    return make(TokenType::Number, start, line, col);
}

Token Lexer::identifier(size_t start, int line, int col) {
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
    Token t = make(TokenType::Identifier, start, line, col);
    auto it = keywords().find(t.lexeme);
    if (it != keywords().end()) t.type = it->second;
    return t;
}

Token Lexer::string(size_t start, int line, int col) {
    while (!atEnd() && peek() != '"' && peek() != '\n') advance();
    if (atEnd() || peek() != '"') return error("Unterminated string", line, col);
    advance();
    return make(TokenType::String, start, line, col);
}

} // namespace sk
