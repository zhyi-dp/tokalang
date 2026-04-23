# Toka Language: Post-Beta (v1.0) Roadmap

## Core Data Structures & Algorithms
- `std/btree_map` / `std/btree_set`: Ordered dictionaries and sets for deterministic iteration and range queries.
- `std/regex`: Regular expression engine (either native implementation or PCRE bindings).
- `std/cell`: Interior mutability abstractions (`Cell`, `RefCell`) for complex graph/tree structures under strict borrowing.

## System & Utilities
- `std/log`: Standardized logging facade supporting levels (Debug, Info, Warn, Error).
- `std/ffi`: C-string abstractions (`CStr`, `CString`) and safer memory boundaries.
- `std/fmt` (Phase 2): Deprecate `libc_snprintf` to achieve a 100% pure Toka formatting engine, essential for bare-metal targets.
- `std/thread` (Phase 2): Move closure memory boxing into the `sys` layer to completely isolate the `thread.tk` API from OS memory allocators.
- `std/net` (Phase 2): Encapsulate remaining direct BSD socket FFI calls into the `sys/net` abstraction layer.

## Developer Tooling
- `std/test`: A native testing macro/utility suite (e.g., `assert_eq!`, test runners).
