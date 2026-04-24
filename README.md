[Website (tokalang.dev)](https://tokalang.dev) | [中文](README_zh.md)

# Toka Programming Language

**Toka is a modern systems programming language without GC. It delivers the absolute performance and low-level control of C, the memory safety of Rust, yet provides a clean and highly productive developer experience reminiscent of scripting languages.**

## 🚀 "Show, Don't Tell"

Say goodbye to the cognitive load of explicit `<'a>` lifetime annotations and complex CMake setups. In Toka, writing high-performance, strictly safe code feels as natural as writing Python:

```rust
// 🚀 Everyday Toka: Physical-level concurrency, minimalist RAII, and native error propagation
import std/io::println
import std/net::TcpStream
import stdx/websocket

// `async` explicitly colors the function; `stream#` lowercase variable with `#` indicates its underlying state will be mutated
fn handle_client(stream#: TcpStream) -> async Result<(), String> {
    // Deep automatic deallocation, no implicit copies, goodbye GC pauses
    auto ws_conn# = websocket::accept_async(stream).await!
    
    // The `!` in `.await!` is Toka's native error propagation operator.
    // Yield control if blocked (await), bubble up instantly on error (!), eliminating verbose `if err` checks.
    ws_conn#.write_text_async("Hello from Toka!").await!
    
    // As it leaves scope, the RAII system silently and safely destroys all sockets and memory.
    return Ok(())
}
```

> **Try it instantly in 1 second:**
> ```bash
> curl -fsSL https://tokalang.dev/install.sh | bash
> ```

---

## 🎯 Is Toka For You?

To respect your time, please verify if Toka fits your needs before diving in:

**✅ It is an excellent choice if:**
*   You need extreme runtime performance and minimal memory footprint (e.g., high-concurrency gateways, game backends, system-level tools).
*   You are tired of Go/Java's GC pauses (STW), but also exhausted by **endlessly fighting the compiler** in Rust, or being forced to take detours just to implement what should be a completely natural logic flow.
*   You want the rapid development speed of Python or TypeScript, while maintaining the low-level safety and control of C.

**❌ It might NOT be for you yet if:**
*   Your production environment mandates pure Windows Server deployment (Native Windows support is under active development; WSL2/macOS/Linux are currently recommended).
*   You require massive out-of-the-box business-level ecosystem frameworks (like Spring Boot) — Toka's current ecosystem is more tailored toward systems-level tooling and infrastructure.

---

## 💎 The 3 Pillars

### 🛡️ Absolute Safety Without Lifetime Annotations
Pioneering the **Single-Hat Principle** and the **PAL (Pointer Aliasing & Lifecycle) Checker**, Toka eliminates dangling pointers and data races at compile time with zero runtime overhead. Enjoy 100% memory safety without fighting the compiler.

### ⚡ Zero-Cost Abstraction & Native C Interop
Built on **LLVM 20**, Toka has **No Garbage Collection (No GC)**. Its memory layout closely mirrors C/C++, allowing you to inline and call the vast existing C ecosystem (like SQLite, OpenGL) seamlessly and without overhead.

### 🚦 Transparent Concurrency & Error Flow
No implicit "black magic schedulers." Toka makes function costs physically explicit: standard functions and `async` (yielding) functions have clear color boundaries. Paired with the native `!` error propagation operator, error handling and async scheduling interweave flawlessly, banishing callback hell and `if err != nil` waterfalls.

### 📦 Blazing Fast Modern Toolchain
Forget messy build scripts. Toka features the built-in `toka run` and an **AI-Native Package Manager**. A simple `toka.json` is all it takes to fetch and build dependencies globally from the edge network.

---

## 🌟 Deep Dive: Attribute Tokens & Soul-Identity Duality

Toka eliminates hidden states by making memory properties explicit through **orthogonal suffix tokens**. Developers can instantly understand the underlying memory structure and lifecycle just by glancing at a variable definition.

### Key Innovations

**1. Syntactic RAII (Native Smart Pointers)**
Toka elevates **Smart Pointers to Core Syntax**. "Lifecycle Management" is internalized as the language's breath.
*   **^T (Unique)** and **~T (Shared)** exist as native grammatical constructs, making RAII the zero-visual-cost default pattern.

**2. Soul-Identity Duality**
Toka philosophically separates an object into its **Soul (Content/Value)** and **Identity (Extension/Address)**.
*   **Precise Semantics**: Access the Soul via Borrow (`&`) and manipulate the Identity via Pointer (`*`), completely eliminating the semantic ambiguity between "reference assignment" and "content modification" found in traditional languages.

**3. Orthogonal Decoupling of Mutability and Nullability**
Say goodbye to the keyword soup of `const volatile unsigned`. Toka strictly defines attribute ownership:
*   **Mutability is a property of the variable (`#`)**: Represents identity transfer or content modification.
*   **Nullability is a property of the type (`?`)**: Represents the potential non-existence of the entity in memory.
*   **Visual Formula**: `T^#?` is instantly recognizable as a "mutable, nullable, unique pointer to T."

**4. @encap Explicit Encapsulation & Resource Safety**
Toka makes normal `shape` members transparent by default, but heavily restricts resource-holding structs (like Files or Sockets) using `@encap`.
*   **Permission & Lifecycle Binding**: Inside an `impl Type@encap` block, you must explicitly expose interfaces (`pub`). More importantly, this is the *only* legal place to define lifecycle methods like `drop`, fundamentally preventing resource tampering and memory leaks at the contract level.

**5. Contract-Based Control Flow**
**"Everything produces a value; everything is a guarantee."**
*   Toka enforces that control flow expressions (like `match`, `for`) must fulfill their value delivery contract on all paths. The unique `for-or` syntax guarantees that even if a loop doesn't execute, the receiver still gets a promised value.

### Token System Table

| Token | Meaning (Value/Content) | Meaning (Identity/Address) |
| :--- | :--- | :--- |
| `#` | **Writable**: Can modify fields | **Swappable**: Can point elsewhere |
| `?` | **Nullable**: Value is `none` | **Nullable**: Identity is `null` |
| `!` | **Writable & Nullable** | **Swappable & Nullable** |
| `^` | - | **Unique Pointer** (Ownership) |
| `~` | - | **Shared Pointer** (Ref Counted) |
| `&` | **Borrow**: View into Value | **Reference**: View into Soul |
| `*` | - | **Raw Pointer** (No Ownership) |

---

## ✅ Project Status (Roadmap)

We are actively building the compiler self-hosting capabilities.

- [x] **Compiler Infrastructure** (Lexer, Parser, LLVM IR CodeGen)
- [x] **Type System** (Primitive Types, Five-State Shapes, ADTs, Pattern Matching)
- [x] **Memory Management** (Unique/Shared Pointers, Move Semantics, Deep Drop, Pointer Rebinding)
- [x] **Object Oriented Features** (`impl` blocks, Trait System)
- [x] **Control Flow Expressions** (Loop expressions, Targeted labels)
- [x] **Modules & Visibility** (Physical/Logical imports, `pub` modifier)
- [x] **Semantic Analysis (Sema)** *(Core Completed)*
    - [x] Strict Mutability Enforcement (`#` Check)
    - [x] Ownership & Borrowing Verification (Move Semantics)
    - [x] Explicit Null Safety (`is` operator)
    - [x] Resource Safety Analysis (Enforced `drop` for resources)
- [x] **Advanced Features**
    - [x] **Generics / Templates**
    - [x] **Concurrency** (OS Threads, Mutex, MPSC Channels, `async`/`await`)
    - [x] **Standard Library** (I/O, Containers `String`/`Vec`/`Option`/`Result`)

### 🚧 Next Steps & Future Roadmap
- [ ] **Cross-Platform Support** (Native Windows MSVC/MinGW ABI Support)
- [ ] **Developer Experience (DX) Upgrades** (LSP Language Server for real-time diagnostics, autocomplete, and jump-to-definition)
- [ ] **Ecosystem** (Official Package Registry & `toka.json` dependency resolver)
- [ ] **Compiler Self-Hosting** (Rewriting the Toka Compiler frontend in Toka)

## 📚 Documentation & Resources

For the latest **Installation Guides**, **Tutorials**, **Code Examples**, and **API References**, please visit the official Toka language website:

👉 **[tokalang.dev](https://tokalang.dev)**

> Note: To ensure documentation consistency and prevent outdated information, all usage instructions, examples, and deep-dive technical articles have been centralized on our official website.

---

## Inspirations & Lineage

Toka is a modern systems programming language built on the shoulders of giants. It was born not to reinvent the wheel, but to explore the optimal balance between **high performance** and **developer experience**, building upon the wisdom of its predecessors.

### 1. C & C++ (Modern C++)
**Core Inspiration: Systems Control, Smart Pointers & RAII**
Toka's memory management mechanism is a direct descendant of Modern C++ (C++11/14+). Toka bakes the semantics of `std::unique_ptr` and `std::shared_ptr` directly into its **core prefix grammar** (`^T` / `~T`), making RAII a zero-visual-cost default standard.

### 2. Rust
**Core Inspiration: The Borrowing Discipline & Memory Safety**
Rust proved that memory safety without garbage collection is achievable. Toka faithfully adopts the strict **"Read-Write Mutex" borrowing rules** to permanently eradicate data races at compile time. However, unlike Rust, Toka **deliberately discards explicit lifetime annotations (e.g., `<'a>`)**, shifting the heavy lifting to the background PAL Checker and lexical scope resolution.

### 3. Haskell / ML Family
**Core Inspiration: Typeclasses & Orthogonality**
Toka's Trait system spiritually inherits from Haskell's Typeclasses. We champion the complete decoupling of "Data Definition" (`shape`) from "Behavior Implementation" (`impl`).

### 4. Swift / Kotlin / C#
**Core Inspiration: Null Safety**
To address "The Billion Dollar Mistake," Toka adopts **Explicit Null Safety**. Using the suffix modifier (`?`), null pointer checks are enforced at the type system level.

### 5. Python
**Core Inspiration: Readability & Productivity**
Toka strives to bring the developer experience of scripting languages to systems programming, aiming to make systems code as readable and approachable as high-level scripts.

### 6. Extended Gratitude
Programming language design is an endless exploration across a vast universe of ideas. Toka embraces the collective wisdom of the entire open-source community and maintains the utmost reverence for all pioneers who have pushed the boundaries of computer science forward.