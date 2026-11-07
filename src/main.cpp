#include "compiler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "printer.hpp"
#include "render.hpp"
#include "vm.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

static int usage() {
    std::cerr << "usage: sketch [options] <file.sk>\n"
                 "  --tokens        print the token stream\n"
                 "  --ast           print the syntax tree\n"
                 "  --bytecode      print the compiled bytecode\n"
                 "  --trace         run step by step, printing each instruction and the stack\n"
                 "  --stats         print instruction count and speed\n"
                 "  --svg <file>    write the drawing to an SVG file\n";
    return 1;
}

int main(int argc, char** argv) {
    bool tokens = false, ast = false, bytecode = false, trace = false, stats = false;
    const char* path = nullptr;
    const char* svgPath = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--tokens")) tokens = true;
        else if (!std::strcmp(argv[i], "--ast")) ast = true;
        else if (!std::strcmp(argv[i], "--bytecode")) bytecode = true;
        else if (!std::strcmp(argv[i], "--trace")) trace = true;
        else if (!std::strcmp(argv[i], "--stats")) stats = true;
        else if (!std::strcmp(argv[i], "--svg") && i + 1 < argc) svgPath = argv[++i];
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

    if (tokens) {
        for (const auto& t : sk::Lexer(source).tokenize()) {
            std::cout << t.line << ":" << t.col << "\t" << sk::tokenTypeName(t.type) << "\t'" << t.lexeme << "'\n";
        }
        return 0;
    }

    sk::ParseResult parsed = sk::parseSource(source);
    if (!parsed.ok()) {
        for (const auto& e : parsed.errors) std::cerr << path << ":" << e << "\n";
        return 1;
    }
    if (ast) {
        std::cout << sk::toSexpr(parsed.program) << "\n";
        return 0;
    }

    sk::CompileResult compiled = sk::compile(parsed.program);
    if (!compiled.ok()) {
        for (const auto& e : compiled.errors) std::cerr << path << ":" << e << "\n";
        return 1;
    }
    if (bytecode) {
        std::cout << sk::disassemble(compiled.module);
        return 0;
    }

    sk::VM vm(compiled.module);
    auto start = std::chrono::steady_clock::now();
    if (trace) {
        while (vm.status() == sk::Status::Running) {
            const sk::Frame& f = vm.frames().back();
            std::cout << "[" << f.fn->name << "] " << sk::disassembleInstr(compiled.module, *f.fn, f.ip) << "\n";
            vm.step();
            std::cout << "        stack: [";
            for (size_t i = 0; i < vm.stack().size(); i++) {
                std::cout << (i ? ", " : "") << vm.format(vm.stack()[i]);
            }
            std::cout << "]\n";
        }
    } else {
        vm.run();
    }
    auto end = std::chrono::steady_clock::now();

    for (const auto& line : vm.output()) std::cout << line << "\n";
    if (vm.status() == sk::Status::Error) {
        std::cerr << path << ":" << vm.errorLine() << ": runtime error: " << vm.error() << "\n";
    }
    if (svgPath) {
        std::ofstream out(svgPath);
        out << sk::toSvg(vm.draws());
        std::cerr << "wrote " << svgPath << " (" << vm.draws().size() << " shapes)\n";
    }
    if (stats) {
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        double mops = ms > 0 ? static_cast<double>(vm.steps()) / ms / 1000.0 : 0;
        std::cerr << vm.steps() << " instructions in " << ms << " ms (" << mops << " M instr/s)\n";
    }
    return vm.status() == sk::Status::Error ? 1 : 0;
}
