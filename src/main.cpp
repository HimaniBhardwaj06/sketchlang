#include "lexer.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: sketch <file.sk>\n";
        return 1;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    sk::Lexer lexer(source);
    for (const auto& t : lexer.tokenize()) {
        std::cout << t.line << ":" << t.col << "\t"
                  << sk::tokenTypeName(t.type) << "\t'" << t.lexeme << "'\n";
    }
    return 0;
}
