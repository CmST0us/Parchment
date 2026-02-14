## Context

InkUI 框架 Phase 0-1 已完成，提供了：
- `Geometry`：Point, Size, Rect, Insets 基础几何类型
- `Event`：TouchEvent, SwipeEvent, TimerEvent 类型定义（tagged union）
- `Canvas`：裁剪绘图引擎，支持 fillRect / drawText / drawBitmap，含逻辑↔物理坐标变换
- `View`：完整的 View 树（addSubview / removeFromParent / dirty 冒泡 / hitTest / 触摸事件冒泡）
- `FlexLayout.h`：FlexStyle / FlexDirection / Align / Justify 类型定义（无算法实现）
- `EpdDriver`：C++ 单例 wrapper，封装 epdiy C API

当前 `main.cpp` 在 InkUI 验证测试后仍调用旧 `ui_core` 框架。本阶段补齐交互系统，使 InkUI 可独立运行。

**约束**：
- ESP32-S3, 8MB PSRAM, C++17 `-fno-exceptions -fno-rtti`
- FreeRTOS 实时环境，事件队列通信
- GT911 触摸驱动为 C 库（`gt911.h`），通过 I2C 读取
- 墨水屏刷新慢（DU ~200ms, GL16 ~1.5s, GC16 ~3s），必须最小化刷新区域

## Goals / Non-Goals

**Goals:**
- 实现 FlexBox 布局算法，支持 Column/Row 方向、flexGrow 弹性、gap/padding
- 实现 RenderEngine 5 阶段渲染循环，支持多脏区域独立刷新和残影管理
- 实现 GestureRecognizer，从 GT911 原始数据识别 Tap/LongPress/Swipe
- 实现 ViewController 生命周期 + NavigationController 页面栈
- 实现 Application 主循环，整合事件分发和渲染
- main.cpp 切换到 InkUI Application 入口

**Non-Goals:**
- 不实现具体 View 子类（TextLabel, ButtonView 等在 Phase 3）
- 不实现 OverlayView / backing store 机制（Phase 4）
- 不实现 FontManager / TextLayout（Phase 4）
- 不删除旧 ui_core 框架（Phase 5 再清理）
- 不添加网络、蓝牙等功能

## Decisions

### 1. FlexLayout 算法内联于 View::onLayout() 默认实现

**选择**：`View::onLayout()` 默认实现调用 `ink::flexLayout(this)` 自由函数

**替代方案**：
- 独立 LayoutManager 对象 → 过度设计，只需一种布局
- 子类各自实现 → 重复代码

**理由**：FlexBox 是唯一布局算法，嵌入默认实现最简洁。子类可 override onLayout() 做自定义布局。

### 2. RenderEngine 多脏区域独立刷新

**选择**：最多 8 个独立脏区域，相邻且同 hint 的合并，不同 hint 保持独立

**替代方案**：
- 单一 bounding box（旧方案）→ 两个小区域在屏幕两端退化为全屏刷新
- 无限脏区域 → 内存不可控

**理由**：8 个区域足够覆盖实际场景（header + 几个 list item + footer），固定数组零分配。

### 3. GestureRecognizer 作为独立 FreeRTOS 任务

**选择**：GestureRecognizer 运行在独立 FreeRTOS 任务中，通过 GT911 中断/轮询读取触摸数据，识别手势后发送 Event 到 Application 的事件队列

**替代方案**：
- 在 Application 主循环中轮询 → 阻塞渲染
- 仅用中断回调 → ISR 不能做复杂状态机

**理由**：沿用旧 `ui_touch.c` 验证过的架构（独立任务 + 中断唤醒 + 轮询追踪），最可靠。

### 4. ViewController 懒加载 View 树

**选择**：`getView()` 首次调用时触发 `viewDidLoad()`，之后直接返回缓存的 view_

**替代方案**：
- 构造函数中创建 View 树 → push 到栈前就分配内存
- 显式 init() 方法 → 容易忘调用

**理由**：UIKit 经典模式，资源在需要时才分配，ViewController 可提前创建。

### 5. NavigationController 不保存被 pop 页面

**选择**：pop() 时 ViewController 被 unique_ptr 销毁；push 的 VC 在栈中保活，返回时 viewWillAppear 重新激活

**替代方案**：
- 所有 VC 永久存活 → PSRAM 浪费
- pop 时序列化状态 → 过于复杂

**理由**：嵌入式内存有限，pop 即销毁是最安全的策略。栈中的 VC（被覆盖但未 pop 的）View 树保活，状态不丢失。

### 6. Application 事件队列超时 30 秒

**选择**：`xQueueReceive` 超时 30 秒，超时后仍执行 renderCycle（处理延迟事件等）

**理由**：墨水屏无需高帧率，30 秒空闲不刷新节省功耗。超时后检查是否有 pending layout/display。

## Risks / Trade-offs

- **[FlexBox 精度]** 整数除法导致像素累积误差 → 将余数分配给最后一个 flex 子项
- **[GestureRecognizer 线程安全]** 手势任务和 Application 主循环在不同线程 → 通过 FreeRTOS 队列通信，无共享可变状态
- **[残影计数器全局性]** 不同区域的刷新模式不同但共享计数器 → 可接受，GC16 全屏清一次即可
- **[ViewController 销毁时机]** pop 销毁 VC 可能在 VC 回调中发生 → pop() 不立即销毁，延迟到主循环下一轮
