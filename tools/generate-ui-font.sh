#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/.local-tools/host-tools"
SRC="$ROOT_DIR/tools/font_atlas_baker.cpp"
BIN="$BUILD_DIR/font_atlas_baker"
CHARSET="$ROOT_DIR/tools/ui_font_charset.txt"
MERGED_CHARSET="$BUILD_DIR/ui_font_charset_merged.txt"
OUT_HEADER="$ROOT_DIR/src/renderer/font/GeneratedUiFontData.h"
OUT_SOURCE="$ROOT_DIR/src/renderer/font/GeneratedUiFontData.cpp"

mkdir -p "$BUILD_DIR"

FONT_PATH=""
for candidate in \
  "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" \
  "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc"
do
  if [[ -f "$candidate" ]]; then
    FONT_PATH="$candidate"
    break
  fi
done

if [[ -z "$FONT_PATH" ]]; then
  echo "Could not find a Noto Sans CJK font." >&2
  exit 1
fi

python3 - "$ROOT_DIR" "$CHARSET" "$MERGED_CHARSET" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
base_charset = Path(sys.argv[2]).read_text(encoding='utf-8')
out_path = Path(sys.argv[3])

seen = set()
merged = []

def push(ch: str) -> None:
    if ord(ch) < 32 or ch in seen:
        return
    seen.add(ch)
    merged.append(ch)

for ch in base_charset:
    push(ch)

for pattern in ("src/**/*.cpp", "src/**/*.h"):
    for path in sorted(root.glob(pattern)):
        text = path.read_text(encoding='utf-8')
        for ch in text:
            if ord(ch) > 127:
                push(ch)

out_path.write_text("".join(merged) + "\n", encoding='utf-8')
PY

c++ "$SRC" -std=c++20 -O2 -Wall -Wextra -Wpedantic $(pkg-config --cflags freetype2) $(pkg-config --libs freetype2) -o "$BIN"
"$BIN" "$FONT_PATH" "$MERGED_CHARSET" "$OUT_HEADER" "$OUT_SOURCE"
