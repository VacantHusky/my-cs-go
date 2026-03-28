#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BIN="${CMAKE_BIN:-$(command -v cmake || true)}"
BUILD_DIR="$ROOT_DIR/build/windows-x64-mingw"

if [[ -z "$CMAKE_BIN" ]]; then
  echo "cmake was not found in PATH. Please install the system cmake package." >&2
  exit 1
fi

"$ROOT_DIR/tools/generate-ui-font.sh"
"$ROOT_DIR/tools/generate-mesh-shaders.sh"
python3 "$ROOT_DIR/tools/generate-mesh-cache.py"
"$CMAKE_BIN" --build "$BUILD_DIR" --config Release
