## Why

当前 `ViewController` 将 view 的创建和后续配置混合在一个 `viewDidLoad()` 纯虚函数中，与 iOS `UIViewController` 的经典时序不一致。iOS 将 view **创建** (`loadView`) 与 **配置** (`viewDidLoad`) 分离，且 `self.view` 在加载后始终可访问。当前实现存在两个核心问题：

1. **职责混乱**：`viewDidLoad()` 既负责创建 root view，又负责添加子 view、绑定数据，职责不清晰。
2. **`view_` 在挂载后变为 nullptr**：`takeView()` 后 VC 无法通过 `view_` 访问根 view，子类必须缓存 raw pointer 或通过 `parent()` 链回溯，易出错且不直观。

## What Changes

- **新增 `loadView()` 虚函数**：专职创建 root view。默认实现创建空白 `View`。子类可 override 以创建自定义 view 层次。
- **`viewDidLoad()` 职责变更** **BREAKING**：不再负责创建 root view，仅用于 view 加载后的额外配置（绑定数据、设置回调等）。变为带默认空实现的虚函数（不再是纯虚）。
- **新增 `view()` 访问器**：返回 root view 的 raw pointer，在 view 加载后始终有效（无论 `view_` 是否被 take）。替代直接访问 `view_` 成员。
- **新增 `isViewLoaded()` 查询方法**：返回 view 是否已加载。
- **迁移现有 VC**：`BootViewController`、`LibraryViewController`、`ReaderViewController` 的 `viewDidLoad()` 拆分为 `loadView()` + `viewDidLoad()`。

## Capabilities

### New Capabilities

_(无新增独立能力，所有变更均为对现有能力的修改)_

### Modified Capabilities

- `ink-view-controller`: view 创建时序从单一 `viewDidLoad()` 拆分为 `loadView()` + `viewDidLoad()`；新增 `view()` 访问器和 `isViewLoaded()`；`viewDidLoad()` 从纯虚变为虚函数。
- `boot-viewcontroller`: 适配新的 `loadView()` / `viewDidLoad()` 时序。
- `library-viewcontroller`: 适配新的 `loadView()` / `viewDidLoad()` 时序。
- `reader-viewcontroller`: 适配新的 `loadView()` / `viewDidLoad()` 时序。

## Impact

- **ink_ui/core/ViewController.h/.cpp**：接口变更（新增 `loadView()`、`view()`、`isViewLoaded()`，`viewDidLoad()` 签名变更）
- **main/controllers/ 下所有 VC**：需拆分 `viewDidLoad()` 实现
- **ink_ui/core/Application.cpp**：`mountViewController` 中 `getView()` + `takeView()` 流程可能需微调
- **无外部 API 变更**：仅影响框架内部 C++ 接口
- **无运行时行为变更**：重构不改变最终渲染结果和页面导航行为
