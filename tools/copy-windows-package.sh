#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/windows-x64-llvm-mingw"
DEFAULT_DEST_DIR="/mnt/e/111222/windows-x64-llvm-mingw"
DEST_DIR="${1:-$DEFAULT_DEST_DIR}"
ASSET_DIR="$ROOT_DIR/assets"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    echo "Usage: $(basename "$0") [DEST_DIR]"
    echo
    echo "Copies Windows runtime files and assets into the destination directory."
    echo "Default destination: $DEFAULT_DEST_DIR"
    exit 0
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Build output directory not found: $BUILD_DIR" >&2
    echo "Run ./tools/build-windows-cross.sh first." >&2
    exit 1
fi

if [[ ! -d "$ASSET_DIR" ]]; then
    echo "Asset directory not found: $ASSET_DIR" >&2
    exit 1
fi

mkdir -p "$DEST_DIR"

mapfile -t runtime_files < <(
    find "$BUILD_DIR" -maxdepth 1 -type f \
        \( -name '*.exe' -o -name '*.dll' \) \
        | sort
)

if [[ "${#runtime_files[@]}" -eq 0 ]]; then
    echo "No Windows runtime files were found in $BUILD_DIR" >&2
    echo "Run ./tools/build-windows-cross.sh first." >&2
    exit 1
fi

echo "Copying Windows runtime files to $DEST_DIR"
for file in "${runtime_files[@]}"; do
    cp -f "$file" "$DEST_DIR/"
    echo "  - $(basename "$file")"
done

echo "Syncing assets to $DEST_DIR/assets"
mkdir -p "$DEST_DIR/assets"
rsync -a --delete "$ASSET_DIR/" "$DEST_DIR/assets/"

echo "Done."
echo "Destination: $DEST_DIR"
