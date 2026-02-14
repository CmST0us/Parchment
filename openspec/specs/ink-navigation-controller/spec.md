## ADDED Requirements

### Requirement: NavigationController 页面栈
`ink::NavigationController` SHALL 管理一个 `std::vector<std::unique_ptr<ViewController>>` 页面栈，最大深度为 `MAX_DEPTH`（8）。

#### Scenario: 初始状态
- **WHEN** 创建 NavigationController
- **THEN** 栈为空，current() 返回 nullptr，depth() 返回 0

### Requirement: push 入栈
`push(std::unique_ptr<ViewController> vc)` SHALL 将新 ViewController 推入栈顶，并驱动生命周期回调：
1. 若栈非空，对当前 VC 调用 viewWillDisappear() → viewDidDisappear()
2. 推入新 VC
3. 调用新 VC 的 getView()（触发 viewDidLoad 如首次）
4. 调用新 VC 的 viewWillAppear() → viewDidAppear()

栈中被覆盖的 VC 的 View 树 SHALL 保持存活，状态不丢失。

#### Scenario: 从空栈 push
- **WHEN** 栈为空，push(vcA)
- **THEN** vcA.viewDidLoad() → vcA.viewWillAppear() → vcA.viewDidAppear()，current() 返回 vcA

#### Scenario: push 覆盖现有页面
- **WHEN** 栈有 vcA，push(vcB)
- **THEN** vcA.viewWillDisappear() → vcA.viewDidDisappear() → vcB 生命周期，vcA 的 View 树仍存活

#### Scenario: 栈满时 push
- **WHEN** 栈已有 8 个 VC，再 push
- **THEN** SHALL 不执行入栈，记录错误日志

### Requirement: pop 出栈
`pop()` SHALL 弹出栈顶 ViewController：
1. 对栈顶 VC 调用 viewWillDisappear() → viewDidDisappear()
2. 弹出并销毁栈顶 VC（unique_ptr 释放）
3. 若栈非空，对新栈顶 VC 调用 viewWillAppear() → viewDidAppear()

#### Scenario: pop 返回上一页
- **WHEN** 栈有 vcA 和 vcB（vcB 在顶），pop()
- **THEN** vcB 被销毁，vcA.viewWillAppear() → vcA.viewDidAppear()，current() 返回 vcA

#### Scenario: 只剩一个时 pop
- **WHEN** 栈只有 1 个 VC，pop()
- **THEN** SHALL 不执行出栈，记录警告日志

### Requirement: replace 替换
`replace(std::unique_ptr<ViewController> vc)` SHALL 替换栈顶 ViewController：
1. 对栈顶 VC 调用 viewWillDisappear() → viewDidDisappear()
2. 销毁栈顶 VC，替换为新 VC
3. 调用新 VC 的 getView() → viewWillAppear() → viewDidAppear()

#### Scenario: 替换栈顶
- **WHEN** 栈顶为 vcA，replace(vcB)
- **THEN** vcA 被销毁，vcB 成为新栈顶

### Requirement: current 查询
`current()` SHALL 返回栈顶 ViewController 的 raw pointer。栈为空时返回 nullptr。

#### Scenario: 栈非空
- **WHEN** 栈有至少一个 VC
- **THEN** current() 返回最后 push/replace 的 VC 指针

### Requirement: depth 查询
`depth()` SHALL 返回当前栈中 ViewController 的数量。

#### Scenario: push 后 depth 增加
- **WHEN** 栈有 1 个 VC，再 push 1 个
- **THEN** depth() 返回 2
