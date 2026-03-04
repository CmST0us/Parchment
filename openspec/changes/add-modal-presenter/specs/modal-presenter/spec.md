## ADDED Requirements

### Requirement: ModalPresenter 类
`ink::ModalPresenter` SHALL 作为独立类负责模态视图的呈现、队列调度、事件拦截和关闭修复。ModalPresenter 由 Application 持有，ViewController 通过 `app.modalPresenter()` 访问。

ModalPresenter 构造时 SHALL 接受以下依赖：
- `View* overlayRoot` — 叠加层根 View（非拥有）
- `View* screenRoot` — 顶层根 View（用于 repairDamage）
- `RenderEngine& renderer` — 渲染引擎（用于 repairDamage）
- `Application& app` — 应用实例（用于 postDelayed 定时器）

#### Scenario: 通过 Application 访问 ModalPresenter
- **WHEN** ViewController 需要显示模态视图
- **THEN** 通过 `app_.modalPresenter()` 获取 ModalPresenter 引用，调用其呈现方法

### Requirement: 双通道模态系统
ModalPresenter SHALL 管理两个独立的模态通道：

| 通道           | 枚举值                    | 位置     | 阻塞触摸 |
|----------------|--------------------------|----------|----------|
| Toast 通道     | `ModalChannel::Toast`    | 顶部居中  | 否       |
| Modal 通道     | `ModalChannel::Modal`    | 屏幕居中  | 是       |

两个通道 SHALL 可以同时各显示一个模态视图（如 Toast + Alert 共存）。

#### Scenario: Toast 和 Alert 共存
- **WHEN** Modal 通道正在显示 Alert，此时 showToast() 被调用
- **THEN** Toast 显示在屏幕顶部，Alert 保持在屏幕中央，两者同时可见

#### Scenario: 通道独立管理
- **WHEN** Toast 通道有模态显示，调用 `dismiss(ModalChannel::Modal)`
- **THEN** 仅 Modal 通道受影响，Toast 通道不变

### Requirement: Toast 呈现
`showToast(std::unique_ptr<View> content, int durationMs)` SHALL 在 Toast 通道显示模态视图。

行为：
- 将 content 包装在 ShadowCardView 中（或直接使用传入的 View）
- 定位于屏幕顶部居中（StatusBar 下方，留 margin）
- 不阻塞底层触摸事件
- 在 `durationMs` 毫秒后自动关闭（默认 2000ms）
- 如果 Toast 通道已有模态显示，新请求 SHALL 入队等待当前 Toast 消失后显示

#### Scenario: 显示 Toast
- **WHEN** 调用 `showToast(view, 2000)`，Toast 通道空闲
- **THEN** view 显示在屏幕顶部居中，2 秒后自动消失

#### Scenario: Toast 排队
- **WHEN** 调用 `showToast(viewA, 2000)`，随后立即调用 `showToast(viewB, 2000)`
- **THEN** viewA 先显示 2 秒后消失，viewB 接着显示 2 秒后消失

#### Scenario: Toast 不阻塞触摸
- **WHEN** Toast 正在显示，用户触摸 Toast 区域以外的内容区域
- **THEN** 触摸事件正常传递给底层 View

### Requirement: Modal 呈现（Alert / ActionSheet / Loading）
ModalPresenter SHALL 提供以下方法呈现阻塞型模态：
- `showAlert(std::unique_ptr<View> content)` — 优先级 2（最高）
- `showActionSheet(std::unique_ptr<View> content)` — 优先级 1
- `showLoading(std::unique_ptr<View> content)` — 优先级 0（最低）

行为：
- 将 content 包装或定位于屏幕居中
- 阻塞底层所有触摸和 swipe 事件
- 不自动消失，需显式调用 `dismiss(ModalChannel::Modal)` 关闭

#### Scenario: 显示 Alert
- **WHEN** 调用 `showAlert(view)`，Modal 通道空闲
- **THEN** view 显示在屏幕居中，底层触摸被阻塞

#### Scenario: 显示 Loading
- **WHEN** 调用 `showLoading(view)`，Modal 通道空闲
- **THEN** view 显示在屏幕居中，底层触摸被阻塞

### Requirement: Modal 通道优先级队列
Modal 通道内 SHALL 按优先级调度：`Alert (2) > ActionSheet (1) > Loading (0)`。

规则：
- 高优先级请求进入时，如果当前显示的是低优先级模态，SHALL 暂存当前模态（从 overlayRoot_ 移除，保留在队列中），显示高优先级模态
- 高优先级关闭后，SHALL 恢复显示被暂存的低优先级模态（如果仍在队列中）
- 同优先级请求 SHALL 入队等待当前模态关闭后显示

#### Scenario: Alert 打断 Loading
- **WHEN** Loading 正在显示（优先级 0），调用 `showAlert(alertView)`（优先级 2）
- **THEN** Loading 被暂存，Alert 显示在屏幕居中

#### Scenario: Alert 关闭后恢复 Loading
- **WHEN** Alert 打断了 Loading 后，调用 `dismiss(ModalChannel::Modal)`
- **THEN** Alert 关闭，Loading 恢复显示（如果未被取消）

#### Scenario: 同优先级排队
- **WHEN** Alert A 正在显示，调用 `showAlert(alertB)`
- **THEN** Alert B 入队等待，Alert A 关闭后 Alert B 自动显示

### Requirement: 模态关闭
`dismiss(ModalChannel channel)` SHALL 关闭指定通道的当前模态视图。

关闭流程：
1. 记录当前模态 View 的 screenFrame（含阴影区域）作为 damageRect
2. 从 overlayRoot_ 移除当前模态 View
3. 如果通道队列中有下一个模态，将其添加到 overlayRoot_ 并定位
4. 调用 `RenderEngine::repairDamage(screenRoot_, damageRect)` 修复底层内容

#### Scenario: 关闭 Alert 无后续队列
- **WHEN** 调用 `dismiss(ModalChannel::Modal)`，Modal 队列为空
- **THEN** Alert 从 overlayRoot_ 移除，底层内容修复刷新

#### Scenario: 关闭 Alert 有后续队列
- **WHEN** 调用 `dismiss(ModalChannel::Modal)`，队列中有 Loading 等待恢复
- **THEN** Alert 移除，Loading 恢复显示，底层被 Alert 覆盖的区域修复

#### Scenario: 关闭无活跃模态的通道
- **WHEN** 调用 `dismiss(ModalChannel::Toast)`，但 Toast 通道当前无模态显示
- **THEN** 无操作，不崩溃

### Requirement: 阻塞状态查询
`isBlocking()` SHALL 返回 true 当且仅当 Modal 通道有活跃的模态视图正在显示。Toast 通道不影响 isBlocking() 的返回值。

#### Scenario: Modal 通道有 Alert
- **WHEN** Alert 正在显示
- **THEN** `isBlocking()` 返回 true

#### Scenario: 仅 Toast 显示
- **WHEN** 只有 Toast 在显示，Modal 通道空闲
- **THEN** `isBlocking()` 返回 false

#### Scenario: 无模态显示
- **WHEN** 两个通道都空闲
- **THEN** `isBlocking()` 返回 false

### Requirement: Toast 自动消失定时器
Toast 呈现时 SHALL 通过 `Application::postDelayed()` 发送延迟 Timer 事件。ModalPresenter SHALL 使用保留的 timerId 范围（0x7F00 起）避免与 VC 自定义 timer 冲突。

`handleTimer(int timerId)` SHALL 检查 timerId 是否属于 ModalPresenter 管理的范围，如果是则处理 Toast 自动消失并返回 true，否则返回 false。

#### Scenario: Toast 定时器触发
- **WHEN** Toast 显示 2 秒后 Timer 事件到达
- **THEN** `handleTimer()` 返回 true，Toast 通道执行 dismiss 流程

#### Scenario: 非 ModalPresenter 的 Timer
- **WHEN** timerId 不在 ModalPresenter 保留范围内
- **THEN** `handleTimer()` 返回 false，事件继续传递给 VC

### Requirement: 模态 View 定位
ModalPresenter SHALL 根据通道类型自动计算模态 View 的 frame：
- **Toast 通道**: 水平居中，垂直位于 StatusBar 下方（y = 20 + margin）
- **Modal 通道**: 水平居中，垂直居中于屏幕

模态 View 的宽高 SHALL 由 View 的 `intrinsicSize()` 或手动设置的 frame 尺寸决定。ModalPresenter 只负责设置位置（x, y），不改变尺寸。

#### Scenario: Alert 居中定位
- **WHEN** Alert View 尺寸为 400x200，屏幕 540x960
- **THEN** frame 设置为 {70, 380, 400, 200}（水平居中、垂直居中）

#### Scenario: Toast 顶部定位
- **WHEN** Toast View 尺寸为 300x60，StatusBar 高度 20px，margin 16px
- **THEN** frame 设置为 {120, 36, 300, 60}（水平居中、顶部偏移 36px）
