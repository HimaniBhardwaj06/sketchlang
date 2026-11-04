#pragma once
#include "ast.hpp"
#include <string>

namespace sk {

// Prints the AST as an S-expression, e.g. (let x (+ 1 (* 2 3))).
// Used by tests and the CLI now; the browser playground will use JSON later.
std::string toSexpr(const Expr& e);
std::string toSexpr(const Stmt& s);
std::string toSexpr(const Program& p);  // one statement per line

} // namespace sk
