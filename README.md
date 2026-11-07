# Sketchlang

A small programming language for generative art. Hand-written lexer, parser,
bytecode compiler and VM in C++20, compiled to WebAssembly and run in a
browser playground that shows the tokens, AST, bytecode and VM state live.

## Status
- [x] Lexer
- [x] Parser (AST)
- [x] Bytecode compiler
- [x] VM
- [x] JSON output and C API for the browser
- [ ] WebAssembly build verified in a browser (run ./build-wasm.sh)
- [x] Browser playground (web/)

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

## Browser playground
Needs Emscripten once: `brew install emscripten`

    ./build-wasm.sh     # builds web/sketch.js + web/sketch.wasm
    ./serve.sh          # then open http://localhost:8000

The playground shows the source, the bytecode with the current instruction highlighted,
the AST, the tokens, the VM stack, call frames, globals, console output and the drawing.
Use Step / Play to watch the program execute one instruction at a time.

## Architecture
    source -> Lexer -> tokens -> Parser -> AST -> Compiler -> bytecode -> VM -> draw commands
                                                                   \-> JSON (api.cpp) -> browser UI
