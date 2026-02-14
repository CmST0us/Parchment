## Context

InkUI 阶段 1 Canvas 已完成，提供了裁剪绘图能力。现在需要 View 基类作为组件树的骨架，以及 Event 类型定义作为交互系统的基础。这是架构文档 §4.2 (View) 和 §4.10 (Event) 的实现。

View 基类是整个框架最核心的类——所有 UI 组件（TextLabel、ButtonView、HeaderView 等）都继承自它。设计需要精确，因为后续无法轻易修改基类的 API。

## Goals / Non-Goals

**Goals:**
- 实现 `ink::View` 基类：完整的树操作、几何属性、脏标记冒泡、命中测试、事件冒泡
- 定义 `ink::Event` 统一事件类型（TouchEvent, SwipeEvent, TimerEvent, Custom）
- 定义 `ink::RefreshHint` 枚举，View 携带刷新提示
- 定义 FlexBox 布局相关类型（FlexDirection, Align, FlexStyle 等），作为 View 成员字段
- View::onDraw 签名引用 Canvas，确保后续 RenderEngine 可以传入裁剪好的 Canvas

**Non-Goals:**
- 不实现 FlexLayout 布局算法（下一个 change）
- 不实现 RenderEngine（后续 change）
- 不实现 GestureRecognizer（后续 change）
- 不实现具体 View 子类（TextLabel 等）
- View::onLayout() 默认实现暂时为空（FlexLayout 实现后填充）

## Decisions

### 1. View 所有权模型

**选择**: 子 View 用 `std::vector<std::unique_ptr<View>>` 持有，parent 用 raw pointer。

**理由**: View 树天然适合独占所有权。unique_ptr 零运行时开销。parent 是非拥有引用，View 的生命周期由父 View 的 subviews_ vector 管理。避免 shared_ptr 的引用计数开销。

### 2. 脏标记冒泡策略

**选择**: `setNeedsDisplay()` 设置自身 `needsDisplay_=true`，然后沿 parent 链向上设置每个祖先的 `subtreeNeedsDisplay_=true`，直到已经为 true 的节点为止（短路）。

**理由**: RenderEngine 遍历树时，如果某节点 `subtreeNeedsDisplay_==false`，可以跳过整棵子树。短路避免重复冒泡到根。

### 3. hitTest 从底到顶返回最深命中

**选择**: `hitTest(x, y)` 递归搜索子 View（逆序遍历，后添加的在上层），返回最深层包含该点的可见 View。

**理由**: 后添加的 View 在视觉上覆盖先添加的（painter's algorithm 顺序）。逆序遍历确保点击时命中最上层 View。

### 4. onTouchEvent 冒泡链

**选择**: hitTest 找到目标 View 后，调用其 `onTouchEvent()`。如果返回 false（未消费），冒泡给 parent，直到某个 View 返回 true 或到达根。

**理由**: 与 Android/iOS 触摸事件模型一致。按钮消费 tap，列表消费 swipe，未处理的事件继续冒泡。

### 5. Event 使用 tagged union 而非继承

**选择**: `Event` 结构体包含 `EventType` 枚举 + union 成员，不用虚函数继承。

**理由**: Event 是高频创建的值类型，通过 FreeRTOS 队列传递。union 保持固定大小（可 memcpy），无堆分配。`-fno-rtti` 下无法用 dynamic_cast。

### 6. FlexLayout.h 仅包含类型定义

**选择**: 本次只定义 `FlexDirection`, `Align`, `Justify`, `FlexStyle` 等枚举和结构体，`flexLayout()` 函数声明但不实现。

**理由**: View 基类需要 `FlexStyle flexStyle_` 成员字段，所以类型定义必须先有。但布局算法是独立子问题，在下一个 change 中实现。

## Risks / Trade-offs

**[View 移动语义]** → View 声明为 movable (`View(View&&) = default`)，但移动后 parent 指针和子 View 的 parent_ 引用可能失效。
→ **缓解**: 实际使用中 View 通过 unique_ptr 管理，不会被移动。移动构造声明仅为编译器满足，实际不应使用。可考虑 delete 移动构造。

**[Event union 大小]** → union 大小取决于最大成员，TouchEvent 有 5 个 int 字段 = 20 bytes。
→ **缓解**: Event 总大小约 24 bytes，FreeRTOS 队列中完全可接受。
