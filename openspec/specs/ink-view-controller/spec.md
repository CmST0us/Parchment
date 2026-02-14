## ADDED Requirements

### Requirement: ViewController 基类
`ink::ViewController` SHALL 作为所有页面控制器的抽象基类，管理一个 View 树的创建和生命周期。子类 MUST override `viewDidLoad()` 以创建 View 树。

#### Scenario: 子类实现
- **WHEN** 定义 `class BootVC : public ViewController` 并 override viewDidLoad()
- **THEN** 编译成功，BootVC 可被 NavigationController 管理

### Requirement: View 懒加载
`getView()` SHALL 在首次调用时触发 `viewDidLoad()`（viewLoaded_==false 时），之后直接返回 `view_` 引用。viewDidLoad() SHALL 恰好被调用一次。

#### Scenario: 首次 getView 触发加载
- **WHEN** 首次调用 vc.getView()
- **THEN** viewDidLoad() 被调用，view_ 已创建，返回 view_ 引用

#### Scenario: 再次 getView 不重复加载
- **WHEN** 第二次调用 vc.getView()
- **THEN** viewDidLoad() 不再被调用，直接返回已有 view_

### Requirement: 生命周期回调
ViewController SHALL 提供以下虚函数生命周期回调（子类可选 override）：

- `viewDidLoad()`: 纯虚函数，创建 View 树，只调用一次
- `viewWillAppear()`: 即将变为可见（push 或从上层返回）
- `viewDidAppear()`: 已变为可见
- `viewWillDisappear()`: 即将变为不可见（被新页面覆盖或被 pop）
- `viewDidDisappear()`: 已变为不可见

#### Scenario: push 时的生命周期
- **WHEN** NavigationController push 一个新 VC
- **THEN** 依次调用 viewDidLoad()（如首次）→ viewWillAppear() → viewDidAppear()

#### Scenario: pop 返回时的生命周期
- **WHEN** 上层 VC 被 pop，本 VC 重新可见
- **THEN** 依次调用 viewWillAppear() → viewDidAppear()（viewDidLoad 不再调用）

### Requirement: handleEvent 事件处理
ViewController SHALL 提供 `handleEvent(const Event& e)` 虚函数，用于处理非触摸事件（TimerEvent、CustomEvent 等）。触摸事件通过 View 树的 hitTest + onTouchEvent 分发，不经过此方法。

#### Scenario: 定时器事件
- **WHEN** TimerEvent 到达，当前 VC override 了 handleEvent
- **THEN** handleEvent 被调用并接收该事件

### Requirement: title 属性
ViewController SHALL 提供 `title_` 字符串成员，用于 NavigationController 或 HeaderView 显示页面标题。

#### Scenario: 设置标题
- **WHEN** vc.title_ = "书库"
- **THEN** HeaderView 可读取该标题进行显示
