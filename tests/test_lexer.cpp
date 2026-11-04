#include <catch2/catch_test_macros.hpp>
#include "lexer.hpp"

using sk::TokenType;

static std::vector<TokenType> types(const std::string& src) {
    std::vector<TokenType> out;
    for (auto& t : sk::Lexer(src).tokenize()) out.push_back(t.type);
    return out;
}

TEST_CASE("empty input gives only Eof") {
    REQUIRE(types("") == std::vector<TokenType>{TokenType::Eof});
}

TEST_CASE("let statement") {
    REQUIRE(types("let x = 10") ==
            std::vector<TokenType>{TokenType::Let, TokenType::Identifier,
                                   TokenType::Assign, TokenType::Number, TokenType::Eof});
}

TEST_CASE("two-character operators") {
    REQUIRE(types("== != <= >=") ==
            std::vector<TokenType>{TokenType::Eq, TokenType::NotEq, TokenType::LessEq,
                                   TokenType::GreaterEq, TokenType::Eof});
}

TEST_CASE("comments are skipped") {
    REQUIRE(types("// hi\n42") ==
            std::vector<TokenType>{TokenType::Number, TokenType::Eof});
}

TEST_CASE("decimal numbers keep their lexeme") {
    auto toks = sk::Lexer("3.14").tokenize();
    REQUIRE(toks[0].lexeme == "3.14");
}

TEST_CASE("line and column tracking") {
    auto toks = sk::Lexer("let\n  x").tokenize();
    REQUIRE(toks[1].line == 2);
    REQUIRE(toks[1].col == 3);
}

TEST_CASE("unexpected character becomes an Error token") {
    auto toks = sk::Lexer("@").tokenize();
    REQUIRE(toks[0].type == TokenType::Error);
}

TEST_CASE("unterminated string is an error") {
    auto toks = sk::Lexer("\"abc").tokenize();
    REQUIRE(toks[0].type == TokenType::Error);
}
