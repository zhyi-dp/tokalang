基于我们长达 7 小时的调试、针对编译器后端源码的对照，以及你刚才补充的 可变性修饰符 (#) 和 初始化豁免原则，这是 Toka 内存模型与指针协议的 最终定稿 (Final Specification)。

这份文档将作为 Toka 编译器的“宪法”，指导后续所有的开发与规范制定。

Toka 语言内存模型与指针协议白皮书

(Toka Memory Model & Pointer Protocol Standard)

版本: 3.0 (Final Constitution)
核心哲学: 单帽定死类型，状态掌管生死，修饰符决定权限。
物理基石: 严格映射编译器后端 CodeGen (Identity/Soul) 与 Sema (Mutability Check)。
1. 核心世界观：灵肉二元论 (The Identity-Soul Duality)

Toka 的内存世界在编译期通过 符号表 (Symbol Table) 严格区分两种实体。

1.1 Identity (句柄/身份)
符号: 有帽 (^p, ~p, &p, *p)
物理本质: 位于 栈 (Stack) 上的“盒子” (指针变量本身, 8 Bytes)。
编译器映射: getIdentityAddr(Symbol) -> 返回栈地址。
权责: 负责生命周期管理、指向关系维系、所有权流转。
1.2 Soul (灵魂/实体)

符号: 无帽 (p 或 x)

物理本质: 数据内容本身 (The Content)。

堆上 Soul: 匿名数据块，必须依附于某个 Identity。

栈上 Soul: 具名值变量 (auto x = 10) 的数据部分。

编译器映射: projectSoul(Identity) -> 返回数据地址 (隐式解引用或直接偏移)。

权责: 负责数据的读取、写入、字段访问。

2. 也是唯一的法则：单帽公理 (The Single Hat Axiom)

Toka 禁止多重戴帽。

定义: 任何变量声明只能有一层帽子。

推论: 不存在 **p、&^p 或 &&p。

编译器类型系统只处理一层 Indirection。

Design Note: 如果需要多级间接，请使用结构体封装。

原生标识: 每个 Soul 在编译期只有 唯一 的原生 Identity 类型。

3. 帽子变换协议 (The Hat Protocols)

这是连接 Identity 与 Soul，以及不同 Identity 之间的桥梁。

3.1 脱帽 (Decapping)：隐式解引用
操作: 去掉 Identity 的帽子 (^p -> p)。
适用性: 四种 Identity 全面适用 (^, ~, &, *)，无例外。
语义: 获取 Identity 指向的 Soul。
编译器行为: 自动插入 Load 指令获取目标地址，并在必要时进行 NullCheck。
3.2 带帽 (Encapping)：栈上反向指涉
操作: 给 栈上 Soul (auto x = 10) 加帽子。
限制: 仅允许生成 借用类 (&x) 或 原始类 (*x) Identity。
禁令: 绝对禁止 给栈上 Soul 戴 ^ (Unique) 或 ~ (Shared) 帽子。
理由: 栈内存不由 Allocator 管理，无法转交堆所有权 (不可 Free)。
3.3 借用帽 (The Borrowing Hat)：as 语法

这是实现零成本安全的核心枢纽。允许从任意堆 Identity 派生出引用 Identity。

语法: let &r = Identity_Owner as & (或 as &TargetType)
场景: let &x = ^p as &Node
物理动作 (Runtime): Bitwise Copy。将 ^p 的地址位拷贝给 &x。
法律动作 (Compile-time):
Lock: 原主 ^p 进入 Frozen 状态。
Grant: &x 获得 Active 状态。
Unlock: &x 销毁时，^p 解冻。
4. 内存来源与所有权 (Origin & Ownership)

所有权逻辑由 Identity 类型、LHS 可变性符号 及 赋值语境 共同决定。

特别公理：初始化豁免 (Initialization Exemption)

在变量声明 (auto ... = ...) 时和初始化列表中的字段初始化赋值，豁免 # (Mutable) 检查。
只有在后续赋值语句 (AssignExpr) 中，才强制检查 LHS 是否具备 # 权限。
4.1 堆主权者 (Heap Sovereigns) —— new 产物

只能指向堆 Soul，拥有所有权。

符号	语义	关键行为 (Init vs Rebind)
^p	Unique (独占)	Init: auto ^q = ^p (Move, p置空)



Rebind: ^#q = ^p (Move, q原指向被Free, p置空)



Error: ^q = ^p (若 q 无 #, 禁止重绑) |
| ~p | Shared (共享) | Init: auto ~q = ~p (Share, RefCount++)



Rebind: ~#q = ~p (Share, q原指向DecRef, p指向IncRef)



Error: ~q = ~p (若 q 无 #, 禁止重绑) |

4.2 原始指针 (Raw Pointers) —— alloc 产物

不拥有所有权，不负责生命周期 (Unsafe)。

符号	语义	关键行为
*p	Raw (裸指)	Init: auto *q = *alloc(N)



Rebind: *#q = *p (Bit Copy, 无生命周期副作用)



来源: 通常由 alloc 产生，用于数组/Buffer。 |

4.3 通用借用 (Universal Borrow) —— as & 产物

不拥有所有权，只负责临时访问。

符号	语义	关键行为
&p	Ref (引用)	Init: let &r = ^p as &



Rebind: &#r = ^q as & (r 转而借用 q, q冻结)



注意: 上述#符号代表可写权限，权限符号还有$(不可写/不可重绑定，且不可空）?(增加可空属性) #(增加可变属性）!(增加可空及可变属性）。

属性符号可以出现在帽子之后（修饰帽子本身），也可以出现在 soul 名字之后（修饰 soul）

可变在不同位置的语义分别为：可重绑定 / soul可修改

可空在不同位置的语义分别为：可为 nullptr / soul 可为 none

因为!包含了可变性，所以上述代码例子中的所有#替换为!仍然符合语法规定。上述解释中的简写表达 若无#若表达完整时应为`若无#或!`

强制规定：重绑定或者修改灵魂是 必须显示`出示`可变属性符号。当然能够出示的前提时，该 Identiy 或者 Soul 在出生时是否具备相应的可变属性

强制规定：如果未来我们支持 move 关键字移动所有权，那么解引用得到的 Soul（相对于原生 Soul）不可被 move

|

5. 符号速查表 (The Sigil Matrix)

一张表看懂语义分析 (Sema) 与代码生成 (CodeGen) 的逻辑：

场景	代码范例	语义	编译器检查 (Sema)	底层行为 (CodeGen)
Init (Move)	auto ^q = ^p	初始化移动	豁免 # 检查	Store(Slot_q, Load(Slot_p)); Zero(Slot_p);
Rebind (Move)	^#q = ^p	重绑移动	**检查 ^ 是否带 #**	Free(Load(Slot_q)); Store(Slot_q, Load(Slot_p)); Zero(Slot_p);
Rebind Error	^q = ^p	非法重绑	❌ 报错: Immutable Identity	(不生成)
Mutate Soul	q# = 10	修改数据	检查 q 是否带有#	Store(Load(Slot_q), 10) (隐式解引用)
Borrow Init	auto &r = ^p as &	借用初始化	豁免 #, 标记 p Frozen	Store(Slot_r, Load(Slot_p));
Arg Capture	foo(p#)	原位登基	标记 p Frozen	(无数据移动，仅控制权视作 Active)
6. 编译器实现的“歧义消解” (Implementation Logic)

问题: p# = 10 (脱帽指针) 和 x# = 10 (栈值变量) 代码形式一致，如何区分？

解决方案: 类型系统是唯一的仲裁者。

解析阶段 (Parser): 统一生成 AssignExpr。
语义阶段 (Sema):
查找 LHS 符号的 Type。
分支 A (Value Type): x 是 i32。标记为 Direct Access。
分支 B (Pointer Type): p 是 ^i32。标记为 Implicit Dereference。
分支 C (Error): 如果 p 是 ^i32 且 RHS 是 ^i32 (带帽)，但 LHS 写法是 p = ... (脱帽)，报错 "Type Mismatch: Cannot assign Identity to Soul"。
生成阶段 (CodeGen):
Direct Access: Store(getIdentityAddr(x), 10)。
Implicit Deref: Store(Load(getIdentityAddr(p)), 10) (带 NullCheck)。

结语

这份文档确立了 Toka 的 三位一体：

Identity (句柄) 是权力的容器。
Soul (灵魂) 是数据的载体。
# (修饰符) 是变更权力的签证。

Keep Fixing, Keep Shipping! 🚀