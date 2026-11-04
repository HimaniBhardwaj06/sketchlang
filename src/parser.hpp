#pragma once
#include "ast.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace sk {

struct ParseResult {
    Program program;
    std::vector<std::string> errors;  // each looks like "3:7: message"
    bool ok() const { return errors.empty(); }
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
    ParseResult parse();

private:
    struct ParseError {};

    // token helpers
    const Token& peek(size_t ahead = 0) const;
    bool check(TokenType t) const { return peek().type == t; }
    Token advance();
    bool match(TokenType t);
    Token consume(TokenType t, const std::string& message);
    ParseError error(const Token& at, const std::string& message);
    bool startsStatement(TokenType t) const;
    void synchronize();

    // statements
    StmtPtr declaration();
    StmtPtr statement();
    StmtPtr letStatement();
    StmtPtr assignStatement();
    StmtPtr repeatStatement();
    StmtPtr whileStatement();
    StmtPtr ifStatement();
    StmtPtr fnStatement();
    StmtPtr returnStatement();
    StmtPtr expressionStatement();
    Block block(const std::string& missingBraceMessage);

    // expressions (Pratt parser)
    ExprPtr expression(int minBp = 0);
    ExprPtr prefix();
    ExprPtr postfix(ExprPtr left);
    ExprPtr primary();

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::vector<std::string> errors_;
    int errorLine_ = 0;  // line of the most recent error, used by synchronize()
};

ParseResult parseSource(std::string_view source);

} // namespace sk
