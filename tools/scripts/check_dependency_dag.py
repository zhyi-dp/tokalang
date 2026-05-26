#!/usr/bin/env python3
# Copyright (c) 2026 Toka Project. All rights reserved.
# tools/scripts/check_dependency_dag.py
# Static dependency linter to strictly enforce Toka 1.0/10.0 LDT Architecture & DAG constraints.

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
        match = re.match(r"^\s*import\s+([a-zA-Z0-9_/]+)", line)
        if match:
            imports.append(match.group(1))
            
    return imports

def check_dag(base_dir):
    """
    Runs the comprehensive Toka 10-Year Anti-Rot Dependency Linter.
    """
    errors = []
    
    # Forbidden namespace directories under stdx
    forbidden_stdx_dirs = {"runtime", "utils", "common", "misc"}
    
    # Standard Freeze list of forbidden abstractions inside std
    forbidden_std_abstractions = {"json", "http", "cli", "log", "encoding"}
    
    stdx_dir = os.path.join(base_dir, "stdx")
    if os.path.isdir(stdx_dir):
        for root, dirs, files in os.walk(stdx_dir):
            for d in dirs:
                if d.lower() in forbidden_stdx_dirs:
                    errors.append(f"[Rule 3/9 Violation]: Found forbidden namespace directory '{d}' under stdx: {root}")
            for f in files:
                base, ext = os.path.splitext(f)
                if base.lower() in forbidden_stdx_dirs:
                    errors.append(f"[Rule 3/9 Violation]: Found forbidden namespace filename '{f}' under stdx: {root}")

    # Process all .tk and .tki files under lib
    for root, dirs, files in os.walk(base_dir):
        # Prevent walking into any forbidden directories to avoid duplicates
        for d in list(dirs):
            if d.lower() in forbidden_stdx_dirs:
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
            
            current_layer = parts[0]
            
            # Check 1.0 Std Freeze Rule (Rule 2: No semantic leaking in std)
            if current_layer == "std":
                for item in forbidden_std_abstractions:
                    if item in file.lower() or item in rel_path.lower():
                        errors.append(f"[Rule 2 Violation]: std cannot contain domain abstraction '{item}'. Violating file: {rel_path}")
            
            # Check Rule 4: Atomic Single Truth (std/atomic.tk is a pure façade)
            if rel_path == os.path.join("std", "atomic.tk"):
                try:
                    with open(filepath, "r", encoding="utf-8") as f:
                        content = f.read()
                        # Strict Facade check: no loops, spinlocks or direct syscalls/extern fn
                        if "loop" in content or "while" in content:
                            errors.append(f"[Rule 4 Violation]: std/atomic.tk must be a pure façade. Spinlocks or loops are strictly forbidden.")
                        if "extern" in content:
                            errors.append(f"[Rule 4 Violation]: std/atomic.tk must be a pure façade. extern fn declarations are strictly forbidden.")
                except Exception:
                    pass

            # Check Rule 5: Pure Rand Algorithm (stdx/rand has no entropy/OS randomness)
            if len(parts) >= 2 and parts[0] == "stdx" and parts[1] == "rand":
                for imp in imports:
                    if imp.startswith("sys/"):
                        errors.append(f"[Rule 5 Violation]: stdx/rand/rand.tk cannot import platform sys API '{imp}'.")

            for imp in imports:
                imp_parts = imp.split('/')
                imp_root = imp_parts[0]
                
                # Check for Rule 3: No forbidden namespace imports under stdx/
                if imp.startswith("stdx/"):
                    for p in imp_parts:
                        if p.lower() in forbidden_stdx_dirs:
                            errors.append(f"[Rule 3 Violation]: {rel_path} imports '{imp}', containing forbidden namespace '{p}'.")

                # ==========================================
                # STRICT DAG DIRECTION VERIFICATION
                # ==========================================
                
                # 1. core: can only import core/*
                if current_layer == "core":
                    if imp_root != "core":
                        errors.append(f"[DAG Violation]: core file {rel_path} attempts to import outside core: '{imp}'")
                        
                # 2. sys: can import sys/*, core/*
                elif current_layer == "sys":
                    if imp_root not in ("sys", "core"):
                        errors.append(f"[DAG Violation]: sys file {rel_path} attempts to import forbidden layer: '{imp}'")
                        
                # 3. std: can import std/*, sys/*, core/*
                elif current_layer == "std":
                    if imp_root not in ("std", "sys", "core"):
                        errors.append(f"[DAG Violation]: std file {rel_path} attempts to import forbidden layer: '{imp}'")
                        
                # 4. stdx: can import stdx/*, std/*, core/*, sys/libc, sys/os/*
                elif current_layer == "stdx":
                    if imp_root not in ("stdx", "std", "core", "sys"):
                        errors.append(f"[DAG Violation]: stdx file {rel_path} attempts to import forbidden layer: '{imp}'")
                    # If importing sys, strictly restrict to sys/libc or sys/os/*
                    if imp_root == "sys":
                        if not (imp == "sys/libc" or imp.startswith("sys/os")):
                            errors.append(f"[DAG Violation]: stdx file {rel_path} directly imports physical platform ABI: '{imp}'")
                            
                # 5. toolchain: can import toolchain/*, sys/*, core/*
                elif current_layer == "toolchain":
                    if imp_root not in ("toolchain", "sys", "core"):
                        errors.append(f"[DAG Violation]: toolchain file {rel_path} attempts to import forbidden layer: '{imp}'")

    return errors

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    lib_dir = os.path.abspath(os.path.join(script_dir, "..", "..", "lib"))
    
    print(f"Running Toka 10-Year Anti-Rot Dependency Linter on: {lib_dir}...")
    errors = check_dag(lib_dir)
    
    if errors:
        print("\n[Toka Constitution Violation]: Strict DAG Broken!")
        print("======================================================================")
        for err in errors:
            print(f"  - {err}")
        print("======================================================================")
        sys.exit(1)
    
    print("Dependency Linter PASSED! All 10-Year anti-rot DAG rules are strictly satisfied.")
    sys.exit(0)

if __name__ == "__main__":
    main()
