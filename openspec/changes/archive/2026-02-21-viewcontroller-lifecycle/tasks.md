## 1. ViewController 基类重构

- [x] 1.1 在 ViewController.h 中新增 `virtual void loadView()` 声明，默认实现创建空白 `View`
- [x] 1.2 将 `viewDidLoad()` 从纯虚函数改为虚函数（空默认实现）
- [x] 1.3 新增 `View* view()` 公开访问器和 `bool isViewLoaded() const` 查询方法
- [x] 1.4 重构 `getView()` 内部时序：loadView() → 缓存 viewRawPtr_ → viewLoaded_=true → viewDidLoad()

## 2. BootViewController 迁移

- [x] 2.1 新增 `loadView()` override，将 view 树创建逻辑（root view、logo、标题、状态标签、进度条）从 `viewDidLoad()` 移入
- [x] 2.2 `viewDidLoad()` 仅保留 SD 卡扫描和状态更新逻辑

## 3. LibraryViewController 迁移

- [x] 3.1 新增 `loadView()` override，将 view 树创建逻辑（header、subheader、list container、page indicator）从 `viewDidLoad()` 移入
- [x] 3.2 `viewDidLoad()` 仅保留 `updatePageInfo()` 和 `buildPage()` 数据加载

## 4. ReaderViewController 迁移

- [x] 4.1 新增 `loadView()` override，将 view 树创建逻辑（header、touch view、content view、footer）从 `viewDidLoad()` 移入
- [x] 4.2 `viewDidLoad()` 仅保留文件加载（loadFile）、文本缓冲区设置和进度恢复
