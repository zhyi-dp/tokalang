# Toka 统一 Shape 与块表达式设计规范 (v1.5)

本文档旨在详述 Toka 语言中 `shape`（结构化数据）以及 `pass`（块返回值）的设计细节，为编译器开发提供直接指导。

## 1. 统一的 Shape 定义语法

Toka 使用统一的 `shape` 关键字定义所有结构化数据，通过括号内的结构自动判定实体种类。

| 语法特征 | 判定 Kind | 访问约束 |
| :--- | :--- | :--- |
| `shape Name(x: T, y: U)` | **Struct** | 直接通过 `.name` 访问 |
| `shape Name(T, U)` | **Tuple** | 通过 `.0`, `.1` 访问 |
| `shape Name(A \| B(T) \| C = N)` | **Enum (Tagged)** | **强制** 必须通过 `match` 或 `if auto` 访问 |
| `shape Name(as T \| as name: T)` | **Union** | `as` 重解释 (Bare) 或 `.name` (Named) |
| `alias Name = [T; N]` | **Array** | 通过 `[index]` 访问 |

> [!IMPORTANT]
> **注意**：在 Toka 中，固定长度数组通过 `alias` 关键字定义，而非 `shape`。这表明数组在 Toka 中主要作为类型别名存在，不具有 Shape 的 Nominal Identity。

---

## 2. Union (联合体) 详解

Union 用于内存重叠的底层开发，不存储 Tag。Toka 支持 **Bare Union** (匿名变体) 和 **Named Union** (命名变体)。

### 2.1 定义语法

#### 风格 A: Bare Union (经典纯裸模式) -> 推荐用于简单转换
```scala
shape Word(
    as u32 |    // 只有类型，没有名字
    as f32      // 必须用 as f32 访问
)
```

#### 风格 B: Named / Hybrid Union (新特性) -> 推荐用于复杂数据
```scala
shape Packet(
    as raw: u32 |                  // Named: 可用 .raw 访问
    as (header: u8, body: u16) |   // Bare: 必须用 as (u8, u16) 访问
    as payload: (h: u8, b: u16)    // Named Struct: 可用 .payload 访问
)
```

- **Bare Variant**: `as T`。完全匿名，仅提供类型转换。
- **Named Variant**: `as name: T`。提供语义化名称，支持 `.name` 访问。
- **混合使用**: 你可以在同一个 Union 中自由混合这两种写法。
- 物理布局：大小为 `max(sizeof(variants))`，对齐为 `max(alignof(variants))`。

### 2.2 安全规则 (Safety Rules)

为了保证 Union 的纯粹性和安全性，Toka 实施了严格的限制：

#### A. 资源隔离 (Data-Only)
Union **严禁包含**任何管理资源的类型 (HasDrop)。
- **❌ 禁止**：智能指针 (`^T`, `~T`)。
- **❌ 禁止**：实现了 `drop` 方法的自定义结构体。
- **❌ 禁止**：包含上述类型的数组或结构体。
- **✅ 允许**：裸指针 (`*T`) 和 `ManuallyDrop<T>` (未来特性)。

这一规则杜绝了 Union 切换变体时的资源泄漏隐患。

#### B. 类型黑名单
为了防止值域陷阱，Union **禁止**包含非位模式完备（Non-Bit-Pattern-Complete）的类型：
- **❌ 禁止**：`bool` (位表示不透明，值域仅 0/1)。
- **❌ 禁止**：`strict enum` (值域不连续)。

#### C. 严格初始化
Union 初始化时，提供的值的大小必须 **严格等于** Union 的总大小。
- 防止部分初始化导致的脏数据残留。
- 必须填充整个内存空间 (可通过 padding 或 0 扩展实现)。

#### D. 空 Union 与 ZST
- **✅ 允许**：Empty Union `shape VoidUnion()` 是合法的，大小为 0。
- **✅ 允许**：Union 成员可以是 ZST (Zero Sized Type)，不占用物理空间。

### 2.3 构造函数匹配规则 (Named & Bare)
- **Named Constructor**: `Bytes4(head = 0xFFFF)` -> 优先匹配名字 `head`。
- **Type-based Constructor**: `Bytes4(0xFFFFFFFF)` -> 匹配类型 `u32`。
- **字面量安全适配**: 如果字面量数值在目标类型范围内，视为匹配 (如 `0` 匹配 `u32`)。

### 2.4 访问与重解释 (Reinterpretation)

#### 2.4.1 As Cast (通用)
任何 Union 都可以通过 `as` 转换为变体类型：
```scala
auto u = Bytes4(0xFF)
auto b = u as u32 // OK
```

#### 2.4.2 Dot Access (Named Only)
命名变体支持语义化访问，编译器自动处理左值转换：
```scala
auto val = Bytes4(head = 10)
val.head = 20 // ✅ 直接修改内存！L-Value Preserved
```
**实现原理**：`val.head` 等价于 `*(val as *u16)`，但语法更干净。

> [!NOTE]
> **赋值 vs 初始化**
> *   **初始化 (Init)**：构造函数如 `Bytes4(...)` 是**全量覆盖** (Total Overwrite)，必须填充整个 Union 大小，确保无未定义数据。
> *   **赋值 (Assign)**：成员赋值如 `val.head = 20` 是**外科手术式** (Surgical Update)，仅修改该成员对应的内存字节，**保留其余字节不变**。这是 Union 用作位操作的核心特性。

---

## 3. 解构与形态 (Destructuring & Morphology)

Toka 的常规解构语法完全适用于 Union 重解释后的结果。

- **值解构**：`auto (x, y) = val` (拷贝)。
- **引用解构**：`auto (&x, &y#) = val`。
    - `&x`：绑定到特定内存位置的只读引用。
    - `&y#`：绑定到特定内存位置的可写引用。

---

## 4. `pass` 关键字与块表达式

`pass` 用于从代码块（`match` 分支、`if` 块、`loop` 等）中产生一个值。

### 4.1 核心规则
- **位置约束**：`pass` 语句必须是所属代码块中的**最后一个有效动作**。
- **显式要求**：**块表达式不接受隐式值返回，必须显式使用 `pass` 关键字。**
- **单行缩写**：对于单行分支，可以省略 `{}`，如 `Maybe::None => pass -1`。

### 4.2 类型完备性与发散 (Divergence)
分支类型的推导遵循 **Bottom Type Unification** 规则：
- **Bottom (Never) Type**：如果一个分支以发散语句 (`return`, `break`, `continue`, `panic`) 结尾，该分支类型视为 `Bottom`。
- **兼容性**：`Bottom` 类型兼容任何其他具体的 `pass` 类型。
    - `i32` vs `i32` -> `i32`
    - `i32` vs `Bottom` (return) -> `i32`
    - `Bottom` vs `Bottom` -> `Bottom`

### 4.3 示例
```scala
auto val = match res {
    auto Maybe::One(v) => {
        pass v // 产生值 v
    }
    Maybe::None => return // 发散，合法
}
```

---

## 5. 编译器实现要点 (CodeGen & Sema)

1.  **Sema (Union)**：
    - `checkShapeDecl`：**检查 HasDrop 成员并报错**；检查 Forbidden Types (`bool` only, u8 allowed)。
    - `checkCallExpr`：验证初始化值的大小是否等于 Union 大小 (Partial Init Check)。
    - `checkMemberExpr`：支持 Named Variant 的点访问，解析为对应的变体类型。
2.  **Sema (Control Flow)**：
    - `unifyBranches`：实现 Bottom Type 逻辑。使用 `allPathsJump(Stmt)` 来判定分支是否发散。
3.  **CodeGen (Union)**：
    - Union 统一生成为 `[MaxSize x i8]` 的结构。
    - `as` 操作符和 `Dot Access` 在 IR 层面统一实现为 `bitcast` (保证 L-Value)。
4.  **CodeGen (Pass)**：
    - 分支汇合处使用 `PHI` 节点或预先分配好的 `ResultAddr` 进行结果收集。


