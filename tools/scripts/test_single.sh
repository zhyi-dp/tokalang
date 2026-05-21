#!/bin/bash

# tool/test_single.sh - Run a single Toka test case

# Auto-Compile Compiler if needed
make -C build -j8
if [ $? -ne 0 ]; then
    echo "Compiler Build Failed"
    exit 1
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

BASE_NAME=$(basename "$TK_FILE")
SAFE_TARGET=$(echo "$TK_FILE" | tr '/' '_')
OUT_DIR="/tmp/tokac_tests"
mkdir -p "$OUT_DIR"
EXE_FILE="${OUT_DIR}/${SAFE_TARGET}.exe"
LOG_FILE="${OUT_DIR}/${SAFE_TARGET}.log"

# --- Configuration ---
TOKAC="./build/bin/tokac"

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
    if ! clang++-20 "$tmp_obj" lib/sys/llvm_shim.o lib/sys/toka_rt.o $(llvm-config-20 --ldflags --libs) -o "$EXE_FILE" >> "$LOG_FILE" 2>&1; then
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
