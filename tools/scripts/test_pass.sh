#!/bin/bash
# tool/test_pass.sh - Parallel Test Runner

# --- Configuration ---
TOKAC="./build/bin/tokac"
# LLI is no longer used. We natively compile the tests to binary.

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'
GRAY='\033[0;90m'

# --- Worker Logic ---
run_worker() {
    test_path="$1"
    [ -e "$test_path" ] || exit 0
    
    file_name=$(basename "$test_path")
    safe_target=$(echo "$test_path" | tr '/' '_')
    out_dir="/tmp/tokac_tests"
    mkdir -p "$out_dir"
    exe_file="${out_dir}/${safe_target}.exe"
    log_file="${out_dir}/${safe_target}.log"
    
    # Capture output in a buffer to ensure atomic printing
    OUTPUT=""
    
    append() {
        OUTPUT="${OUTPUT}$1\n"
    }
    
    # Step 1: Compile Native
    if [ "$file_name" = "llvm_shim_test.tk" ] || [ "$file_name" = "llvm_backend_instructions.tk" ]; then
        tmp_obj="${exe_file}.o"
        if ! "$TOKAC" --emit-obj "$test_path" -o "$tmp_obj" > /dev/null 2> "$log_file"; then
            append "$(printf "[${RED}FAIL${NC}] %-35s" "$file_name")"
            append "    ${RED}$test_path:1: error: Compilation failed${NC}"
            LOGS=$(tail -n 5 "$log_file" | sed 's/^/    | /')
            append "$LOGS"
            echo -ne "$OUTPUT"
            rm -f "$log_file" "$exe_file" "$tmp_obj"
            exit 1
        fi
        if ! clang++-20 "$tmp_obj" lib/sys/llvm_shim.o lib/sys/toka_rt.o $(llvm-config-20 --ldflags --libs) -o "$exe_file" >> "$log_file" 2>&1; then
            append "$(printf "[${RED}FAIL${NC}] %-35s" "$file_name")"
            append "    ${RED}$test_path:1: error: Linking failed${NC}"
            LOGS=$(tail -n 5 "$log_file" | sed 's/^/    | /')
            append "$LOGS"
            echo -ne "$OUTPUT"
            rm -f "$log_file" "$exe_file" "$tmp_obj"
            exit 1
        fi
        rm -f "$tmp_obj"
    else
        if ! "$TOKAC" "$test_path" -o "$exe_file" > /dev/null 2> "$log_file"; then
            append "$(printf "[${RED}FAIL${NC}] %-35s" "$file_name")"
            append "    ${RED}$test_path:1: error: Compilation failed${NC}"
            # Tail logs
            LOGS=$(tail -n 5 "$log_file" | sed 's/^/    | /')
            append "$LOGS"
            echo -ne "$OUTPUT"
            rm -f "$log_file" "$exe_file"
            exit 1
        fi
    fi

    # Step 2: Run Native
    {
        "$exe_file" >> "$log_file" 2>&1 &
        pid=$!
        ( sleep 30; kill -9 $pid 2>/dev/null; echo "TIMEOUT" >> "$log_file" ) >/dev/null 2>&1 < /dev/null &
        killer=$!
        wait $pid
        sub_exit=$?
        kill -9 $killer 2>/dev/null
        exit $sub_exit
    } 2>&1 | grep -v "Abort trap"
    exit_code=${PIPESTATUS[0]}

    # Step 3: Extract & Verify
    errors=()
    
    # Extract panic info
    panic_log_line=$(grep "runtime error: Panic with" "$log_file" | head -n 1)
    if [ -n "$panic_log_line" ]; then
        actual_line=$(echo "$panic_log_line" | grep -oE ":[0-9]+ runtime" | grep -oE "[0-9]+")
        actual_msg=$(echo "$panic_log_line" | sed -n 's/.*Panic with "\(.*\)" \*\*\*/\1/p')
    else
        actual_line=""
        actual_msg=""
    fi
    
    all_expected_lines=$(grep -n "// EXPECT_PANIC" "$test_path" | cut -d: -f1 | tr '\n' ' ')

    if [ $exit_code -eq 0 ]; then
        for exp_line in $all_expected_lines; do
            errors+=("${RED}$test_path:$exp_line: error: Expected panic did not trigger.${NC}")
        done
    else
        if [ -n "$actual_line" ]; then
             # Check execution flow
             for exp_line in $all_expected_lines; do
                if [ "$exp_line" -lt "$actual_line" ]; then
                    errors+=("${RED}$test_path:$exp_line: error: Expected panic missed (passed check).${NC}")
                fi
             done
             
             # Check crash location
             crashed_source=$(sed "${actual_line}q;d" "$test_path")
             if [[ "$crashed_source" == *"assert"* ]]; then
                 errors+=("${RED}$test_path:$actual_line: error: Assertion failed: \"$actual_msg\"${NC}")
             else
                 is_exp=$(echo "$crashed_source" | grep "// EXPECT_PANIC")
                 if [ -z "$is_exp" ]; then
                     errors+=("${RED}$test_path:$actual_line: error: Unexpected panic: \"$actual_msg\"${NC}")
                 fi
             fi
        else
             errors+=("${RED}$test_path:1: error: Runtime crash ($exit_code) without panic info.${NC}")
        fi
    fi

    # Report
    if [ ${#errors[@]} -eq 0 ]; then
        if [ $exit_code -ne 0 ]; then
             append "$(printf "[${GREEN}PASS${NC} with Expected ${YELLOW}Panic${NC}] %-35s" "$file_name")"
             # Context for crash
             if [ -f "$log_file" ]; then
                panic_line=$(grep "runtime error: Panic with" "$log_file" | head -n 1)
                append "${GRAY}    --------------------------------------------------${NC}"
                if [ -n "$panic_line" ]; then
                     append "    | Exit Code: $exit_code"
                     append "    | $panic_line"
                else
                     append "    | Exit Code: $exit_code (No panic info found)"
                     append "    | Last logs:"
                     append "$(tail -n 3 "$log_file" | sed 's/^/    | /')"
                fi
                append "${GRAY}    --------------------------------------------------${NC}"
             fi
        else
            append "$(printf "[${GREEN}PASS${NC}] %-35s" "$file_name")"
        fi
        rm -f "$exe_file" "$log_file"
        echo -ne "$OUTPUT"
        exit 0
    else
        append "$(printf "[${RED}FAIL${NC}] %-35s" "$file_name")"
        for err in "${errors[@]}"; do
            append "    $err"
        done
        
        # Context dump
        if [ -f "$log_file" ]; then
            append "${GRAY}    --------------------------------------------------${NC}"
            append "    | Exit Code: $exit_code"
            append "    | Last logs:"
            append "$(tail -n 3 "$log_file" | sed 's/^/    | /')"
            append "${GRAY}    --------------------------------------------------${NC}"
        fi
        
        echo -ne "$OUTPUT"
        rm -f "$exe_file" # Keep log? No, simplify cleanup.
        exit 1
    fi
}

# --- Entry Point ---

if [ "$1" == "--worker" ]; then
    run_worker "$2"
fi

# Determine core count dynamically for CI and local optimizations
if command -v nproc &> /dev/null; then
    CORES=$(nproc)
else
    CORES=$(sysctl -n hw.ncpu)
fi

# Orchestrator
cmake --build build --parallel $CORES > /dev/null || { echo "Compiler Build Failed"; exit 1; }

echo "Starting Toka 'PASS' Test Suite (Parallel: $CORES)..."
echo "---------------------------------"

RESULTS_FILE=$(mktemp)

# Run parallel tests based on available cores
find tests/pass -name "*.tk" -print0 | xargs -0 -P $CORES -n 1 "$0" --worker | tee "$RESULTS_FILE"

# Stats
# Strip ANSI codes for accurate counting
# sed -r is GNU, sed -E is BSD/Mac. Use perl or simplified grep.
pass_count=$(sed 's/\x1b\[[0-9;]*m//g' "$RESULTS_FILE" | grep -c "\[PASS")
fail_count=$(sed 's/\x1b\[[0-9;]*m//g' "$RESULTS_FILE" | grep -c "\[FAIL")

rm -f "$RESULTS_FILE"

echo "---------------------------------"
echo "Summary:"
echo -e "  Passed: ${GREEN}$pass_count${NC}"
echo -e "  Failed: ${RED}$fail_count${NC}"

[ $fail_count -eq 0 ] || exit 1