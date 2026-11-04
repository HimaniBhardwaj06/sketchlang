#include "lexer.hpp"
#include "parser.hpp"
#include "printer.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

static int usage() {
    std::cerr << "usage: sketch [--tokens | --ast] <file.sk>\n";
    return 1;
}

int main(int argc, char** argv) {
    bool showTokens = false;
    const char* path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--tokens") == 0) showTokens = true;
        else if (std::strcmp(argv[i], "--ast") == 0) showTokens = false;
        else path = argv[i];
    }
    if (!path) return usage();

    std::ifstream in(path);
    if (!in) {
        std::cerr << "cannot open " << path << "\n";
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    if (showTokens) {
        for (const auto& t : sk::Lexer(source).tokenize()) {
            std::cout << t.line << ":" << t.col << "\t"
                      << sk::tokenTypeName(t.type) << "\t'" << t.lexeme << "'\n";
        }
        return 0;
    }

    sk::ParseResult result = sk::parseSource(source);
    if (!result.ok()) {
        for (const auto& e : result.errors) std::cerr << path << ":" << e << "\n";
        return 1;
    }
    std::cout << sk::toSexpr(result.program) << "\n";
    return 0;
}
