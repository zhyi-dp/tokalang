#!/bin/bash
set -e

# Toka Project Rebuild Script
# Automatically recompiles both the 'tokac' compiler and the 'toka' native package manager CLI

# Workaround for LLVM 20 ASan container-overflow false positives on macOS
export ASAN_OPTIONS=detect_container_overflow=0

ROOT_DIR=$(pwd)
BIN_DIR="$ROOT_DIR/build/bin"

# Attempt to find LLVM 20 clang path dynamically via brew
if command -v brew >/dev/null 2>&1; then
    LLVM_CLANG="$(brew --prefix llvm@20 2>/dev/null || brew --prefix llvm 2>/dev/null)/bin/clang"
fi

# Fallback paths
if [ -z "$LLVM_CLANG" ] || [ ! -f "$LLVM_CLANG" ]; then
    if [ -f "/opt/homebrew/opt/llvm@20/bin/clang" ]; then
        LLVM_CLANG="/opt/homebrew/opt/llvm@20/bin/clang"
    elif [ -f "/usr/local/opt/llvm@20/bin/clang" ]; then
        LLVM_CLANG="/usr/local/opt/llvm@20/bin/clang"
    else
        LLVM_CLANG="clang-20"
    fi
fi

if ! command -v "$LLVM_CLANG" >/dev/null 2>&1 && [ ! -f "$LLVM_CLANG" ]; then
    echo "Error: LLVM 20 clang not found."
    echo "Please ensure llvm@20 is installed via Homebrew (macOS) or apt (Linux)."
    exit 1
fi

echo "====================================="
echo "0. Generating Diagnostics Definition"
echo "====================================="
python3 tools/scripts/gen_diagnostics.py

echo "====================================="
echo "1. Building Toka Compiler (tokac)"
echo "====================================="
make -C build -j8

# Ensure the newly built tokac is in the PATH so it can compile the toka wrapper
export PATH="$BIN_DIR:$PATH"

echo "   -> Compiling Toka Runtime (toka_rt.o / toka_rt.obj)..."
if [ "$(uname)" == "Darwin" ]; then
    $LLVM_CLANG -isysroot $(xcrun --show-sdk-path) -c lib/sys/toka_rt.c -o lib/sys/toka_rt.o
else
    $LLVM_CLANG -c lib/sys/toka_rt.c -o lib/sys/toka_rt.o
    cp lib/sys/toka_rt.o lib/sys/toka_rt.obj 2>/dev/null || true
fi

echo ""
echo "====================================="
echo "2. Building Toka CLI Tool (toka)"
echo "====================================="
cd tools/toka
echo "   -> Compiling and Linking tools/toka/src/main.tk with internal LLD..."
tokac -I "$ROOT_DIR/lib" -I src src/main.tk "$ROOT_DIR/lib/sys/toka_rt.o" -o toka

echo "   -> Installing toka to $BIN_DIR/toka..."
mkdir -p "$BIN_DIR"
cp toka "$BIN_DIR/toka"

# Clean up build artifacts in tools/toka
rm -f toka

# Return to root directory
cd "$ROOT_DIR"

echo ""
echo "====================================="
echo "3. Building Toka Formatter (tokafmt)"
echo "====================================="
cd tools/tokafmt
echo "   -> Compiling and Linking tools/tokafmt/src/main.tk with internal LLD..."
tokac -I "$ROOT_DIR/lib" src/main.tk "$ROOT_DIR/lib/sys/toka_rt.o" -o tokafmt

echo "   -> Installing tokafmt to $BIN_DIR/tokafmt..."
cp tokafmt "$BIN_DIR/tokafmt"

# Clean up build artifacts in tools/tokafmt
rm -f tokafmt

cd "$ROOT_DIR"

echo ""
echo "====================================="
echo "4. Building Toka Language Server (tokalsp)"
echo "====================================="
cd tools/tokalsp
echo "   -> Compiling and Linking tools/tokalsp/main.tk with internal LLD..."
tokac -I "$ROOT_DIR/lib" main.tk "$ROOT_DIR/lib/sys/toka_rt.o" -o tokalsp

echo "   -> Installing tokalsp to $BIN_DIR/tokalsp..."
cp tokalsp "$BIN_DIR/tokalsp"

# Clean up build artifacts in tools/tokalsp
rm -f tokalsp

cd "$ROOT_DIR"

echo ""
echo "====================================="
echo "5. Building Toka Incremental Engine (forge)"
echo "====================================="
cd tools/forge
echo "   -> Compiling and Linking tools/forge/src/main.tk with internal LLD..."
tokac -I "$ROOT_DIR/lib" src/main.tk "$ROOT_DIR/lib/sys/toka_rt.o" -o forge

echo "   -> Installing forge to $BIN_DIR/forge..."
cp forge "$BIN_DIR/forge"

# Clean up build artifacts in tools/forge
rm -f forge

cd "$ROOT_DIR"

echo ""
echo "✨ Rebuild Successful! 'tokac', 'toka', 'tokafmt', 'tokalsp', and 'forge' are ready in build/bin."
echo "Make sure to add $BIN_DIR to your PATH if you haven't already:"
echo "    export PATH=\"$ROOT_DIR/build/bin:\$PATH\""
