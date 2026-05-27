# Toka v0.9.8-02 Release Notes

We are proud to announce the release of **Toka v0.9.8-02**. This release marks a historical milestone in Toka's development, delivering the complete decoupling of the 2.0 Diagnostic Layer, introducing 5 advanced industrial-grade behavior lints, and executing a massive physical refactoring of the core string/byte system under the strict **Toka 1.0 Unified Text & Byte-Stream Constitution**.

---

## 🏛️ 1. Diagnostic System 2.0 & FSM Decoupling

Toka v0.9.8-02 formally introduces the **Compiler Dual-Layer Diagnostic Protocol**, separating diagnostic facts from IDE/LSP semantics to ensure zero compiler bloat and permanent version-lock protection.

- **External Diagnostic Specification (`spec/diagnostic.map.json`)**: Offloaded warning and error explanations, domains, severities, and auto-fix metadata to a detached spec file. This specification is protected by a strict `compiler_compat` version-lock gate.
- **Robust Semantic Node Mapping (`semantic_id`)**: The compiler now emits high-fidelity semantic facts. Every diagnostic report is bound to a three-dimensional `semantic_id` structure:
  `semantic_id = { file_id, node_serial, expansion_context }`
  This ensures real-time, error-free diagnostics tracking inside IDEs and LSP toolings.
- **Error Count Double-Increment Resolution**: Eliminated a critical bug where `ErrorCount` was incremented twice when JSON output was disabled. This resolver ensures compiler error thresholds are hit exactly, preventing truncated compilation logs and snapshot mismatches.

---

## ⚙️ 2. Behavioral Guidance & High-Aesthetic Structural Lints

Five major program sanity and structural lints have been built into the Sema pipeline to guarantee code hygiene and catch logic bugs before codegen:

- **`W0402` (Unused Variable)**: Detects defined but unreferenced local variables and arguments. Automatically respects `_` prefixes and `self` to prevent false positives.
- **`W0403` (Unused Import)**: Performs multi-to-one dependency tracking to identify redundant standard library or local imports, preserving dependency DAG cleanness.
- **`W0406` (Unused Result)**: Catches standalone statements discarding valuable `Result` or `Option` payloads, eliminating unhandled logical vulnerabilities.
- **`S0401` (Unreachable Code)**: Warns when statements are placed after unconditional terminations (`ReturnStmt`).
- **`S0402` (Potential Non-Progress Loop)**: Employs a zero-overhead compile-time heuristic to scan `loop cond {}` blocks, raising structural alerts if conditional variables are not mutated inside the loop.
- **Magenta Caret Visual Hierarchy**: Structural warnings (`S` class) are now rendered in bold **洋红色 (Magenta Bold)** on CLI outputs, creating a distinct visual hierarchy alongside yellow warnings and red errors.

---

## 🧹 3. Standard Library Surgical Clean & FFI Corrections

To set a standard for clean, warning-free system development, the entire Toka Standard Library has been surgically cleaned:

- **Zero-Warning Standard**: Cleaned all unused imports, unused variables, and mutability warnings in `lib/std`, `lib/stdx`, and `lib/sys`.
- **Obsolete File Deprecation**: Physically purged the deprecated `/lib/std/memory.tk` to eliminate dead code and private extern definitions.
- **Process & Net ABI Alignments**: Refactored `lib/std/process.tk` command arguments mapping, resolving reference life issues during parameter loops using standard `Option::Some(&val)` borrowing with clone bounds.

---

## ❄️ 4. The Core String & Byte Stream Physical Refactoring

In accordance with the **Toka 1.0 Final Spec**, the text and byte system has been physically redesigned into a **Stream View Model**, separating textual observation from physical binary manipulation.

- **Txt & Byte Dual-Track Alignment**:
  - **`str` (16 bytes)**: Strictly UTF-8 text view. Supports $O(1)$ `byte_count()`, $O(N)$ logical `count()`, character-based `slice()`, `at()`, and zero-cost `as_bytes()` downgrade.
  - **`bytes` (16 bytes)**: Strictly binary view. Supports $O(1)$ `size()`, boundary-checked $O(1)$ **`slice_bytes()`**, and FFI-safe UTF-8 promotion `try_to_str()`.
- **Abolition of Reverse Character Scanning**: Strictly removed all backward character scans from standard libraries. Backtracking is now explicitly managed by the **`Cursor`** stream decoder, featuring a bounded $O(4)$ safe **`rewind(k)`** FFI alignment check.
- **FFI Memory Equivalence (`memcmp` Equality)**:
  - Rewrote `equals()`, `starts_with()`, `ends_with()`, and `index_of_str()` on `str` views using direct `raw_ptr()` comparisons. This ensures sub-views and sliced strings evaluate with bitwise accuracy under raw pointers, completely solving reference slice address boundaries.
- **Import Symbol Pollution Prevention**: Removed private `fn panic` definitions from `str.tk` and `string.tk`. Bound checks now call `__toka_panic_handler` directly, avoiding global namespace collisions during wildcard `::*` imports.

---

### 📊 Verification Suite Results

- **Native Compiler自举**: `make -C build -j8` ➔ **100% Passed**
- **Sema Regression Suite**: `./tools/scripts/test_pass.sh` ➔ **`[PASS 262/262]` (0 Failures)**
- **Negative Suite**: `./tools/scripts/test_fail.sh` ➔ **`[FAIL 110/110]` (100% Match)**
