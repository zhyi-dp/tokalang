# Toka Compiler Technical Debt & TODOs

This document tracks unresolved core issues, known bugs, and technical debt that require deep compiler architecture refactoring.

## High Priority Compiler Architecture Issues (Discovered during Stdlib Hardening)

### 1. Borrow Checker Flow Analysis Limitation
**Status**: Unresolved / Workaround Applied (Sema Phase)
**Severity**: High
**Description**: 
Currently, the borrow checker (`src/Sema_Expr.cpp`, `src/Sema_Stmt.cpp`) relies on a linear environment tracing mechanism rather than robust Control Flow Graph (CFG) Liveness Analysis. 
If a variable is moved (`cede`) in one branch (e.g., `if`), and borrowed or moved in an exclusive mutually un-reachable branch (e.g., `else if`), the compiler erroneously flags it as `Use of moved value` (`ERR_USE_MOVED`).
**Impact**: 
This limitation severely restricts the flexibility of control flow when implementing complex standard library algorithms (e.g., Robin Hood displacement in `HashMap`). Developers are forced to extract movement operations outside the conditional scope or revert to fallback implementations.
**Proposed Fix**: 
Refactor branch condition environment merges (`Env_Out = Env_True INTERSECT Env_False`) to accurately track moved-states relative to flow execution paths.

### 2. LLVM PhysReg Copy CodeGen Crash
**Status**: Unresolved / Mitigated via SOA (CodeGen Phase)
**Severity**: Critical
**Description**: 
When instantiating complex or heavily-nested generic types (e.g., `Option<Entry<'K, 'V>>` or generic `Bucket` structs) and attempting to pass or return them by value, the LLVM CodeGen backend fatally crashes with `LLVM ERROR: Cannot emit physreg copy instruction`.
**Impact**: 
Toka currently fails to correctly map the ABI (Application Binary Interface) calling conventions for large multi-generic structures. Instead of implicitly elevating large structural returns to `byval` or pointer mechanisms (`alloca` / `sret`), it tries to map them to physical registers, causing LLVM assertion failures.
**Mitigation**:
Standard Library components (like `HashMap`) are currently circumventing this by adopting a Structure of Arrays (SOA) flat layout.
**Proposed Fix**:
Investigate `src/CodeGen_Type.cpp` and `src/CodeGen_Expr.cpp` to correctly lower complex product shapes to valid LLVM memory types and strictly enforce ABI parameter size attributes for returned compounds.
