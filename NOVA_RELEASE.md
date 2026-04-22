# 🚀 Toka v0.8.0-beta "Nova" 

Welcome to **Toka v0.8.0-beta ("Nova")**, the first official beta release of the language!

The **Nova** release marks a historic milestone for Toka. The language engine is no longer just an experiment; it now boasts a fully functional, zero-cost concurrency stack, airtight RAII memory safety mechanics, and an incredibly intuitive error propagation system.

This release represents exactly what Toka was built for: **"Static strict-type ironclad safety"** blended perfectly with **"Smooth scripting-like coding experiences"**.

## 🌟 Major Highlights

### 1. `!` Error Propagation & Early Stack-Return
Say goodbye to manual `.unwrap()` crashes and `if err != null` boilerplates. Toka has fully implemented the `!` propagation operator.

By appending `!`, the compiler automatically destructures `Result` or `Option` containers. If a poisoning exception or `Err` is detected, it triggers a **safe stack-return cutoff**, gracefully throwing the error upwards to the caller while activating all local RAII descructors.

```toka
pub fn handle_connection(stream#: TcpStream) -> async Result<(), String> {
    // Gracefully handle TCP errors or assign valid connection entity
    auto ws_conn# = ws_accept_async(stream).await!
    // ...
```

### 2. RAII Concurrency & `Mutex<T>`
Toka's Pointer Morphology and `@encap` mechanisms have reached their final form. Our `Mutex<T>` implementation physically encapsulates the data within the lock shell.

You cannot modify the data without safely extracting the `mut_guard`. Once the scope ends, the guard is implicitly dropped, releasing the lock. No more manual `Free()` or `Lock.Release()` leaking!

```toka
auto m# = Mutex<i32>::new(0)
{
    auto mut_guard = m#.lock()!
    auto *mut_val# = mut_guard.get_ref()
    mut_val = mut_val + 10
} 
// Lock is automatically released here!
```

### 3. SFINAE Trait-Bound Generic Filtering
We have resolved significant compiler regressions mathematically filtering out invalid `impl` generation. Toka’s Semantic Analyzer (`Sema`) now actively runs Trait Constraint validation checks *before* AST template instantiation, ensuring methods like `parse_json()` are never generated for unsupported primitive structs.

## 📦 Distribution & Installation

Toka now officially distributes its binary toolchain (`tokac`, `tokafmt`) alongside the standard library (`lib/`).

You can install Toka "Nova" globally on your macOS or Linux machine via our new one-line curl command:

```bash
curl -fsSL https://tokalang.dev/install.sh | bash
```

Alternatively, download the `.tar.gz` package from the Assets below and add the `bin/` directory to your OS path manually!

## 🐞 Fixes & Miscellaneous
- Fixed `LLI` dynamic interpreter mapping bounds.
- VS Code plugin size compressed to `<10KB` (Removed obsolete `node_modules`).
- Core iterator APIs hardened against memory leakage.
- Full syntax highlighting support for the `!` operator inside `.tk` files.

---
**Enjoy the Nova release and happy coding!** 🚀
