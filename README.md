# Sketchlang

A small programming language for generative art. Hand-written lexer, parser,
bytecode compiler and VM in C++20, compiled to WebAssembly and run in a
browser playground that shows the tokens, AST, bytecode and VM state live.

## Status
- [x] Lexer
- [x] Parser (AST)
- [x] Bytecode compiler
- [x] VM
- [ ] WebAssembly build
- [ ] Browser playground

## Build
    cmake -S . -B build
    cmake --build build
    ./build/sketch --ast examples/circles.sk      # prints the AST
    ./build/sketch --tokens examples/circles.sk   # prints the tokens
    ./build/sketch --bytecode examples/circles.sk # prints the bytecode
    ./build/sketch --trace examples/circles.sk    # step-by-step with the stack
    ./build/sketch --svg out.svg examples/rings.sk   # draws a picture
    ./build/sketch --stats examples/bench.sk      # instruction count and speed
    ctest --test-dir build --output-on-failure
