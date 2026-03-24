#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BIN="$ROOT_DIR/.local-tools/toolchains/cmake-4.3.0-linux-x86_64/bin/cmake"
NINJA_BIN="$ROOT_DIR/.local-tools/toolchains/ninja-linux/ninja"
TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/windows-x64-llvm-mingw.cmake"
BUILD_DIR="$ROOT_DIR/build/windows-x64-llvm-mingw"

"$CMAKE_BIN" -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_MAKE_PROGRAM="$NINJA_BIN" \
  -DCMAKE_BUILD_TYPE=Release

echo "Configured: $BUILD_DIR"

