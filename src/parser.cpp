#include "parser.hpp"
#include "lexer.hpp"
#include <algorithm>

namespace sk {

namespace {

// Binding power of each infix operator. Higher binds tighter. 0 = not an operator.
int infixBp(TokenType t) {
    switch (t) {
        case TokenType::Eq:
        case TokenType::NotEq:     return 1;
        case TokenType::Less:
        case TokenType::LessEq:
        case TokenType::Greater:
        case TokenType::GreaterEq: return 2;
        case TokenType::Plus:
        case TokenType::Minus:     return 3;
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:   return 4;
        default:                   return 0;
    }
}

constexpr int kUnaryBp = 5;  // tighter than every binary operator

ExprPtr makeExpr(decltype(Expr::node) node, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->node = std::move(node);
    e->line = line;
    e->col = col;
    return e;
}

StmtPtr makeStmt(decltype(Stmt::node) node, int line, int col) {
    auto s = std::make_unique<Stmt>();
    s->node = std::move(node);
    s->line = line;
    s->col = col;
    return s;
}

} // namespace

// ---------------------------------------------------------------- helpers

const Token& Parser::peek(size_t ahead) const {
    size_t i = std::min(pos_ + ahead, tokens_.size() - 1);
    return tokens_[i];
}

Token Parser::advance() {
    Token t = tokens_[pos_];
    if (t.type != TokenType::Eof) pos_++;
    return t;
}

bool Parser::match(TokenType t) {
    if (!check(t)) return false;
    advance();
    return true;
}

Parser::ParseError Parser::error(const Token& at, const std::string& message) {
    std::string text = std::to_string(at.line) + ":" + std::to_string(at.col) + ": ";
    if (at.type == TokenType::Error) {
        text += at.lexeme;  // the lexer already explained what is wrong
    } else {
        text += message + ", found ";
        text += at.type == TokenType::Eof ? "end of input" : "'" + at.lexeme + "'";
    }
    errors_.push_back(text);
    errorLine_ = at.line;
    return ParseError{};
}

Token Parser::consume(TokenType t, const std::string& message) {
    if (check(t)) return advance();
    throw error(peek(), message);
}

bool Parser::startsStatement(TokenType t) const {
    switch (t) {
        case TokenType::Let:
        case TokenType::Repeat:
        case TokenType::While:
        case TokenType::If:
        case TokenType::Fn:
        case TokenType::Return: return true;
        default:                return false;
    }
}

// After an error, skip the rest of the broken statement. We resume at the first
// token that is a statement keyword, a closing brace, or simply on a new line.
void Parser::synchronize() {
    while (!check(TokenType::Eof)) {
        if (startsStatement(peek().type) || check(TokenType::RBrace)) return;
        if (peek().line > errorLine_) return;
        advance();
    }
}

// ------------------------------------------------------------- statements

ParseResult Parser::parse() {
    ParseResult result;
    while (!check(TokenType::Eof)) {
        if (StmtPtr s = declaration()) result.program.push_back(std::move(s));
    }
    result.errors = std::move(errors_);
    return result;
}

StmtPtr Parser::declaration() {
    size_t before = pos_;
    try {
        return statement();
    } catch (const ParseError&) {
        if (pos_ == before) advance();  // guarantee progress
        synchronize();
        return nullptr;
    }
}

StmtPtr Parser::statement() {
    switch (peek().type) {
        case TokenType::Let:    return letStatement();
        case TokenType::Repeat: return repeatStatement();
        case TokenType::While:  return whileStatement();
        case TokenType::If:     return ifStatement();
        case TokenType::Fn:     return fnStatement();
        case TokenType::Return: return returnStatement();
        case TokenType::Identifier:
            if (peek(1).type == TokenType::Assign) return assignStatement();
            return expressionStatement();
        default:
            return expressionStatement();
    }
}

Block Parser::block(const std::string& missingBraceMessage) {
    consume(TokenType::LBrace, missingBraceMessage);
    Block body;
    while (!check(TokenType::RBrace) && !check(TokenType::Eof)) {
        if (StmtPtr s = declaration()) body.push_back(std::move(s));
    }
    consume(TokenType::RBrace, "Expected '}' to close the block");
    return body;
}

StmtPtr Parser::letStatement() {
    Token kw = advance();
    Token name = consume(TokenType::Identifier, "Expected a variable name after 'let'");
    consume(TokenType::Assign, "Expected '=' after the variable name");
    ExprPtr init = expression();
    return makeStmt(LetStmt{name.lexeme, std::move(init)}, kw.line, kw.col);
}

StmtPtr Parser::assignStatement() {
    Token name = advance();
    advance();  // '='
    ExprPtr value = expression();
    return makeStmt(AssignStmt{name.lexeme, std::move(value)}, name.line, name.col);
}

StmtPtr Parser::repeatStatement() {
    Token kw = advance();
    ExprPtr count = expression();
    Block body = block("Expected '{' after the repeat count");
    return makeStmt(RepeatStmt{std::move(count), std::move(body)}, kw.line, kw.col);
}

StmtPtr Parser::whileStatement() {
    Token kw = advance();
    ExprPtr cond = expression();
    Block body = block("Expected '{' after the while condition");
    return makeStmt(WhileStmt{std::move(cond), std::move(body)}, kw.line, kw.col);
}

StmtPtr Parser::ifStatement() {
    Token kw = advance();
    IfStmt node;
    node.cond = expression();
    node.thenBlock = block("Expected '{' after the if condition");
    if (match(TokenType::Else)) {
        node.hasElse = true;
        if (check(TokenType::If)) {
            node.elseBlock.push_back(ifStatement());  // else if
        } else {
            node.elseBlock = block("Expected '{' after 'else'");
        }
    }
    return makeStmt(std::move(node), kw.line, kw.col);
}

StmtPtr Parser::fnStatement() {
    Token kw = advance();
    FnDecl fn;
    fn.name = consume(TokenType::Identifier, "Expected a function name after 'fn'").lexeme;
    consume(TokenType::LParen, "Expected '(' after the function name");
    if (!check(TokenType::RParen)) {
        do {
            fn.params.push_back(consume(TokenType::Identifier, "Expected a parameter name").lexeme);
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expected ')' after the parameters");
    fn.body = block("Expected '{' before the function body");
    return makeStmt(std::move(fn), kw.line, kw.col);
}

StmtPtr Parser::returnStatement() {
    Token kw = advance();
    ExprPtr value;
    if (!check(TokenType::RBrace) && !check(TokenType::Eof) && !startsStatement(peek().type)) {
        value = expression();
    }
    return makeStmt(ReturnStmt{std::move(value)}, kw.line, kw.col);
}

StmtPtr Parser::expressionStatement() {
    Token first = peek();
    ExprPtr e = expression();
    return makeStmt(ExprStmt{std::move(e)}, first.line, first.col);
}

// ------------------------------------------------------------ expressions
//
// Pratt parsing: parse a prefix expression, then keep absorbing infix
// operators as long as they bind tighter than `minBp`.

ExprPtr Parser::expression(int minBp) {
    ExprPtr left = prefix();
    for (;;) {
        int bp = infixBp(peek().type);
        if (bp == 0 || bp <= minBp) break;  // '<=' makes operators left-associative
        Token op = advance();
        ExprPtr right = expression(bp);
        int line = left->line, col = left->col;
        left = makeExpr(Binary{op.type, std::move(left), std::move(right)}, line, col);
    }
    return left;
}

ExprPtr Parser::prefix() {
    if (check(TokenType::Minus) || check(TokenType::Bang)) {
        Token op = advance();
        ExprPtr operand = expression(kUnaryBp);
        return makeExpr(Unary{op.type, std::move(operand)}, op.line, op.col);
    }
    return postfix(primary());
}

ExprPtr Parser::postfix(ExprPtr left) {
    while (check(TokenType::LParen)) {
        Token paren = advance();
        auto* var = std::get_if<Variable>(&left->node);
        if (!var) {
            errors_.push_back(std::to_string(paren.line) + ":" + std::to_string(paren.col) +
                              ": Only named functions can be called");
            errorLine_ = paren.line;
            throw ParseError{};
        }
        Call call{var->name, {}};
        int line = left->line, col = left->col;
        if (!check(TokenType::RParen)) {
            do {
                call.args.push_back(expression());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RParen, "Expected ')' after the arguments");
        left = makeExpr(std::move(call), line, col);
    }
    return left;
}

ExprPtr Parser::primary() {
    Token t = peek();
    switch (t.type) {
        case TokenType::Number:
            advance();
            return makeExpr(NumberLit{std::stod(t.lexeme)}, t.line, t.col);
        case TokenType::String:
            advance();
            return makeExpr(StringLit{t.lexeme.substr(1, t.lexeme.size() - 2)}, t.line, t.col);
        case TokenType::True:
            advance();
            return makeExpr(BoolLit{true}, t.line, t.col);
        case TokenType::False:
            advance();
            return makeExpr(BoolLit{false}, t.line, t.col);
        case TokenType::Identifier:
            advance();
            return makeExpr(Variable{t.lexeme}, t.line, t.col);
        case TokenType::LParen: {
            advance();
            ExprPtr inner = expression();
            consume(TokenType::RParen, "Expected ')' after the expression");
            return inner;
        }
        default:
            throw error(t, "Expected an expression");
    }
}

ParseResult parseSource(std::string_view source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

} // namespace sk
