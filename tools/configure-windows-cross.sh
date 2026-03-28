#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BIN="${CMAKE_BIN:-$(command -v cmake || true)}"
NINJA_BIN="${NINJA_BIN:-$(command -v ninja || true)}"
TOOLCHAIN_FILE="$ROOT_DIR/cmake/toolchains/windows-x64-mingw.cmake"
BUILD_DIR="$ROOT_DIR/build/windows-x64-mingw"

if [[ -z "$CMAKE_BIN" ]]; then
  echo "cmake was not found in PATH. Please install the system cmake package." >&2
  exit 1
fi

if [[ -z "$NINJA_BIN" ]]; then
  echo "ninja was not found in PATH. Please install the system ninja-build package." >&2
  exit 1
fi

"$CMAKE_BIN" -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_MAKE_PROGRAM="$NINJA_BIN" \
  -DCMAKE_BUILD_TYPE=Release

echo "Configured: $BUILD_DIR"
