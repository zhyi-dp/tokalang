#!/usr/bin/env python3
# Copyright (c) 2026 Toka Project. All rights reserved.
# tools/scripts/check_dependency_dag.py
# Static dependency linter to strictly enforce Toka 1.0 architecture & DAG constraints.

import os
import sys
import re

def parse_imports(filepath):
    """
    Parses a Toka source file and returns all imported package paths.
    """
    imports = []
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading file {filepath}: {e}")
        return imports

    # Strip multi-line comments: /* ... */
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    
    # Process line by line
    for line in content.splitlines():
        # Strip single-line comments
        line = re.split(r"//", line)[0].strip()
        if not line:
            continue
        
        # Match import statements, e.g., "import package/name::{" or "import package/name :: {"
        # Also handles "import package/name" if that's supported.
        match = re.match(r"^\s*import\s+([a-zA-Z0-9_/]+)", line)
        if match:
            imports.append(match.group(1))
            
    return imports

def check_dag(base_dir):
    """
    Runs the full 9 DAG checks under base_dir (normally GitDP/tokalang/lib).
    """
    errors = []
    
    # Rule 8: stdx/core is frozen and only contains context.tk
    stdx_core_dir = os.path.join(base_dir, "stdx", "core")
    if os.path.isdir(stdx_core_dir):
        files = os.listdir(stdx_core_dir)
        for f in files:
            if f != "context.tk":
                errors.append(f"[Rule 8 Violation]: stdx/core is physically frozen. Found forbidden file: {os.path.join(stdx_core_dir, f)}")
    
    # Rule 9: No forbidden namespace directories (runtime, utils, common, misc) under lib/stdx/
    forbidden_dirs = {"runtime", "utils", "common", "misc"}
    stdx_dir = os.path.join(base_dir, "stdx")
    if os.path.isdir(stdx_dir):
        for root, dirs, files in os.walk(stdx_dir):
            for d in dirs:
                if d.lower() in forbidden_dirs:
                    errors.append(f"[Rule 9 Violation]: Found forbidden directory name '{d}' in {root}")
            for f in files:
                # Also prevent filenames that match
                base, ext = os.path.splitext(f)
                if base.lower() in forbidden_dirs:
                    errors.append(f"[Rule 9 Violation]: Found forbidden filename '{f}' in {root}")

    # Process all .tk and .tki files under lib
    for root, dirs, files in os.walk(base_dir):
        # Prevent searching in any forbidden directory under lib/
        for d in list(dirs):
            if d.lower() in forbidden_dirs:
                # We already caught this, but let's prevent walking into it to avoid duplicate errors
                dirs.remove(d)
                
        for file in files:
            if not (file.endswith(".tk") or file.endswith(".tki")):
                continue
                
            filepath = os.path.join(root, file)
            # Compute relative path within lib (e.g. stdx/io/bufio.tk)
            rel_path = os.path.relpath(filepath, base_dir)
            parts = rel_path.split(os.sep)
            
            # Find all imports in this file
            imports = parse_imports(filepath)
            
            # Rule 6: sys/llvm_ffi is the unique LLVM-related file in lib/sys/
            if parts[0] == "sys":
                if "llvm" in file.lower() and file != "llvm_ffi.tk":
                    errors.append(f"[Rule 6 Violation]: {rel_path} has 'llvm' in its name, but only sys/llvm_ffi.tk is allowed.")
                # Scan for any external function starting with toka_llvm_ in other sys files
                if file != "llvm_ffi.tk":
                    try:
                        with open(filepath, "r", encoding="utf-8") as f:
                            for line_num, line in enumerate(f, 1):
                                if "toka_llvm_" in line:
                                    errors.append(f"[Rule 6 Violation]: {rel_path}:{line_num} declares/references 'toka_llvm_', but only sys/llvm_ffi.tk is allowed to declare LLVM FFI.")
                    except Exception as e:
                        pass

            # Rule 5: lib/stdx/rand/rand.tk cannot import any platform-specific API (sys/*)
            if rel_path == os.path.join("stdx", "rand", "rand.tk"):
                for imp in imports:
                    if imp.startswith("sys/"):
                        errors.append(f"[Rule 5 Violation]: stdx/rand/rand.tk cannot import platform-specific API '{imp}'.")

            for imp in imports:
                # Check for Rule 9: No forbidden namespace imports (only under stdx namespace)
                if imp.startswith("stdx/"):
                    imp_parts = imp.split('/')
                    for p in imp_parts:
                        if p.lower() in forbidden_dirs:
                            errors.append(f"[Rule 9 Violation]: {rel_path} imports '{imp}', which contains forbidden namespace '{p}'.")

                # Rule 1: stdx absolutely cannot import toolchain
                if parts[0] == "stdx" and imp.startswith("toolchain"):
                    errors.append(f"[Rule 1 Violation]: {rel_path} is in stdx but imports toolchain module '{imp}'.")

                # Rule 2: toolchain absolutely cannot import stdx
                if parts[0] == "toolchain" and imp.startswith("stdx"):
                    errors.append(f"[Rule 2 Violation]: {rel_path} is in toolchain but imports stdx module '{imp}'.")

                # Rule 3: stdx cannot import specific platform physical implementations (sys/linux, sys/macos, sys/windows, etc.)
                if parts[0] == "stdx":
                    if imp.startswith("sys/") and not (imp == "sys/libc" or imp.startswith("sys/os")):
                        errors.append(f"[Rule 3 Violation]: {rel_path} is in stdx but directly imports physical platform ABI '{imp}'.")

                # Rule 4: std absolutely cannot import stdx
                if parts[0] in ("std", "core", "sys") and imp.startswith("stdx"):
                    errors.append(f"[Rule 4 Violation]: {rel_path} is in standard/core/sys library but imports stdx module '{imp}'.")

                # Rule 7: stdx/core absolutely cannot import stdx/io/* or stdx/net/*
                if len(parts) >= 2 and parts[0] == "stdx" and parts[1] == "core":
                    if imp.startswith("stdx/io") or imp.startswith("stdx/net"):
                        errors.append(f"[Rule 7 Violation]: {rel_path} is in stdx/core but imports '{imp}'.")

    return errors

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # Assuming tools/scripts/check_dependency_dag.py, we go up to GitDP/tokalang/lib
    lib_dir = os.path.abspath(os.path.join(script_dir, "..", "..", "lib"))
    
    print(f"Running Dependency Linter on: {lib_dir}...")
    errors = check_dag(lib_dir)
    
    if errors:
        print("\n[Toka Constitution Violation]: Strict DAG Broken!")
        print("==================================================")
        for err in errors:
            print(f"  - {err}")
        print("==================================================")
        sys.exit(1)
    
    print("Dependency Linter PASSED! All 9 DAG rules are strictly satisfied.")
    sys.exit(0)

if __name__ == "__main__":
    main()
