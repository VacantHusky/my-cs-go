#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BIN="$ROOT_DIR/.local-tools/toolchains/cmake-4.3.0-linux-x86_64/bin/cmake"
BUILD_DIR="$ROOT_DIR/build/windows-x64-llvm-mingw"

"$ROOT_DIR/tools/generate-ui-font.sh"
"$ROOT_DIR/tools/generate-mesh-shaders.sh"
python3 "$ROOT_DIR/tools/generate-mesh-cache.py"
"$CMAKE_BIN" --build "$BUILD_DIR" --config Release
