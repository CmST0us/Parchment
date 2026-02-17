## MODIFIED Requirements

### Requirement: push 入栈
`push(std::unique_ptr<ViewController> vc)` SHALL 将新 ViewController 推入栈顶，并驱动生命周期回调：
1. 若栈非空，对当前 VC 调用 viewWillDisappear() → viewDidDisappear()
2. 推入新 VC
3. 调用新 VC 的 getView()（触发 viewDidLoad 如首次）
4. **触发 onViewControllerChange 回调（如已设置）**
5. 调用新 VC 的 viewWillAppear() → viewDidAppear()

栈中被覆盖的 VC 的 View 树 SHALL 保持存活，状态不丢失。

#### Scenario: 从空栈 push
- **WHEN** 栈为空，push(vcA)
- **THEN** vcA.viewDidLoad() → onViewControllerChange 回调 → vcA.viewWillAppear() → vcA.viewDidAppear()，current() 返回 vcA

#### Scenario: push 覆盖现有页面
- **WHEN** 栈有 vcA，push(vcB)
- **THEN** vcA.viewWillDisappear() → vcA.viewDidDisappear() → vcB.getView() → onViewControllerChange 回调 → vcB 生命周期，vcA 的 View 树仍存活

#### Scenario: 栈满时 push
- **WHEN** 栈已有 8 个 VC，再 push
- **THEN** SHALL 不执行入栈，记录错误日志

### Requirement: pop 出栈
`pop()` SHALL 弹出栈顶 ViewController：
1. 对栈顶 VC 调用 viewWillDisappear() → viewDidDisappear()
2. 弹出并销毁栈顶 VC（unique_ptr 释放）
3. 若栈非空，**触发 onViewControllerChange 回调**
4. 对新栈顶 VC 调用 viewWillAppear() → viewDidAppear()

#### Scenario: pop 返回上一页
- **WHEN** 栈有 vcA 和 vcB（vcB 在顶），pop()
- **THEN** vcB 被销毁，onViewControllerChange 回调 → vcA.viewWillAppear() → vcA.viewDidAppear()，current() 返回 vcA

#### Scenario: 只剩一个时 pop
- **WHEN** 栈只有 1 个 VC，pop()
- **THEN** SHALL 不执行出栈，记录警告日志

### Requirement: replace 替换
`replace(std::unique_ptr<ViewController> vc)` SHALL 替换栈顶 ViewController：
1. 对栈顶 VC 调用 viewWillDisappear() → viewDidDisappear()
2. 销毁栈顶 VC，替换为新 VC
3. 调用新 VC 的 getView() → **触发 onViewControllerChange 回调**
4. 调用新 VC 的 viewWillAppear() → viewDidAppear()

#### Scenario: 替换栈顶
- **WHEN** 栈顶为 vcA，replace(vcB)
- **THEN** vcA 被销毁，onViewControllerChange 回调 → vcB 成为新栈顶

## ADDED Requirements

### Requirement: onViewControllerChange 回调
NavigationController SHALL 提供 `setOnViewControllerChange(std::function<void(ViewController*)> callback)` 方法。

当 push/pop/replace 导致当前 VC 变化时，SHALL 以新的当前 VC 指针调用此回调。回调在 getView() 之后、viewWillAppear() 之前触发。

#### Scenario: 设置回调
- **WHEN** 调用 `nav.setOnViewControllerChange(callback)`
- **THEN** 后续 push/pop/replace 操作均触发 callback

#### Scenario: 回调未设置
- **WHEN** 未调用 setOnViewControllerChange，执行 push
- **THEN** push 正常执行，无回调触发
