## ADDED Requirements

### Requirement: ViewController 基类
`ink::ViewController` SHALL 作为所有页面控制器的抽象基类，管理一个 View 树的创建和生命周期。子类可 override `loadView()` 创建自定义 View 树，override `viewDidLoad()` 做加载后配置。

#### Scenario: 子类实现
- **WHEN** 定义 `class BootVC : public ViewController` 并 override loadView()
- **THEN** 编译成功，BootVC 可被 NavigationController 管理

### Requirement: View 懒加载
`getView()` SHALL 在首次调用时依次触发 `loadView()` 和 `viewDidLoad()`，之后直接返回 `viewRawPtr_`。执行顺序 SHALL 为：
1. 调用 `loadView()` — 子类创建 `view_`
2. 缓存 `viewRawPtr_ = view_.get()`
3. 设置 `viewLoaded_ = true`
4. 调用 `viewDidLoad()` — 子类做后续配置

`viewDidLoad()` 执行时，`isViewLoaded()` SHALL 返回 true，`view()` SHALL 返回有效指针。

#### Scenario: 首次 getView 触发 loadView + viewDidLoad
- **WHEN** 首次调用 vc.getView()
- **THEN** loadView() 被调用创建 view_，然后 viewDidLoad() 被调用，返回 view 指针

#### Scenario: 再次 getView 不重复加载
- **WHEN** 第二次调用 vc.getView()
- **THEN** loadView() 和 viewDidLoad() 均不再被调用，直接返回已缓存的 viewRawPtr_

#### Scenario: viewDidLoad 中 view() 可用
- **WHEN** viewDidLoad() 执行期间调用 view()
- **THEN** 返回 loadView() 中创建的 root view 指针，isViewLoaded() 返回 true

### Requirement: 生命周期回调
ViewController SHALL 提供以下虚函数生命周期回调：

- `loadView()`: 创建 View 树，默认创建空白 View，子类可 override
- `viewDidLoad()`: View 加载后的配置回调，默认空实现，子类可 override
- `viewWillAppear()`: 即将变为可见（push 或从上层返回）
- `viewDidAppear()`: 已变为可见
- `viewWillDisappear()`: 即将变为不可见（被新页面覆盖或被 pop）
- `viewDidDisappear()`: 已变为不可见

#### Scenario: push 时的生命周期
- **WHEN** NavigationController push 一个新 VC
- **THEN** 依次调用 loadView()（如首次）→ viewDidLoad()（如首次）→ viewWillAppear() → viewDidAppear()

#### Scenario: pop 返回时的生命周期
- **WHEN** 上层 VC 被 pop，本 VC 重新可见
- **THEN** 依次调用 viewWillAppear() → viewDidAppear()（loadView 和 viewDidLoad 不再调用）

### Requirement: loadView 虚函数
`ink::ViewController` SHALL 提供 `virtual void loadView()` 方法，用于创建 root view 并赋值给 `view_`。默认实现 SHALL 创建一个空白 `ink::View` 实例。子类可 override 以创建自定义 view 层次。

#### Scenario: 默认 loadView 创建空白 View
- **WHEN** 子类未 override `loadView()`，首次调用 `view()`
- **THEN** `loadView()` 默认实现创建空白 `View`，`view()` 返回该 View 的指针

#### Scenario: 子类 override loadView 创建自定义 View 树
- **WHEN** 子类 override `loadView()` 创建包含 TextLabel 子 view 的 View 树
- **THEN** `view()` 返回自定义 root view，子 view 已添加

### Requirement: view() 公开访问器
`ink::ViewController` SHALL 提供 `View* view()` 公开方法，触发懒加载（若 view 未加载）并返回 root view 的 raw pointer。该指针在 view 加载后 SHALL 始终有效，无论 `view_` 是否已被 `takeView()` 转移所有权。

#### Scenario: view() 触发懒加载
- **WHEN** view 未加载时调用 `view()`
- **THEN** `loadView()` 和 `viewDidLoad()` 依次执行，返回有效 View 指针

#### Scenario: view() 在 takeView 后仍有效
- **WHEN** `takeView()` 已转移 `view_` 所有权后调用 `view()`
- **THEN** 返回与加载时相同的有效 View 指针（viewRawPtr_）

### Requirement: isViewLoaded 查询方法
`ink::ViewController` SHALL 提供 `bool isViewLoaded() const` 方法。view 加载前返回 false，`loadView()` 完成后返回 true。

#### Scenario: 加载前查询
- **WHEN** VC 刚创建，未触发 view 加载
- **THEN** `isViewLoaded()` 返回 false

#### Scenario: 加载后查询
- **WHEN** `view()` 已触发懒加载
- **THEN** `isViewLoaded()` 返回 true

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
