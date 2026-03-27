#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GLSLANG_BIN="$ROOT_DIR/.local-tools/build/glslang-host/StandAlone/glslang"
OUT_DIR="$ROOT_DIR/assets/generated/shaders"
VERT_SRC="$ROOT_DIR/src/renderer/vulkan/shaders/mesh.vert"
FRAG_SRC="$ROOT_DIR/src/renderer/vulkan/shaders/mesh.frag"
INSTANCED_VERT_SRC="$ROOT_DIR/src/renderer/vulkan/shaders/mesh_instanced.vert"
VERT_OUT="$OUT_DIR/mesh.vert.spv"
FRAG_OUT="$OUT_DIR/mesh.frag.spv"
INSTANCED_VERT_OUT="$OUT_DIR/mesh_instanced.vert.spv"
TEXT_VERT_SRC="$ROOT_DIR/src/renderer/vulkan/shaders/text.vert"
TEXT_FRAG_SRC="$ROOT_DIR/src/renderer/vulkan/shaders/text.frag"
TEXT_VERT_OUT="$OUT_DIR/text.vert.spv"
TEXT_FRAG_OUT="$OUT_DIR/text.frag.spv"

mkdir -p "$OUT_DIR"

if [[ ! -x "$GLSLANG_BIN" ]]; then
    if [[ -f "$VERT_OUT" && -f "$FRAG_OUT" && -f "$INSTANCED_VERT_OUT" && -f "$TEXT_VERT_OUT" && -f "$TEXT_FRAG_OUT" ]]; then
        echo "Using existing generated Vulkan shaders."
        exit 0
    fi
    echo "Missing glslang compiler at $GLSLANG_BIN" >&2
    exit 1
fi

"$GLSLANG_BIN" -V "$VERT_SRC" -o "$VERT_OUT"
"$GLSLANG_BIN" -V "$FRAG_SRC" -o "$FRAG_OUT"
"$GLSLANG_BIN" -V "$INSTANCED_VERT_SRC" -o "$INSTANCED_VERT_OUT"
"$GLSLANG_BIN" -V "$TEXT_VERT_SRC" -o "$TEXT_VERT_OUT"
"$GLSLANG_BIN" -V "$TEXT_FRAG_SRC" -o "$TEXT_FRAG_OUT"

echo "Generated Vulkan mesh and text shaders into $OUT_DIR"
