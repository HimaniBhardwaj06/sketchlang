#!/usr/bin/env bash
# Serves the playground at http://localhost:8000 (browsers will not load .wasm from file://)
cd "$(dirname "$0")/web"
echo "Open http://localhost:8000"
python3 -m http.server 8000
