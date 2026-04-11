#!/bin/bash

# Auto-Compile Compiler if needed
make -C build -j8
if [ $? -ne 0 ]; then
    echo "Compiler Build Failed"
    exit 1
fi

# Configuration
LLI=$(which lli-20 || which lli || echo "/usr/local/opt/llvm@20/bin/lli")
VERIFIER="tools/scripts/test_verify_fail.py"

if [ ! -f "$VERIFIER" ]; then
    echo "Error: Verifier script not found at $VERIFIER"
    exit 1
fi

# Delegate to the Python script
python3 "$VERIFIER"
exit $?
