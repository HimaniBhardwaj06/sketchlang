#include <catch2/catch_test_macros.hpp>
#include "parser.hpp"
#include "printer.hpp"

static std::string ast(const std::string& src) {
    auto r = sk::parseSource(src);
    REQUIRE(r.ok());
    return sk::toSexpr(r.program);
}

static std::vector<std::string> errs(const std::string& src) {
    return sk::parseSource(src).errors;
}

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

TEST_CASE("multiplication binds tighter than addition") {
    REQUIRE(ast("1 + 2 * 3") == "(expr (+ 1 (* 2 3)))");
    REQUIRE(ast("1 * 2 + 3") == "(expr (+ (* 1 2) 3))");
}

TEST_CASE("operators are left-associative") {
    REQUIRE(ast("1 - 2 - 3") == "(expr (- (- 1 2) 3))");
    REQUIRE(ast("8 / 4 / 2") == "(expr (/ (/ 8 4) 2))");
}

TEST_CASE("parentheses override precedence") {
    REQUIRE(ast("(1 + 2) * 3") == "(expr (* (+ 1 2) 3))");
}

TEST_CASE("unary operators bind tighter than binary ones") {
    REQUIRE(ast("-2 * 3") == "(expr (* (- 2) 3))");
    REQUIRE(ast("- -x") == "(expr (- (- x)))");
    REQUIRE(ast("!a == b") == "(expr (== (! a) b))");
}

TEST_CASE("comparison binds looser than arithmetic") {
    REQUIRE(ast("1 + 2 < 4") == "(expr (< (+ 1 2) 4))");
    REQUIRE(ast("a < b == c < d") == "(expr (== (< a b) (< c d)))");
}

TEST_CASE("let and assignment") {
    REQUIRE(ast("let x = 10") == "(let x 10)");
    REQUIRE(ast("x = x + 20") == "(assign x (+ x 20))");
}

TEST_CASE("literals") {
    REQUIRE(ast("let s = \"hi\"") == "(let s \"hi\")");
    REQUIRE(ast("let t = true") == "(let t true)");
    REQUIRE(ast("let p = 3.14") == "(let p 3.14)");
}

TEST_CASE("function calls") {
    REQUIRE(ast("circle(x, 40, 8)") == "(expr (call circle x 40 8))");
    REQUIRE(ast("f()") == "(expr (call f))");
    REQUIRE(ast("f(g(1), 2 + 3)") == "(expr (call f (call g 1) (+ 2 3)))");
}

TEST_CASE("repeat loop with a body") {
    REQUIRE(ast("let x = 10\nrepeat 4 {\n circle(x, 40, 8)\n x = x + 20\n}") ==
            "(let x 10)\n(repeat 4 (expr (call circle x 40 8)) (assign x (+ x 20)))");
}

TEST_CASE("while loop") {
    REQUIRE(ast("while x < 3 { x = x + 1 }") == "(while (< x 3) (assign x (+ x 1)))");
}

TEST_CASE("if, else, and else if") {
    REQUIRE(ast("if a < b { f() } else { g() }") ==
            "(if (< a b) (then (expr (call f))) (else (expr (call g))))");
    REQUIRE(ast("if a { f() }") == "(if a (then (expr (call f))))");
    REQUIRE(ast("if a { f() } else if b { g() }") ==
            "(if a (then (expr (call f))) (else (if b (then (expr (call g))))))");
}

TEST_CASE("function declarations and return") {
    REQUIRE(ast("fn add(a, b) { return a + b }") == "(fn add (a b) (return (+ a b)))");
    REQUIRE(ast("fn f() { return }") == "(fn f () (return))");
}

TEST_CASE("missing variable name reports line and column") {
    auto e = errs("let = 5");
    REQUIRE(e.size() == 1);
    REQUIRE(e[0].rfind("1:5", 0) == 0);
    REQUIRE(contains(e[0], "variable name"));
}

TEST_CASE("unclosed block is reported at end of input") {
    auto e = errs("repeat 3 { f()");
    REQUIRE(!e.empty());
    REQUIRE(contains(e[0], "'}'"));
    REQUIRE(contains(e[0], "end of input"));
}

TEST_CASE("lexer errors surface as parse errors") {
    auto e = errs("let x = @");
    REQUIRE(e.size() == 1);
    REQUIRE(contains(e[0], "Unexpected character"));
}

TEST_CASE("only named functions can be called") {
    REQUIRE(!errs("1(2)").empty());
}

TEST_CASE("parser recovers and keeps going after an error") {
    auto r = sk::parseSource("let = 1\nlet y = 2\nlet = 3");
    REQUIRE(r.errors.size() == 2);
    REQUIRE(r.program.size() == 1);
    REQUIRE(sk::toSexpr(r.program) == "(let y 2)");
}

TEST_CASE("error inside a block does not swallow the next statement") {
    auto r = sk::parseSource("repeat 2 { let = 1\n f() }\ng()");
    REQUIRE(r.errors.size() == 1);
    REQUIRE(sk::toSexpr(r.program) == "(repeat 2 (expr (call f)))\n(expr (call g))");
}

TEST_CASE("garbage input terminates") {
    auto r = sk::parseSource("} ) ( , + } let");
    REQUIRE(!r.ok());
}
