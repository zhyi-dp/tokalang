#!/bin/bash
set -e

# Toka Project Rebuild Script
# Automatically recompiles both the 'tokac' compiler and the 'toka' native package manager CLI

ROOT_DIR=$(pwd)
BIN_DIR="$ROOT_DIR/build/bin"
LLVM_CLANG="/usr/local/opt/llvm@20/bin/clang"

if [ ! -f "$LLVM_CLANG" ]; then
    echo "Error: LLVM 20 clang not found at $LLVM_CLANG."
    echo "Please ensure llvm@20 is installed via Homebrew."
    exit 1
fi

echo "====================================="
echo "1. Building Toka Compiler (tokac)"
echo "====================================="
make -C build -j8

# Ensure the newly built tokac is in the PATH so it can compile the toka wrapper
export PATH="$BIN_DIR:$PATH"

echo ""
echo "====================================="
echo "2. Building Toka CLI Tool (toka)"
echo "====================================="
cd tools/toka
echo "   -> Compiling tools/toka/src/main.tk to main.ll..."
tokac -I "$ROOT_DIR/lib" src/main.tk > main.ll

echo "   -> Compiling main.ll to executable with LLVM 20 Clang..."
$LLVM_CLANG main.ll -isysroot $(xcrun --show-sdk-path) -o toka

echo "   -> Installing toka to $BIN_DIR/toka..."
mkdir -p "$BIN_DIR"
cp toka "$BIN_DIR/toka"

# Clean up build artifacts in tools/toka
rm -f main.ll toka

# Return to root directory
cd "$ROOT_DIR"

echo ""
echo "====================================="
echo "3. Building Toka Formatter (tokafmt)"
echo "====================================="
cd tools/tokafmt
echo "   -> Compiling tools/tokafmt/src/main.tk to main.ll..."
tokac -I "$ROOT_DIR/lib" src/main.tk > main.ll

echo "   -> Compiling main.ll to executable with LLVM 20 Clang..."
$LLVM_CLANG main.ll -isysroot $(xcrun --show-sdk-path) -o tokafmt

echo "   -> Installing tokafmt to $BIN_DIR/tokafmt..."
cp tokafmt "$BIN_DIR/tokafmt"

# Clean up build artifacts in tools/tokafmt
rm -f main.ll tokafmt

cd "$ROOT_DIR"

echo ""
echo "✨ Rebuild Successful! 'tokac', 'toka', and 'tokafmt' are ready in build/bin."
echo "Make sure to add $BIN_DIR to your PATH if you haven't already:"
echo "    export PATH=\"$ROOT_DIR/build/bin:\$PATH\""
