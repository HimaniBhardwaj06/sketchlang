# Sketchlang

A small programming language for generative art. Hand-written lexer, parser,
bytecode compiler and VM in C++20, compiled to WebAssembly and run in a
browser playground that shows the tokens, AST, bytecode and VM state live.

## Status
- [x] Lexer
- [x] Parser (AST)
- [ ] Bytecode compiler
- [ ] VM
- [ ] WebAssembly build
- [ ] Browser playground

## Build
    cmake -S . -B build
    cmake --build build
    ./build/sketch examples/circles.sk            # prints the AST
    ./build/sketch --tokens examples/circles.sk   # prints the tokens
    ctest --test-dir build --output-on-failure
