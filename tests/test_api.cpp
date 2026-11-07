#include <catch2/catch_test_macros.hpp>
#include "api.hpp"
#include <cctype>
#include <string>

namespace {

// A small strict JSON validator, so tests catch bad escaping or stray commas.
struct JsonCheck {
    const std::string& s;
    size_t i = 0;
    explicit JsonCheck(const std::string& text) : s(text) {}

    void ws() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++; }
    bool lit(const char* word) {
        size_t n = std::string(word).size();
        if (s.compare(i, n, word) != 0) return false;
        i += n;
        return true;
    }
    bool str() {
        if (i >= s.size() || s[i] != '"') return false;
        i++;
        while (i < s.size() && s[i] != '"') {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x20) return false;
            if (c == '\\') {
                i++;
                if (i >= s.size()) return false;
                char e = s[i];
                if (e == 'u') {
                    for (int k = 0; k < 4; k++) {
                        i++;
                        if (i >= s.size() || !std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
                    }
                } else if (std::string("\"\\/bfnrt").find(e) == std::string::npos) {
                    return false;
                }
            }
            i++;
        }
        if (i >= s.size()) return false;
        i++;
        return true;
    }
    bool num() {
        size_t start = i;
        if (i < s.size() && s[i] == '-') i++;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' ||
                                s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) i++;
        return i > start;
    }
    bool value() {
        ws();
        if (i >= s.size()) return false;
        char c = s[i];
        if (c == '{') {
            i++; ws();
            if (i < s.size() && s[i] == '}') { i++; return true; }
            for (;;) {
                ws();
                if (!str()) return false;
                ws();
                if (i >= s.size() || s[i] != ':') return false;
                i++;
                if (!value()) return false;
                ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == '}') { i++; return true; }
                return false;
            }
        }
        if (c == '[') {
            i++; ws();
            if (i < s.size() && s[i] == ']') { i++; return true; }
            for (;;) {
                if (!value()) return false;
                ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == ']') { i++; return true; }
                return false;
            }
        }
        if (c == '"') return str();
        if (c == 't') return lit("true");
        if (c == 'f') return lit("false");
        if (c == 'n') return lit("null");
        return num();
    }
    bool valid() {
        if (!value()) return false;
        ws();
        return i == s.size();
    }
};

bool validJson(const std::string& s) { return JsonCheck(s).valid(); }
bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

} // namespace

TEST_CASE("api: compile returns valid JSON with tokens, ast and bytecode") {
    std::string r = sk_compile("let x = 10\nrepeat 4 {\n circle(x, 40, 8)\n x = x + 20\n}");
    REQUIRE(validJson(r));
    REQUIRE(has(r, "\"ok\":true"));
    REQUIRE(has(r, "\"errors\":[]"));
    REQUIRE(has(r, "\"type\":\"Let\""));       // token and AST node
    REQUIRE(has(r, "\"type\":\"Repeat\""));
    REQUIRE(has(r, "\"op\":\"CALL_NATIVE\""));
    REQUIRE(has(r, "\"note\":\"circle (3 args)\""));
}

TEST_CASE("api: parse errors are structured and still return tokens") {
    std::string r = sk_compile("let = 5");
    REQUIRE(validJson(r));
    REQUIRE(has(r, "\"ok\":false"));
    REQUIRE(has(r, "\"stage\":\"parse\""));
    REQUIRE(has(r, "\"line\":1"));
    REQUIRE(has(r, "\"col\":5"));
    REQUIRE(has(r, "\"ast\":null"));
    REQUIRE(has(r, "\"bytecode\":null"));
    REQUIRE(has(r, "\"tokens\":[{"));
}

TEST_CASE("api: compile errors are reported with the compile stage") {
    std::string r = sk_compile("print(y)");
    REQUIRE(validJson(r));
    REQUIRE(has(r, "\"stage\":\"compile\""));
    REQUIRE(has(r, "Undefined variable 'y'"));
    REQUIRE(has(r, "\"bytecode\":null"));
}

TEST_CASE("api: awkward characters are escaped") {
    std::string r = sk_compile("print(\"quote \\\" and \\\\ and\ttab\")\nlet s = \"a\"");
    REQUIRE(validJson(r));
    r = sk_compile("let x = @\n\"unterminated");
    REQUIRE(validJson(r));
    r = sk_compile("print(\"caf\xc3\xa9 \xe2\x9c\x93\")");
    REQUIRE(validJson(r));
}

TEST_CASE("api: empty program and null source are fine") {
    REQUIRE(validJson(sk_compile("")));
    REQUIRE(has(sk_compile(""), "\"ok\":true"));
    REQUIRE(validJson(sk_compile(nullptr)));
}

TEST_CASE("api: state before any compile") {
    sk_compile("let = 1");  // failed compile leaves no VM
    std::string s = sk_state();
    REQUIRE(validJson(s));
    REQUIRE(has(s, "\"status\":\"none\""));
    REQUIRE(sk_step() == 2);
}

TEST_CASE("api: stepping exposes the instruction pointer, stack and globals") {
    sk_compile("let x = 7\nprint(x)");
    std::string s = sk_state();
    REQUIRE(validJson(s));
    REQUIRE(has(s, "\"status\":\"running\""));
    REQUIRE(has(s, "\"ip\":0"));
    REQUIRE(has(s, "\"line\":1"));
    REQUIRE(has(s, "\"defined\":false"));

    REQUIRE(sk_step() == 0);                    // CONST 7
    s = sk_state();
    REQUIRE(validJson(s));
    REQUIRE(has(s, "\"ip\":1"));
    REQUIRE(has(s, "\"stack\":[{\"t\":\"number\",\"v\":\"7\"}]"));

    REQUIRE(sk_step() == 0);                    // DEF_GLOBAL x
    s = sk_state();
    REQUIRE(has(s, "\"defined\":true"));
    REQUIRE(has(s, "\"v\":\"7\""));

    REQUIRE(sk_run(1000) == 1);                 // finish
    s = sk_state();
    REQUIRE(validJson(s));
    REQUIRE(has(s, "\"status\":\"finished\""));
    REQUIRE(has(s, "\"output\":[\"7\"]"));
    REQUIRE(has(s, "\"ip\":-1"));
}

TEST_CASE("api: runtime errors carry the line") {
    sk_compile("print(1)\nprint(1 / 0)");
    REQUIRE(sk_run(1000) == 2);
    std::string s = sk_state();
    REQUIRE(validJson(s));
    REQUIRE(has(s, "\"status\":\"error\""));
    REQUIRE(has(s, "Division by zero"));
    REQUIRE(has(s, "\"errorLine\":2"));
    REQUIRE(has(s, "\"line\":2"));
}

TEST_CASE("api: hitting the run limit is reported as an error (infinite-loop guard)") {
    sk_compile("let i = 0\nwhile i < 100 { i = i + 1 }");
    REQUIRE(sk_run(10) == 2);
    std::string s = sk_state();
    REQUIRE(validJson(s));
    REQUIRE(has(s, "Instruction limit exceeded"));
    sk_compile("let i = 0\nwhile i < 100 { i = i + 1 }");
    REQUIRE(sk_run(100000) == 1);
}

TEST_CASE("api: reset restarts the same program") {
    sk_compile("print(1)");
    REQUIRE(sk_run(100) == 1);
    REQUIRE(sk_reset() == 0);
    std::string s = sk_state();
    REQUIRE(has(s, "\"status\":\"running\""));
    REQUIRE(has(s, "\"output\":[]"));
    REQUIRE(sk_run(100) == 1);
    REQUIRE(has(sk_state(), "\"output\":[\"1\"]"));
}

TEST_CASE("api: draw commands are exported compactly") {
    sk_compile("color(255, 0, 10)\ncircle(10, 20, 5)\nline(0, 0, 50, 60)");
    REQUIRE(sk_run(1000) == 1);
    std::string d = sk_draws();
    REQUIRE(validJson(d));
    REQUIRE(d == "[[0,10,20,5,0,255,0,10],[2,0,0,50,60,255,0,10]]");
    REQUIRE(has(sk_state(), "\"drawCount\":2"));
}

TEST_CASE("api: all example programs produce valid JSON at every step") {
    const char* programs[] = {
        "fn f(n) { if n < 2 { return n }\n return f(n - 1) + f(n - 2) }\nprint(f(6))",
        "let k = 0\nrepeat 3 { let a = k * 2\n color(a, 1, 2)\n circle(5, 5, 2)\n k = k + 1 }",
        "print(\"hi\")\nprint(true)\nprint(1.5)",
    };
    for (const char* p : programs) {
        REQUIRE(validJson(sk_compile(p)));
        int guard = 0;
        while (guard++ < 10000) {
            REQUIRE(validJson(sk_state()));
            if (sk_step() != 0) break;
        }
        REQUIRE(validJson(sk_state()));
        REQUIRE(validJson(sk_draws()));
    }
}
