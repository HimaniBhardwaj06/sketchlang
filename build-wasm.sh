#!/usr/bin/env bash
# Builds the Sketchlang engine to WebAssembly: produces web/sketch.js and web/sketch.wasm
# Requires Emscripten:  brew install emscripten   (check with: emcc --version)
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v emcc >/dev/null 2>&1; then
  echo "emcc not found. Install it with: brew install emscripten" >&2
  exit 1
fi

em++ \
  src/lexer.cpp src/parser.cpp src/printer.cpp src/bytecode.cpp src/compiler.cpp \
  src/vm.cpp src/natives.cpp src/render.cpp src/json.cpp src/api.cpp \
  -std=c++20 -O2 -fexceptions \
  -sEXPORTED_FUNCTIONS=_sk_compile,_sk_state,_sk_draws,_sk_step,_sk_run,_sk_reset,_sk_version,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8 \
  -sMODULARIZE=1 -sEXPORT_NAME=createSketchModule \
  -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web \
  -o web/sketch.js

echo "Built web/sketch.js and web/sketch.wasm"
ls -la web/sketch.js web/sketch.wasm
