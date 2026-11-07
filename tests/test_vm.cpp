#include <catch2/catch_test_macros.hpp>
#include "compiler.hpp"
#include "parser.hpp"
#include "vm.hpp"

namespace {

struct Outcome {
    std::string out;       // print() lines joined with '\n'
    std::string error;     // runtime error message, empty if none
    int errorLine = 0;
    size_t draws = 0;
    sk::DrawCmd firstDraw{};
};

Outcome run(const std::string& src, uint64_t maxSteps = 10'000'000) {
    auto parsed = sk::parseSource(src);
    REQUIRE(parsed.ok());
    auto compiled = sk::compile(parsed.program);
    REQUIRE(compiled.ok());
    sk::VM vm(compiled.module);
    vm.run(maxSteps);
    Outcome o;
    for (size_t i = 0; i < vm.output().size(); i++) o.out += (i ? "\n" : "") + vm.output()[i];
    if (vm.status() == sk::Status::Error) {
        o.error = vm.error();
        o.errorLine = vm.errorLine();
    }
    o.draws = vm.draws().size();
    if (!vm.draws().empty()) o.firstDraw = vm.draws()[0];
    return o;
}

std::vector<std::string> compileErrors(const std::string& src) {
    auto parsed = sk::parseSource(src);
    REQUIRE(parsed.ok());
    return sk::compile(parsed.program).errors;
}

bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

} // namespace

TEST_CASE("arithmetic and precedence") {
    REQUIRE(run("print(1 + 2 * 3)").out == "7");
    REQUIRE(run("print((1 + 2) * 3)").out == "9");
    REQUIRE(run("print(10 / 4)").out == "2.5");
    REQUIRE(run("print(7 % 3)").out == "1");
    REQUIRE(run("print(-3 + 5)").out == "2");
}

TEST_CASE("comparison, equality and logic") {
    REQUIRE(run("print(1 < 2)").out == "true");
    REQUIRE(run("print(2 <= 1)").out == "false");
    REQUIRE(run("print(1 == 1)").out == "true");
    REQUIRE(run("print(1 == true)").out == "false");
    REQUIRE(run("print(1 != 2)").out == "true");
    REQUIRE(run("print(!(1 < 2))").out == "false");
    REQUIRE(run("print(\"a\" == \"a\")").out == "true");
    REQUIRE(run("print(\"a\" == \"b\")").out == "false");
}

TEST_CASE("print formats") {
    REQUIRE(run("print(\"hello\")").out == "hello");
    REQUIRE(run("print(true)").out == "true");
    REQUIRE(run("print(1.5)").out == "1.5");
    REQUIRE(run("fn f() { }\nprint(f())").out == "nil");
}

TEST_CASE("global variables and assignment") {
    REQUIRE(run("let x = 5\nx = x * 2\nprint(x)").out == "10");
}

TEST_CASE("repeat and nested repeat") {
    REQUIRE(run("let t = 0\nrepeat 5 { t = t + 2 }\nprint(t)").out == "10");
    REQUIRE(run("let t = 0\nrepeat 3 { repeat 4 { t = t + 1 } }\nprint(t)").out == "12");
    REQUIRE(run("let t = 0\nrepeat 0 { t = 1 }\nprint(t)").out == "0");
}

TEST_CASE("vm: while loop") {
    REQUIRE(run("let i = 0\nwhile i < 4 { i = i + 1 }\nprint(i)").out == "4");
}

TEST_CASE("vm: if, else and else if") {
    REQUIRE(run("if 1 < 2 { print(\"a\") } else { print(\"b\") }").out == "a");
    REQUIRE(run("if 1 > 2 { print(\"a\") } else { print(\"b\") }").out == "b");
    REQUIRE(run("let x = 5\nif x < 3 { print(1) } else if x < 10 { print(2) } else { print(3) }").out == "2");
    REQUIRE(run("if false { print(1) }\nprint(9)").out == "9");
}

TEST_CASE("functions, arguments and return values") {
    REQUIRE(run("fn add(a, b) { return a + b }\nprint(add(2, 3))").out == "5");
    REQUIRE(run("fn sq(x) { return x * x }\nprint(sq(sq(3)))").out == "81");
}

TEST_CASE("recursion") {
    REQUIRE(run("fn fib(n) {\n if n < 2 { return n }\n return fib(n - 1) + fib(n - 2)\n}\nprint(fib(15))").out == "610");
}

TEST_CASE("functions can be called before they are defined") {
    REQUIRE(run("print(double(4))\nfn double(x) { return x * 2 }").out == "8");
}

TEST_CASE("functions can read and write globals") {
    REQUIRE(run("let k = 10\nfn addk(x) { return x + k }\nprint(addk(5))").out == "15");
    REQUIRE(run("let c = 0\nfn inc() { c = c + 1 }\ninc()\ninc()\nprint(c)").out == "2");
}

TEST_CASE("local variables and block scopes") {
    REQUIRE(run("fn f() {\n let a = 1\n repeat 2 {\n  let b = a + 1\n  a = b\n }\n return a\n}\nprint(f())").out == "3");
}

TEST_CASE("early return from inside a loop") {
    REQUIRE(run("fn find() {\n let i = 0\n while true {\n  if i == 5 { return i }\n  i = i + 1\n }\n}\nprint(find())").out == "5");
}

TEST_CASE("native math functions") {
    REQUIRE(run("print(sqrt(16))").out == "4");
    REQUIRE(run("print(abs(-2.5))").out == "2.5");
    REQUIRE(run("print(floor(3.9))").out == "3");
    REQUIRE(run("print(sin(0))").out == "0");
    REQUIRE(run("print(cos(0))").out == "1");
    REQUIRE(run("print(random() < 1)").out == "true");
}

TEST_CASE("drawing commands are recorded with the current colour") {
    auto o = run("color(255, 0, 128)\ncircle(10, 20, 5)\nrect(1, 2, 3, 4)");
    REQUIRE(o.draws == 2);
    REQUIRE(o.firstDraw.kind == sk::DrawCmd::Kind::Circle);
    REQUIRE(o.firstDraw.a == 10);
    REQUIRE(o.firstDraw.b == 20);
    REQUIRE(o.firstDraw.c == 5);
    REQUIRE(o.firstDraw.r == 255);
    REQUIRE(o.firstDraw.g == 0);
    REQUIRE(o.firstDraw.bl == 128);
}

TEST_CASE("runtime error: division by zero reports the line") {
    auto o = run("print(1)\nprint(1 / 0)");
    REQUIRE(has(o.error, "Division by zero"));
    REQUIRE(o.errorLine == 2);
    REQUIRE(o.out == "1");  // output before the error is kept
}

TEST_CASE("runtime error: type errors") {
    REQUIRE(has(run("print(1 + true)").error, "Operands must be numbers"));
    REQUIRE(has(run("if 1 { print(1) }").error, "Condition must be a boolean"));
    REQUIRE(has(run("print(!5)").error, "boolean"));
    REQUIRE(has(run("circle(true, 1, 1)").error, "circle() expects a number"));
    REQUIRE(has(run("print(sqrt(-1))").error, "negative"));
}

TEST_CASE("runtime error: global used before it is defined") {
    auto o = run("fn show() { print(late) }\nshow()\nlet late = 1");
    REQUIRE(has(o.error, "Undefined variable 'late'"));
}

TEST_CASE("runaway recursion is caught") {
    REQUIRE(has(run("fn f() { f() }\nf()").error, "Stack overflow"));
}

TEST_CASE("infinite loops hit the instruction limit") {
    REQUIRE(has(run("while true { }", 1000).error, "Instruction limit"));
}

TEST_CASE("compile errors") {
    REQUIRE(has(compileErrors("print(x)")[0], "Undefined variable 'x'"));
    REQUIRE(has(compileErrors("x = 1")[0], "Undefined variable 'x'"));
    REQUIRE(has(compileErrors("foo(1)")[0], "Undefined function 'foo'"));
    REQUIRE(has(compileErrors("circle(1, 2)")[0], "expects 3 argument"));
    REQUIRE(has(compileErrors("fn f(a) { }\nf(1, 2)")[0], "expects 1 argument"));
    REQUIRE(has(compileErrors("return 1")[0], "top-level"));
    REQUIRE(has(compileErrors("repeat 1 { fn g() { } }")[0], "top level"));
    REQUIRE(has(compileErrors("repeat 1 {\n let a = 1\n let a = 2\n}")[0], "already declared"));
    REQUIRE(has(compileErrors("fn f() { }\nfn f() { }")[0], "already defined"));
    REQUIRE(has(compileErrors("fn circle() { }")[0], "built-in"));
    REQUIRE(has(compileErrors("fn f(a, a) { }")[0], "Duplicate parameter"));
}

TEST_CASE("step-by-step execution exposes the stack") {
    auto parsed = sk::parseSource("let x = 1");
    auto compiled = sk::compile(parsed.program);
    sk::VM vm(compiled.module);
    REQUIRE(vm.status() == sk::Status::Running);
    vm.step();                              // CONST 1
    REQUIRE(vm.stack().size() == 1);
    REQUIRE(vm.stack()[0].n == 1);
    vm.step();                              // DEF_GLOBAL x
    REQUIRE(vm.stack().empty());
    REQUIRE(vm.globalDefined(0));
    while (vm.status() == sk::Status::Running) vm.step();
    REQUIRE(vm.status() == sk::Status::Finished);
    REQUIRE(vm.steps() == 4);               // CONST, DEF_GLOBAL, NIL, RETURN
}

TEST_CASE("disassembler names instructions and operands") {
    auto parsed = sk::parseSource("fn f() { }\nprint(1)\nf()");
    auto compiled = sk::compile(parsed.program);
    std::string text = sk::disassemble(compiled.module);
    REQUIRE(has(text, "== <main> =="));
    REQUIRE(has(text, "== f =="));
    REQUIRE(has(text, "CALL_NATIVE"));
    REQUIRE(has(text, "print"));
    REQUIRE(has(text, "CALL "));
}

TEST_CASE("runaway drawing and printing are capped") {
    REQUIRE(has(run("repeat 200000 { circle(1, 1, 1) }", 100'000'000).error, "Too many shapes"));
    REQUIRE(has(run("repeat 20000 { print(1) }", 100'000'000).error, "Too much output"));
}
