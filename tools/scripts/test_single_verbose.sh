#!/bin/bash

# Auto-Compile Compiler if needed
make -C build -j8
if [ $? -ne 0 ]; then
    echo "Compiler Build Failed"
    exit 1
fi

TEST_FILE=$1
SAFE_TARGET=$(echo "$TEST_FILE" | tr '/' '_')
OUT_DIR="/tmp/tokac_tests"
mkdir -p "$OUT_DIR"
LL_FILE="${OUT_DIR}/${SAFE_TARGET}.ll"

echo "Testing $TEST_FILE..."
./build/bin/tokac $TEST_FILE > $LL_FILE
if [ $? -eq 0 ]; then
    echo "Compilation Succeeded"
    lli $LL_FILE
    if [ $? -eq 0 ]; then
        echo "Execution Succeeded"
        exit 0
    else
        echo "Execution Failed"
        exit 1
    fi
else
    echo "Compilation Failed"
    exit 1
fi
