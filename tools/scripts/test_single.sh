#!/bin/bash

# tool/test_single.sh - Run a single Toka test case

# Auto-Compile Compiler if needed
if [ -d build ] && [ -f build/Makefile ] && command -v make &> /dev/null; then
    make -C build -j8
    if [ $? -ne 0 ]; then
        echo "Compiler Build Failed"
        exit 1
    fi
fi

if [ -z "$1" ]; then
  echo "Usage: $0 <tk_file>"
  echo "Example: $0 tests/pass/test_small.tk"
  exit 1
fi

TK_FILE="$1"

if [ ! -f "$TK_FILE" ]; then
    echo "Error: File '$TK_FILE' not found."
    exit 1
fi

BASE_NAME="${TK_FILE##*/}"
SAFE_TARGET="${TK_FILE//\//_}"
OUT_DIR="/tmp/tokac_tests"
mkdir -p "$OUT_DIR"
EXE_FILE="${OUT_DIR}/${SAFE_TARGET}.exe"
LOG_FILE="${OUT_DIR}/${SAFE_TARGET}.log"

# --- Configuration ---
TOKAC="./build/bin/tokac"

# Autodetect compiler and tools
if command -v clang++-20 &> /dev/null; then
    CLANGXX="clang++-20"
elif command -v clang++ &> /dev/null; then
    CLANGXX="clang++"
else
    CLANGXX="clang++"
fi

if command -v llvm-config-20 &> /dev/null; then
    LLVM_CONFIG="llvm-config-20"
elif command -v llvm-config &> /dev/null; then
    LLVM_CONFIG="llvm-config"
else
    LLVM_CONFIG="llvm-config"
fi

# Pre-read LLVM flags to avoid subshell forks in workers (critical for Windows MSYS2 stability under SSH)
LLVM_CPPFLAGS=""
LLVM_LDFLAGS_LIBS=""
if command -v "$LLVM_CONFIG" &> /dev/null; then
    "$LLVM_CONFIG" --cxxflags > .tokac_cppflags_$$.txt 2>/dev/null
    if [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* || "$OSTYPE" == "win32"* ]]; then
        read -r LLVM_CPPFLAGS < .tokac_cppflags_$$.txt
    else
        LLVM_CPPFLAGS=$(cat .tokac_cppflags_$$.txt | tr '\n' ' ')
    fi
    rm -f .tokac_cppflags_$$.txt

    "$LLVM_CONFIG" --ldflags --libs > .tokac_ldflags_$$.txt 2>/dev/null
    if [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* || "$OSTYPE" == "win32"* ]]; then
        read -r LLVM_LDFLAGS_LIBS < .tokac_ldflags_$$.txt
    else
        LLVM_LDFLAGS_LIBS=$(cat .tokac_ldflags_$$.txt | tr '\n' ' ')
    fi
    rm -f .tokac_ldflags_$$.txt
fi

EXTRA_LIBS=""
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    EXTRA_LIBS="-lws2_32"
fi

# Compile runtime objects dynamically for the local architecture
SYSROOT_FLAGS=""
if [[ "$OSTYPE" == "darwin"* ]]; then
    SYSROOT_FLAGS="-isysroot $(xcrun --show-sdk-path)"
fi

if command -v clang-20 &> /dev/null; then
    CLANG="clang-20"
elif command -v clang &> /dev/null; then
    CLANG="clang"
else
    CLANG="clang"
fi

rm -f lib/sys/toka_rt.o
"$CLANG" $SYSROOT_FLAGS -c lib/sys/toka_rt.c -o lib/sys/toka_rt.o || { echo "Failed to compile toka_rt.c"; exit 1; }

rm -f lib/sys/llvm_shim.o
SHIM_CXXFLAGS="$LLVM_CPPFLAGS"
if [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* || "$OSTYPE" == "win32"* ]]; then
    SHIM_CXXFLAGS="$SHIM_CXXFLAGS -DLLVM_SHARED_LIBS"
fi
"$CLANGXX" $SYSROOT_FLAGS -O3 -c lib/sys/llvm_shim.cpp -o lib/sys/llvm_shim.o $SHIM_CXXFLAGS || { echo "Failed to compile llvm_shim.cpp"; exit 1; }

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

echo "[TEST] Running $TK_FILE"
echo "  - Compiling Native..."

# Compile Native
if [ "$BASE_NAME" = "llvm_shim_test.tk" ] || [ "$BASE_NAME" = "llvm_backend_instructions.tk" ]; then
    tmp_obj="${EXE_FILE}.o"
    if ! "$TOKAC" --emit-obj "$TK_FILE" -o "$tmp_obj" > /dev/null 2> "$LOG_FILE"; then
        echo -e "  - ${RED}Compilation FAILED${NC}"
        echo "  - Error Log:"
        cat "$LOG_FILE"
        rm -f "$LOG_FILE" "$EXE_FILE" "$tmp_obj"
        exit 1
    fi
    if ! "$CLANGXX" $SYSROOT_FLAGS "$tmp_obj" lib/sys/llvm_shim.o lib/sys/toka_rt.o $LLVM_LDFLAGS_LIBS $EXTRA_LIBS -o "$EXE_FILE" >> "$LOG_FILE" 2>&1; then
        echo -e "  - ${RED}Linking FAILED${NC}"
        echo "  - Error Log:"
        cat "$LOG_FILE"
        rm -f "$LOG_FILE" "$EXE_FILE" "$tmp_obj"
        exit 1
    fi
    rm -f "$tmp_obj"
else
    if ! "$TOKAC" "$TK_FILE" -o "$EXE_FILE" > /dev/null 2> "$LOG_FILE"; then
        echo -e "  - ${RED}Compilation FAILED${NC}"
        echo "  - Error Log:"
        cat "$LOG_FILE"
        rm -f "$LOG_FILE" "$EXE_FILE"
        exit 1
    fi
fi

echo "  - Running Native Binary..."

# Shift to get extra args
shift

# Run Native
"$EXE_FILE" "$@" 2>&1 | tee "$LOG_FILE"
RUN_STATUS=${PIPESTATUS[0]}

echo -e "  - Finished with Exit Code: ${YELLOW}$RUN_STATUS${NC}"

# Check for panic if expected
panic_log_line=$(grep "runtime error: Panic with" "$LOG_FILE" | head -n 1)
if [ -n "$panic_log_line" ]; then
    echo -e "  - ${RED}Panic Detected:${NC} $panic_log_line"
fi

# Cleanup
# rm -f "$EXE_FILE" "$LOG_FILE"

exit $RUN_STATUS
