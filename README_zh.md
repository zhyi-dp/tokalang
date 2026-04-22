[Website (tokalang.dev)](https://tokalang.dev) | [English](README.md)

# Toka 编程语言

Toka 是一门由易中华 (YiZhonghua) 于 2025 年创造的系统级编程语言。它旨在实现 **安全**、**高效** 和 **语法简洁**，并通过其创新的 **属性标记系统 (Attribute Token System)** 解决了传统编程中安全性与生产力之间的权衡难题。

## 🌟 核心理念：属性标记 (Attribute Tokens)

Toka 通过采用正交后缀符号将属性显式化，Toka 彻底消除了内存状态的隐蔽性。开发者仅需扫一眼变量定义，就能瞬间秒懂其背后隐藏的内存结构与生命周期。

## 🛡️ 经过数学证明的安全性：PAL 检查器

在 Toka 极度简洁的语法背后，伫立着其最核心的安全引擎：**PAL (Pointer Aliasing & Lifecycle) 检查器**。

与依赖于繁琐的手动生命周期标注（例如 `<'a>`）的方案不同，Toka 通过其全自动化、基于词法边界约束的静态分析引擎来捍卫内存安全。
*   **形式化验证已完成**：目前关于 PAL 检查器的底层理论基础、数学完备性证明，以及相关学术论文工作已经全部宣告完成。
*   **零开销的静默安全**：PAL 可以在编译期以零运行时开销的姿态，完美推演“读写互斥锁”原理，提前在编译器拦截悬空指针、Double-Free (二次释放)、内存逃逸以及迭代器失效问题。

### 🚀 核心革新点 (Key Innovations)

**1. 原生语法的 RAII (Syntactic RAII)**
Toka 是极少数将**智能指针提升为核心语法符号**的语言。这意味着**“生命周期管理”被内化为语言的呼吸**，而非仅仅是标准库提供的功能。
*   **^T (独占)** 和 **~T (共享)** 直接作为原生类型存在，让 RAII 成为零视觉负担的默认模式。

**2. 灵-身二元论 (Soul-Identity Duality)**
Toka 哲学性地将对象拆分为 **Soul (内涵/值)** 与 **Identity (外延/身份)**。
*   **精确语义**：通过借用 (`&`) 访问 Soul，通过指针 (`*`) 操作 Identity。
*   **消除歧义**：这彻底解决了传统语言中“引用赋值”与“内容修改”的语义模糊。在 Toka 中，移动身份 (`move`) 和修改内容 (`mut`) 是完全不同的语法维度。

**3. 正交属性系统 (Orthogonal Attributes)**
告别 `const volatile unsigned` 的关键字堆砌。
*   **视觉化化学式**：通过 `#` (可变), `?` (可空), `!` (可变且可空) 等正交后缀，Toka 让变量定义像化学式一样清晰。`T^#?` 一眼即知是“可变、可空、独占的 T 指针”。

**4. @encap 显式封装 (Explicit Encapsulation)**
Toka 默认 `shape` 的成员是透明可访问的，但通过 `@encap` 实现了细粒度的访问控制。
*   **权限收束**：通过 `impl Type@encap` 块，开发者必须显式列出 (`pub`) 哪些字段对外部可见。未列出的字段自动私有化。
*   **生命周期绑定**：`@encap` 块也是定义构造、析构 (`drop`) 等生命周期关键方法的唯一合法场所，从而确保资源的安全性。

**5. 契约式控制流 (Contract-Based Control Flow)**
**“一切皆有值，一切皆保证”**。
*   **分支平衡**：Toka 强制控制流表达式（如 `match`, `for`）在所有路径上都必须履行值的交付契约。
*   **循环回退**：独特的 `for-or` 语法保证了即使循环未执行（空迭代），接收者也能得到承诺的值保证。

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

**示例:**
```rust
auto x# = 10        // 可变整数 (Mutable Integer)
x# = 11            // 允许修改 (OK)

auto ^p = new Rect  // Rect 的独占指针 (默认初始化)
auto ^#p2? = ...    // 可交换(指向可变)、可空、独占指针
```

## ✅ 项目状态 (路线图)

我们正在积极构建编译器的自举 (self-hosting) 能力。

- [x] **编译器基础设施**
    - [x] 词法分析器 (Lexer)
    - [x] 语法分析器 (Parser / AST Generation)
    - [x] LLVM IR 代码生成 (Code Generation)
- [x] **类型系统**
    - [x] 基础类型 (`i32`, `f64`, `bool` 等)
    - [x] 五态 Shape 系统 (Struct, Tuple, Array)
    - [x] **代数数据类型 (ADTs)** (通过 `shape`)
    - [x] 模式匹配 (`match` 语句)
- [x] **内存管理 (Memory Management)**
    - [x] 独占指针 (`^`) 与移动语义 (Move Semantics)
    - [x] 共享指针 (`~`) 与引用计数 (Reference Counting)
    - [x] **递归式释放 (Recursive Drop)** (Deep Drop)
    - [x] **Soul-Identity 内存模型** (不透明指针支持)
    - [x] **指针重绑定 (Pointer Rebinding)** (`&x = ...` 强更新)
- [x] **面向对象特性**
    - [x] `impl` 块 (方法)
    - [x] **Trait 系统** (接口、默认实现)
- [x] **控制流表达式 (Control Flow Expressions)**
    - [x] 循环 (`while`, `for`, `loop`) 作为表达式
    - [x] 通过 `pass` 和 `break` 产生值
    - [x] 循环的 `or` 回退块
    - [x] `break`/`continue` 的定向标签 (Target Labels)
- [x] **模块与可见性 (Modules & Visibility)**
    - [x] 文件级模块 (File-based Modules)
    - [x] `import` 导入系统 (物理路径与逻辑导入)
    - [x] `pub` 可见性修饰符
- [x] **语义分析 (Sema)** *(核心完成 / Core Completed)*
    - [x] 基础设施脚手架 (Infrastructure Scaffolding)
    - [x] **严格的可变性强制检查** (`#` 检查)
    - [x] 类型检查 (Type Checking Pass)
    - [x] 所有权与借用验证 (Ownership & Borrowing Verification)
    - [x] **空安全 (Null Safety)** (`is` 操作符、严格判空)
    - [x] **资源安全分析 (Resource Safety)** (强制含资源 Shape 实现 `drop`)
- [x] **高级特性**
    - [x] **泛型 / 模板 (Generics)** (类型与函数)
    - [x] **自动析构合成 (Automatic Drop Synthesis)** (递归 Deep Drop)
    - [x] **显式资源退让 (Explicit Resource Yielding: `cede`)**
    - [x] **并发 (Concurrency)**
        - [x] 原生系统线程 (`std/thread`)
        - [x] 同步原语 (`Mutex`, `RwMutex`, `CondVar`)
        - [x] 通道通信 (MPSC Channels)
        - [x] `Task` 与 `async`/`await`
    - [x] **标准库 (Standard Library)**
        - [x] 基础 I/O
        - [x] 内存管理
        - [x] 核心容器类型 (`String`, `Vec`, `Option`, `Result`)

## 🛠 构建与使用

### 支持平台 (Target Platforms)
Toka 编译器及其标准库已正式支持以下平台：
- **macOS** (x86_64 / arm64)：通过 `kqueue` 事件循环实现原生高并发支持。
- **Linux** (x86_64)：基于 `epoll` 原生事件循环。

### 极速安装 (官方推荐)

为了获得最流畅的开箱即用体验，您可以直接使用一键安装脚本（自动拉取编译好的 `tokac` 编译器、`toka` 原生包管理器以及标准库）：

```bash
curl -fsSL https://tokalang.dev/install.sh | bash
```

脚本将自动识别并下载适配您操作系统的版本，同时将 `PATH` 以及 `TOKA_LIB` 基底环境变量注入到您的 `.zshrc` 或 `.bashrc` 中。

### 前置要求
- **C++17** 兼容的编译器 (Clang/GCC)
- **CMake** 3.15+
- **LLVM 20** (Libraries and Headers, 本项目深度依赖 LLVM 20 的 Opaque Pointers 与协程 Intrinsics 特性)

### 构建编译器
```bash
# 1. 创建构建目录
mkdir -p build && cd build

# 2. 通过 CMake 配置
cmake ..

# 3. 编译
make
```

### 运行 Toka 程序
目前，`tokac` 将 `.tk` 源文件编译为 LLVM IR (`.ll`)。你可以使用 LLVM 解释器 (`lli`) 执行它们，或者使用 `clang` 进一步编译。

**一键编译并运行:**
```bash
./build/bin/tokac tests/test_trait.tk > output.ll && lli output.ll
```

## 📄 示例代码 (Examples)

### 1. 安全的高并发流式异步处理
Toka 的正交内存标记（如表示可写的 `#`）与异步环境相结合，配合原生错误传播符 `!`，使得服务端代码变得极为优雅：

```rust
import std/io::println
import std/net::TcpStream
import stdx/websocket::ws_accept_async
import core/result::Result

fn handle_connection(stream#: TcpStream) -> async Result<(), String> {
    // 抛弃所有的 .unwrap() 与 丑陋的报错检查
    // 通过 .await!，任何阻塞错误都会被安全且及时地抛向异常栈
    auto ws_conn# = ws_accept_async(stream).await!
    println("Server: [New Client Connected]")
    
    while true {
        auto msg = ws_conn#.read_text_async().await!
        if msg.len() == 0 {
            println("Server: Peer gracefully closed payload")
            break
        }
        println("Received: {}", msg)
        ws_conn#.write_text_async(msg.as_view()).await!
    }
    
    // 连接对象在此处将被 RAII 系统自动深沉销毁清理！
    return Result<(), String>::Ok(())
}
```

### 2. 代数数据类型 (ADTs) 与 模式匹配

```rust
import std/io::println

// 结构化形态设计 (Structural Shape)
shape State (
    Running |
    Stopped(i32)
)

fn main() -> i32 {
    auto s = State::Stopped(404)
    
    // 天然的模式匹配机制
    match s {
        auto Stopped(code) => println("System stopped with code: {}", code),
        _ => println("System is running normally...")
    }
    
    // 显式空指针安全 ('nul' 关键字) 与守卫解包 ('guard')
    auto nul ^ptr = null
    guard ^ptr {
        // 在该安全块内，指针已被证实非空且可用
        println("Valid pointer handled.")
    }

    return 0
}
```

## 致敬与灵感来源 (Inspirations & Lineage)

Toka 是一门站在巨人肩膀上的现代系统级编程语言。它的诞生并非为了重新发明轮子，而是为了在前辈们的智慧基础上，探索**高性能**与**开发体验**之间的最佳平衡。

我们诚挚地致敬以下为 Toka 勾勒出核心骨架的编程语言先驱。我们致力于完全公平地溯源，并毫不隐瞒这些语言对 Toka 产生的心智影响与启迪：

### 1. C & C++ (Modern C++)
**核心启迪：极速的系统控制权、智能指针与 RAII (起源)**
Toka 的内存管理机制直接从 Modern C++ (C++11/14+) 一脉相承，并紧密继承了 C 语言剥离一切多余包装的底层控制力。
*   **语法级别的智能指针**: Toka 将 `std::unique_ptr` 和 `std::shared_ptr` 的语义直接熔铸为**核心前缀语法**。使用 `^T`（独占指针）和 `~T`（共享指针），开发者不必承受模板代码的视觉冗余即可享受完整的 RAII。它让资源安全首次变成一种“零视觉负担”的默认标准。
*   **深核控制力**: Toka 依然保留了裸指针 (`*T`) 和对内存布局的精确操控，贯彻着 C/C++ "Zero-overhead abstraction (零成本抽象)" 的古老信条。

### 2. Rust
**核心启迪：所有权与借用安全法则 (Borrowing Discipline)**
Rust 证明了无 GC 的极致内存安全是可以达成的，并为此后的系统级语言确立了必须跨越的标准。Toka 从 Rust 中汲取了最精华的所有权安全规则验证理念，但在工程实现上做出了截然不同、以极低心智阻力为侧重点的架构取舍。
*   **借用检查规则**: 我们忠实地引进了极为严苛的 **"读写互斥"借用规则** (同一作用域内，允许多个不可变引用并存，或仅存在唯一的可变引用)，从编译期根本上掐断了数据竞争 (Data Races) 和内存泄漏。
*   **重塑心智模型**: 与 Rust 对于一切关联依赖都要求显式标注极其复杂的生命周期生命线（例如 `<'a>`）不同，Toka **蓄意抛弃了生命周期泛型符号**。取而代之的是，Toka 将这项繁重的推理交给了隐藏在幕后的 **PAL 检查器** 和更加平缓的面向对象词法作用域。这使得开发者在享受同级别安全性的同时，免除了与编译器展开拉锯战的代码噪音。

### 3. Haskell / ML Family
**核心致敬：类型类与正交设计 (Typeclasses & Orthogonality)**
Toka 的 Trait 系统在精神上继承自 **Haskell 的 Typeclasses**（而非传统的 OOP 接口）。
*   **数据与行为分离**：我们推崇将“数据定义”(`shape`)与“行为实现”(`impl`)完全剥离。这种设计赋予了 Toka 极强的扩展性——你甚至可以为标准库中的类型实现自定义的 Trait。
*   **代数数据类型 (ADTs)**：Toka 的 `shape` 完美复刻了 ML 家族对数据结构的精确建模能力。

### 4. Swift / Kotlin / C#
**核心致敬：空安全 (Null Safety)**
为了彻底解决“十亿美元的错误 (The Billion Dollar Mistake)”，Toka 采用了现代应用级语言通用的 **显式空安全** 设计。
* **类型级防御**：通过后缀修饰符 (`?`)，我们将空指针检查强制提升到了类型系统层面，而非依赖运行时的防御性编程。

### 5. Python
**核心致敬：可读性与开发效率 (Readability & Productivity)**
Toka 致力于将脚本语言的开发体验带入系统编程。我们借鉴了 Python **极简的语法风格**和“少即是多”的设计哲学，力求让系统级代码也拥有脚本般的清晰度与亲和力。

### 6. 万有引力与未尽的致敬 (Extended Gratitude & Convergent Evolution)
编程语言的设计是一场在浩瀚星海中不断传承的探索。由于篇幅限制，我们无法在此穷尽每一项特性的初创来源（例如，Toka 对 `async/await` 的实现机制与 C# 有些许异曲同工，其务实的模块与包管机制也难免带有 Go 或 Zig 的影子）。
如果 Toka 展现的某些创新点恰好与软件工程史上其他伟大语言发生了“趋同进化 (Convergent Evolution)”，或者我们遗漏了某些同样闪耀的参考溯源，请相信这绝非刻意的借鉴隐瞒。Toka 拥抱并崇尚开源社区汇聚的集体智慧，并对一切推动了程序设计学发展的先行者们保持着最虔诚的敬畏。

---

### 特别致敬 (Special Acknowledgement)

**AI 辅助工程 (AI-Assisted Engineering)**
Toka 语言的编译器实现——特别是类型系统的重构 (Type System Refactoring) 与代码生成适配 (CodeGen Adaptation)——是在 **Google Gemini** 的深度协作下完成的。

这种“人类架构师 + AI 结对编程”的开发模式，极大地加速了 Toka 从设计概念走向工业级实现的进程。所有的架构决策、设计哲学及核心逻辑均由人类作者主导，而 AI 则在代码实现与测试验证中提供了不可或缺的助力。