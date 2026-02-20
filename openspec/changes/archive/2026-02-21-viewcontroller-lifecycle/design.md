## Context

当前 `ink::ViewController` 的 view 创建使用单一 `viewDidLoad()` 纯虚函数承担「创建 view 树」和「配置/加载数据」两个职责。这与 iOS `UIViewController` 的经典模式不同——iOS 将 view 创建 (`loadView`) 和创建后配置 (`viewDidLoad`) 分为两步。

现有实现的问题：
1. 子类在 `viewDidLoad()` 中混合创建和配置逻辑，无法复用默认 view 创建
2. `view_` 在 `takeView()` 后变为 nullptr，子类回调中访问根 view 需要 workaround（缓存 raw pointer、parent() 链回溯）
3. 缺少 `isViewLoaded()` 查询方法，外部无法判断 view 是否已加载

现有调用链：`getView()` → `viewDidLoad()` (首次) → `viewLoaded_ = true` → 缓存 `viewRawPtr_`

## Goals / Non-Goals

**Goals:**
- 引入 `loadView()` / `viewDidLoad()` 两步时序，与 iOS UIViewController 语义对齐
- 提供 `view()` 公开访问器，加载后始终返回有效指针
- 提供 `isViewLoaded()` 查询方法
- 迁移三个现有 VC 到新时序

**Non-Goals:**
- 不引入 `viewWillLayoutSubviews` / `viewDidLayoutSubviews`（FlexBox 布局自动管理）
- 不改变 NavigationController 的 push/pop/replace 流程
- 不改变 view 所有权转移机制（takeView/returnView）
- 不引入 NIB/Storyboard 等声明式 view 加载

## Decisions

### Decision 1: loadView() 默认实现创建空白 View

**选择**: `loadView()` 提供默认实现，创建一个空白 `View` 并赋值给 `view_`。

**理由**: 与 iOS 一致——`UIViewController.loadView()` 默认创建空白 `UIView`。这让简单的 VC 无需 override `loadView()`，只在 `viewDidLoad()` 中添加子 view 即可。

**替代方案**: `loadView()` 为纯虚函数——强制所有子类实现，增加样板代码，且与 iOS 语义不同。

### Decision 2: viewDidLoad() 从纯虚变为虚函数（空默认实现）

**选择**: `viewDidLoad()` 变为 `virtual void viewDidLoad() {}`。

**理由**: 与 iOS 一致——`viewDidLoad` 在 iOS 中不是抽象方法，子类可选择不 override。这也允许叶子 VC 把所有逻辑放在 `loadView()` 中。

### Decision 3: 新增 view() 公开访问器

**选择**: 新增 `View* view()` 方法，触发懒加载并返回 `viewRawPtr_`。

**理由**:
- 与 iOS 的 `self.view` 语义对齐（访问即触发加载）
- 加载后始终返回有效指针，无论 `view_` 是否已被 take
- `getView()` 保留作为等价别名，保持向后兼容

### Decision 4: getView() 内部时序调整

**选择**: 调整 `getView()` 内部执行顺序为：

```
loadView()           → 子类创建 view_
viewRawPtr_ = view_  → 缓存 raw pointer
viewLoaded_ = true   → 标记已加载
viewDidLoad()        → 子类做后续配置
```

**理由**:
- `viewDidLoad()` 执行时 `isViewLoaded()` 已返回 true，`view()` 已可用
- 与 iOS 一致：`viewDidLoad` 调用时 `self.view` 和 `isViewLoaded` 均已有效

### Decision 5: view_ 保持 protected 可访问性

**选择**: `view_` 保持 `protected`，子类在 `loadView()` 中直接赋值。

**理由**:
- `loadView()` 中子类必须创建并赋值 `view_`，与 iOS 的 `self.view = ...` 等价
- 在 `viewDidLoad()` 和其他回调中，推荐使用 `view()` 而非 `view_`（因 `view_` 在挂载后为 nullptr）

## Risks / Trade-offs

**[Breaking Change] viewDidLoad() 不再是纯虚函数** → 所有现有子类必须同步迁移（将 view 创建逻辑从 `viewDidLoad()` 移至 `loadView()`）。风险低——项目只有 3 个 VC，且在同一次变更中完成。

**[语义变化] viewDidLoad() 调用时 view 已存在** → 现有子类在 `viewDidLoad()` 开头创建 `view_` 的代码，在新时序下如果不迁移会导致覆盖 `loadView()` 创建的默认 view。必须确保所有子类同步迁移。

**[命名相似] view_ 与 view() 并存** → 可能造成混淆。通过注释和约定明确：`loadView()` 中用 `view_` 赋值，其他场景用 `view()` 访问。
