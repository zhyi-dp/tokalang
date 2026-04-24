[中文官网 (tokalang.dev/zh)](https://tokalang.dev/zh) | [English](README.md)

# Toka 编程语言

**Toka 是一门无 GC 的现代系统级编程语言。它拥有 C 的绝对性能与底层掌控力，兼具 Rust 级别的内存安全，却能提供如脚本语言般清爽的开发体验。**

## 🚀 "Show, Don't Tell"

告别繁重的 `<'a>` 生命周期标注负担，忘记错综复杂的 CMake。在 Toka 中，写出兼具**极致性能**与**绝对安全**的代码就像写 Python 一样自然：

```rust
// 🚀 Toka 的日常：物理级并发，极简的符号 RAII 与原生异常传播
import std/io::println
import std/net::TcpStream
import stdx/websocket

// `async` 显式染色；`stream#` 小写变量加 `#` 表示该变量的底层状态会被操作修改
fn handle_client(stream#: TcpStream) -> async Result<(), String> {
    // 自动深层释放、无隐式拷贝、彻底告别 GC 暂停
    auto ws_conn# = websocket::accept_async(stream).await!
    
    // .await! 中的 `!` 是 Toka 的原生异常传播符。
    // 遇到阻塞则交出控制权 (await)，遇到报错则直接向上冒泡 (!)，抛弃繁琐的 if err 检查
    ws_conn#.write_text_async("Hello from Toka!").await!
    
    // 离开作用域时，RAII 机制会静默、安全地深度回收所有的 Socket 与内存资源
    return Ok(())
}
```

> **只需一秒，立即体验：**
> ```bash
> curl -fsSL https://tokalang.dev/install.sh | bash
> ```

---

## 🎯 Toka 适合你吗？

为了尊重您的时间，在您决定深入了解前，请确认：

**✅ 它是你的绝佳选择，如果：**
*   你需要极高的运行性能与极低的内存占用（如高并发网关、游戏后端、系统级工具）。
*   你受够了 Go/Java 的 GC 暂停（STW），但也厌倦了在 Rust 里为了过编译而**无休止地和编译器“打架”**，甚至被迫绕路去实现某些本该非常自然的代码逻辑。
*   你希望语言能像 Python/TypeScript 一样写得飞快，但依然拥有 C 语言的底层安全与原生掌控力。

**❌ 它可能暂不适合你，如果：**
*   你的生产环境必须部署在纯 Windows Server 上（目前原生支持仍在开发中，暂推荐 WSL2/macOS/Linux）。
*   你需要开箱即用的极其庞大的业务级生态（如 Spring Boot 全家桶）—— Toka 的生态目前更偏向底层和系统级。

---

## 💎 三大核心支柱 (The 3 Pillars)

### 🛡️ 抛弃生命周期标注的绝对安全
首创**单帽原则 (Single-Hat Principle)** 与 **PAL (Pointer Aliasing & Lifecycle) 检查器**。在编译期以零运行时开销杜绝悬空指针与数据竞争。享受 100% 的内存安全，免受与编译器搏斗的折磨。

### ⚡ 零成本抽象与 C 原生互操作
底层基于 **LLVM 20** 构建，**无垃圾回收 (No GC)**。内存布局与 C/C++ 极其贴近，支持无缝、无开销地内联调用现有的庞大 C 语言历史遗产（如 SQLite、OpenGL）。

### 🚦 物理级透明的并发模型与错误流
拒绝隐式的“黑魔法调度器”。Toka 将函数的开销显式化：普通函数、`async` (异步挂起) 都有清晰的颜色边界。配合原生的 `!` 错误传播符，错误处理与异步调度完美交织，彻底告别回调地狱和 `if err != nil` 瀑布。

### 📦 极速的现代化工具链
忘记繁琐的构建脚本。内置 `toka run` 与 **AI 原生包管理器**。只需一个简单的 `toka.json`，即可在全局边缘网络中极速拉取并构建全部依赖。

---

## 🌟 深度理念：属性标记与灵身二元论

Toka 通过采用**正交后缀符号**将内存属性显式化，彻底消除了隐蔽状态。开发者仅需扫一眼变量定义，就能瞬间秒懂其背后隐藏的内存结构与生命周期。

### 核心革新点 (Key Innovations)

**1. 原生语法的 RAII (Syntactic RAII)**
Toka 将**智能指针提升为核心语法符号**。这意味着“生命周期管理”被内化为语言的呼吸。
*   **^T (独占)** 和 **~T (共享)** 直接作为原生类型存在，让 RAII 成为零视觉负担的默认模式。

**2. 灵-身二元论 (Soul-Identity Duality)**
Toka 哲学性地将对象拆分为 **Soul (内涵/值)** 与 **Identity (外延/身份)**。
*   **精确语义**：通过借用 (`&`) 访问 Soul，通过指针 (`*`) 操作 Identity。消除“引用赋值”与“内容修改”的语义模糊。

**3. 变与空的彻底解耦 (Orthogonal Attributes)**
告别 `const volatile unsigned` 的关键字堆砌。Toka 极度严谨地规定了属性的归属：
*   **可变性是变量的属性 (`#`)**：代表身份转移或内容修改。
*   **可空性是类型的属性 (`?`)**：代表内存中可能不存在该实体。
*   **视觉化化学式**：`T^#?` 一眼即知它是一个“本身可被重新赋值的、可能为空的、指向 T 的独占指针”。

**4. @encap 显式封装与资源安全 (Explicit Encapsulation)**
Toka 默认普通的 `shape` 对外透明，但对于持有关键资源（如 File、Socket）的结构体，必须使用 `@encap` 进行严格封装。
*   **权限与生命周期绑定**：在 `impl Type@encap` 块中，开发者必须显式暴露接口，且这是定义 `drop` (析构) 等生命周期方法的唯一合法场所。这从底层契约上防止了资源的非法篡改与内存泄漏。

**5. 契约式控制流 (Contract-Based Control Flow)**
**“一切皆有值，一切皆保证”**。
*   强制控制流表达式（如 `match`, `for`）在所有路径上履行值的交付契约。独特的 `for-or` 语法保证即使循环未执行，接收者也能得到承诺的值。

### 符号系统表

| 标记 (Token) | 内容上的含义 (Value/Content) | 身份上的含义 (Identity/Address) |
| :--- | :--- | :--- |
| `#` | **可写**: 可修改字段/内容 | **可交换**: 可重定向(Reseat) |
| `?` | **可空**: 值为 `none` | **可空**: 身份为 `null` |
| `!` | **可写且可空** | **可交换且可空** |
| `^` | - | **独占指针** (所有权) |
| `~` | - | **共享指针** (引用计数) |
| `&` | **借用**: 值的临时视图 | **引用**: Soul 的临时视图 |
| `*` | - | **原始指针** (无所有权) |

---

## ✅ 项目状态 (路线图)

我们正在积极构建编译器的自举 (self-hosting) 能力。

- [x] **编译器基础设施** (Lexer, Parser, LLVM IR CodeGen)
- [x] **类型系统** (基础类型, 五态 Shape 系统, ADTs, 模式匹配)
- [x] **内存管理** (独占/共享指针, 移动语义, 递归式释放, 指针重绑定)
- [x] **面向对象特性** (`impl` 块, Trait 系统)
- [x] **控制流表达式** (循环表达式, `break`/`continue` 标签)
- [x] **模块与可见性** (物理路径与逻辑导入, `pub` 可见性)
- [x] **语义分析 (Sema)** *(核心完成)*
    - [x] 严格的可变性强制检查 (`#` 检查)
    - [x] 所有权与借用验证 (Ownership & Borrowing Verification)
    - [x] 显式空安全 (Null Safety)
    - [x] 资源安全分析 (强制含资源 Shape 实现 `drop`)
- [x] **高级特性**
    - [x] **泛型 / 模板 (Generics)**
    - [x] **并发 (Concurrency)** (原生线程, 锁, MPSC 通道, `async`/`await`)
    - [x] **标准库 (Standard Library)** (I/O, 容器 `String`/`Vec`/`Option`/`Result`)

### 🚧 正在进行与未来规划 (Next Steps)
- [ ] **跨平台支持** (Windows 原生 MSVC/MinGW ABI 适配)
- [ ] **开发体验 (DX) 升级** (LSP 语言服务器，提供实时诊断、自动补全与跳转)
- [ ] **生态系统** (完善官方包注册表与 `toka.json` 依赖解析器)
- [ ] **编译器自举 (Self-Hosting)** (使用 Toka 重写 Toka 编译器前端)

## 📚 官方文档与资源

更多详细的**安装指引**、**教程**、**代码示例**以及**API 手册**，请访问 Toka 语言官方网站：

👉 **[tokalang.dev/zh](https://tokalang.dev/zh)**

> 注意：为了保证文档的统一性，避免信息过期，所有使用说明、最佳实践与深度技术文章均已集中迁移至官方网站。

---

## 致敬与灵感来源 (Inspirations & Lineage)

Toka 是一门站在巨人肩膀上的现代系统级编程语言。它的诞生并非为了重新发明轮子，而是为了在前辈们的智慧基础上，探索**高性能**与**开发体验**之间的最佳平衡。

### 1. C & C++ (Modern C++)
**核心启迪：极速的系统控制权、智能指针与 RAII (起源)**
Toka 的内存管理机制直接从 Modern C++ (C++11/14+) 一脉相承。Toka 将 `std::unique_ptr` 和 `std::shared_ptr` 的语义直接熔铸为**核心前缀语法** (`^T` / `~T`)，让资源安全首次变成一种“零视觉负担”的默认标准。

### 2. Rust
**核心启迪：所有权与借用安全法则 (Borrowing Discipline)**
Rust 证明了无 GC 的极致内存安全是可以达成的。Toka 忠实地引进了极为严苛的 **"读写互斥"借用规则**，从编译期掐断数据竞争。但与 Rust 不同，Toka **蓄意抛弃了显式生命周期泛型符号 (如 `<'a>`)**，转而依靠 PAL 检查器和词法作用域来默默完成这些繁重的推理。

### 3. Haskell / ML Family
**核心致敬：类型类与正交设计 (Typeclasses & Orthogonality)**
Toka 的 Trait 系统精神上继承自 Haskell 的 Typeclasses。我们推崇将“数据定义”(`shape`)与“行为实现”(`impl`)完全剥离。

### 4. Swift / Kotlin / C#
**核心致敬：空安全 (Null Safety)**
为了彻底解决“十亿美元的错误”，Toka 采用了**显式空安全**。通过后缀修饰符 (`?`)，我们将空指针检查强制提升到了类型系统层面。

### 5. Python
**核心致敬：可读性与开发效率 (Readability & Productivity)**
Toka 致力于将脚本语言的开发体验带入系统编程，力求让系统级代码拥有脚本般的清晰度与亲和力。

### 6. 万有引力与未尽的致敬
编程语言的设计是一场在浩瀚星海中不断传承的探索。Toka 拥抱并崇尚开源社区汇聚的集体智慧，并对一切推动了程序设计学发展的先行者们保持着最虔诚的敬畏。