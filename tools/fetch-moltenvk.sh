#!/bin/sh
# Fetches the official MoltenVK release and stages the universal
# libMoltenVK.dylib for bundling into melonDS.app (see the macOS
# bundle logic in src/frontend/qt_sdl/CMakeLists.txt, which looks in
# external/moltenvk first).
#
# Usage: tools/fetch-moltenvk.sh [version] [outdir]
set -eu

VERSION="${1:-v1.4.0}"
OUTDIR="${2:-external/moltenvk}"
URL="https://github.com/KhronosGroup/MoltenVK/releases/download/${VERSION}/MoltenVK-macos.tar"

mkdir -p "$OUTDIR"

if [ -f "$OUTDIR/libMoltenVK.dylib" ]; then
    echo "$OUTDIR/libMoltenVK.dylib already present, skipping download" >&2
    echo "$OUTDIR/libMoltenVK.dylib"
    exit 0
fi

echo "Downloading MoltenVK $VERSION..." >&2
curl -fL "$URL" | tar -x -C "$OUTDIR"

# release archive layout has moved around between versions; just locate
# the macOS dynamic library wherever it lives
DYLIB=$(find "$OUTDIR" -name libMoltenVK.dylib \( -path '*macos*' -o -path '*macOS*' \) | head -1)
if [ -z "$DYLIB" ]; then
    echo "error: no macOS libMoltenVK.dylib in the release archive:" >&2
    find "$OUTDIR" -name '*.dylib' >&2
    exit 1
fi

cp "$DYLIB" "$OUTDIR/libMoltenVK.dylib"
echo "$OUTDIR/libMoltenVK.dylib"
