## Context

InkUI 框架当前的 View 树结构以 `windowRoot_` 为根，采用 Column 布局顺序排列 `StatusBarView` 和 `contentArea_`。所有页面内容通过 NavigationController 管理，挂载到唯一的 `contentArea_` 槽位。框架没有 Z 轴叠加能力，无法在页面内容之上显示模态视图。

阅读器需要 Loading（加载中）、Toast（轻提示）、Alert（确认弹窗）、ActionSheet（操作菜单）等模态交互。这些模态具有不同的行为特征：Toast 不阻塞触摸且自动消失，Alert/ActionSheet/Loading 阻塞底层触摸。

硬件约束：4bpp 灰度 framebuffer，无 alpha 通道，墨水屏刷新代价高。

## Goals / Non-Goals

**Goals:**
- 在不改动 `windowRoot_` 以下现有 View 树的前提下，为 InkUI 添加模态视图叠加能力
- 支持 Toast（非阻塞）和 Modal（阻塞）两个独立通道，通道间可共存
- Modal 通道内支持优先级队列（Alert > ActionSheet > Loading）
- 提供 ShadowCardView 通用阴影卡片组件，在 4bpp 灰度下实现投影悬浮效果
- 模态关闭后正确修复底层内容（局部 repairDamage，不全屏刷新）

**Non-Goals:**
- 不实现具体的 Alert/ActionSheet/Loading/Toast 内容视图（本次只做框架层）
- 不实现半透明全屏遮罩（墨水屏代价太高，靠阴影提供层级区分）
- 不支持模态嵌套（模态上再弹模态）
- 不改动 NavigationController 的逻辑

## Decisions

### Decision 1: screenRoot_ 包裹层方案（方案 B 完整版）

在 `windowRoot_` 外新增 `screenRoot_` 作为最顶层根节点，`overlayRoot_` 与 `windowRoot_` 并列为其子节点。两者通过 Z 序叠放实现分层。

```
screenRoot_ (FlexDirection::None, 540x960)
├── windowRoot_ (Column, frame={0,0,540,960})
│   ├── StatusBarView
│   └── contentArea_
│       └── [VC View]
└── overlayRoot_ (FlexDirection::None, frame={0,0,540,960}, bgColor=Clear)
    ├── [Toast View]   ← 由 ModalPresenter 管理
    └── [Modal View]   ← 由 ModalPresenter 管理
```

**替代方案**: 方案 A（给 View 加 absolute 定位属性，overlay 直接挂在 windowRoot_ 下）。未选择因为多通道场景会导致 Column 容器中多个 absolute 子节点，概念混乱且不 scale。

**替代方案**: 方案 C（给 FlexLayout 加 Stack 布局模式，windowRoot_ 变成 Stack 容器）。未选择因为改动最大，需要把 StatusBar + contentArea 包到新的 mainLayer 中，且 Stack 布局目前只有模态一个使用场景，过度工程。

### Decision 2: FlexDirection::None 实现自由定位

给 `FlexDirection` 枚举新增 `None` 值。`flexLayout()` 遇到 `direction == None` 时，**不对子节点做定位计算**，仅递归调用子节点的 `onLayout()`。子节点保留手动设置的 frame。

这是最小化的改动，只影响 FlexLayout 一处 switch/if 判断。`screenRoot_` 和 `overlayRoot_` 都使用此模式。

### Decision 3: ModalPresenter 独立类

新增 `ink::ModalPresenter` 类，从 Application 中分离模态管理职责。Application 持有 `ModalPresenter` 实例并暴露访问器。

依赖关系：
```
ModalPresenter 持有:
  - View* overlayRoot_          (非拥有, 用于添加/移除模态 View)
  - View* screenRoot_           (非拥有, repairDamage 的根)
  - RenderEngine& renderer_     (用于 repairDamage)
  - Application& app_           (用于 postDelayed 定时器)
```

ViewController 通过 `app.modalPresenter()` 访问模态能力。

### Decision 4: 双通道调度

| 通道   | 位置      | 阻塞触摸 | 自动消失 | 包含类型                  |
|--------|-----------|----------|----------|--------------------------|
| Toast  | 顶部居中   | 否       | 是       | Toast                    |
| Modal  | 屏幕居中   | 是       | 否       | Alert, ActionSheet, Loading |

通道间可共存（Toast + Alert 同时显示）。通道内排队：

- **Toast 通道**: 先进先出队列，当前 Toast 消失后显示下一个
- **Modal 通道**: 优先级队列。高优先级（Alert）进入时，暂存当前低优先级（Loading），高优先级关闭后恢复低优先级

Modal 通道内优先级：`Alert (2) > ActionSheet (1) > Loading (0)`。

### Decision 5: 仅阴影不做全屏遮罩

模态窗使用 ShadowCardView 绘制投影阴影，不使用全屏半透明遮罩。

理由：
- 4bpp framebuffer 无 alpha 通道，遮罩意味着直接覆盖底层像素
- 关闭模态后全屏重绘代价过高（墨水屏刷新明显）
- 阴影本身已提供足够的层级区分和悬浮感

触摸拦截通过 Application 的逻辑层判断（`ModalPresenter::isBlocking()`），不依赖视觉遮罩。

### Decision 6: 事件拦截策略

**Touch 事件**:
1. hitTest 从 `screenRoot_` 开始（原来从 `windowRoot_`）
2. overlayRoot_ 是最后添加的子节点，hitTest 自然先检查它
3. 如果 `ModalPresenter::isBlocking()` 为 true，且 hitTest 目标不在 overlayRoot_ 子树中，则吞掉触摸

**Swipe 事件**:
- 如果 `isBlocking()` 为 true，吞掉 swipe（不传给 `navigator_.current()`）

**Timer 事件**:
- 先交给 `ModalPresenter::handleTimer()`，如果消费则不再传给 VC

### Decision 7: 阴影绘制算法

ShadowCardView 在 `onDraw()` 中绘制：
1. 先用白色填充整个 frame（清除底层残留）
2. 绘制右偏+下偏的灰度渐变阴影（扩散 6-8px，灰度从 10→14 渐变到白色）
3. 绘制白色卡片矩形覆盖阴影内部
4. 子 View 在卡片内部区域通过 FlexBox 布局

阴影参数：偏移 (2px, 2px)，扩散 8px，4 级灰度渐变。

### Decision 8: 模态关闭修复流程

1. 记录模态 View 的 screenFrame（含阴影区域）
2. 从 overlayRoot_ 移除模态 View（unique_ptr 析构）
3. 如有队列中的下一个模态，添加到 overlayRoot_
4. 调用 `renderEngine_.repairDamage(screenRoot_, damageRect)` 修复底层内容

`repairDamage` 会从 screenRoot_ 开始递归重绘所有与 damageRect 相交的 View，然后局部刷新该区域。模态已从树中移除，所以不会被重绘，底层 windowRoot_ 的内容会被正确恢复。

## Risks / Trade-offs

- **[脏区域上限]** RenderEngine 最多跟踪 8 个脏区域。Toast + Modal 同时出现时，加上底层 View 的更新，可能接近上限。→ 缓解：模态通常是静态的，不会频繁触发 setNeedsDisplay()；现有合并逻辑会处理重叠区域。
- **[Timer ID 冲突]** ModalPresenter 使用 Timer 事件做 Toast 自动消失。需要确保 timerId 不与 VC 自定义的 timerId 冲突。→ 缓解：使用高位 timerId（如 0x7F00 起），并在 handleTimer 中判断范围。
- **[FlexDirection::None 的边界行为]** None 模式下子节点的 intrinsicSize / flexGrow 等属性无意义，但不影响正确性（flexLayout 直接跳过定位阶段）。
- **[repairDamage 重绘范围]** 如果模态 View 很大（如全屏 ActionSheet），repairDamage 等效于全屏重绘。→ 可接受，ActionSheet 关闭本身就需要大面积刷新。
