#pragma once
#include "token.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace sk {

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;
using Program = Block;

// ---------- expressions ----------
struct NumberLit { double value; };
struct StringLit { std::string value; };
struct BoolLit   { bool value; };
struct Variable  { std::string name; };
struct Unary     { TokenType op; ExprPtr operand; };
struct Binary    { TokenType op; ExprPtr left; ExprPtr right; };
struct Call      { std::string callee; std::vector<ExprPtr> args; };

struct Expr {
    std::variant<NumberLit, StringLit, BoolLit, Variable, Unary, Binary, Call> node;
    int line = 0;
    int col = 0;
};

// ---------- statements ----------
struct LetStmt    { std::string name; ExprPtr init; };
struct AssignStmt { std::string name; ExprPtr value; };
struct ExprStmt   { ExprPtr expr; };
struct RepeatStmt { ExprPtr count; Block body; };
struct WhileStmt  { ExprPtr cond; Block body; };
struct IfStmt     { ExprPtr cond; Block thenBlock; Block elseBlock; bool hasElse = false; };
struct FnDecl     { std::string name; std::vector<std::string> params; Block body; };
struct ReturnStmt { ExprPtr value; };  // value is null for a bare `return`

struct Stmt {
    std::variant<LetStmt, AssignStmt, ExprStmt, RepeatStmt, WhileStmt,
                 IfStmt, FnDecl, ReturnStmt> node;
    int line = 0;
    int col = 0;
};

} // namespace sk
