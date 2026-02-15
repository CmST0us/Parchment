## ADDED Requirements

### Requirement: 应用启动流程
应用 SHALL 在 `app_main()` 中按顺序执行以下初始化，然后 push BootViewController：
1. NVS 初始化（settings_store_init）
2. 板级初始化（board_init）
3. GT911 触摸初始化
4. SD 卡挂载
5. 字体子系统初始化（ui_font_init）
6. InkUI Application 初始化（app.init()）
7. navigator().push(BootViewController)
8. app.run()（永不返回）

#### Scenario: 正常启动
- **WHEN** 设备上电
- **THEN** 依次初始化各子系统，push BootVC，进入主循环，屏幕显示启动画面

### Requirement: 导航流 Boot → Library
BootViewController SHALL 在 2 秒延迟后通过 `navigator().replace()` 切换到 LibraryViewController。替换后 navigator.depth() SHALL 保持为 1。

#### Scenario: Boot 到 Library
- **WHEN** 启动画面显示 2 秒后
- **THEN** 自动切换到书库页面，Boot 页面被销毁

### Requirement: 导航流 Library → Reader
LibraryViewController 中点击书籍条目 SHALL 通过 `navigator().push()` 推入 ReaderViewController。push 后 navigator.depth() SHALL 为 2。

#### Scenario: Library 到 Reader
- **WHEN** 用户在书库点击 "三体.txt"
- **THEN** ReaderViewController 被 push，显示阅读页面，Library 的 View 树保持存活

### Requirement: 导航流 Reader → Library
ReaderViewController 中点击返回按钮 SHALL 通过 `navigator().pop()` 返回 LibraryViewController。pop 后 navigator.depth() SHALL 为 1，LibraryViewController 的 viewWillAppear 被调用。

#### Scenario: Reader 返回 Library
- **WHEN** 用户在阅读页点击返回
- **THEN** ReaderVC 被 pop 并销毁，Library 的 viewWillAppear 触发进度刷新

### Requirement: controllers 文件组织
所有 ViewController 实现文件 SHALL 位于 `main/controllers/` 目录下，每个 VC 一对 `.h/.cpp` 文件。main.cpp SHALL 仅 include BootViewController.h 并执行初始化和 push。

#### Scenario: 文件结构
- **WHEN** 查看 main/controllers/ 目录
- **THEN** 包含 BootViewController.h/.cpp、LibraryViewController.h/.cpp、ReaderViewController.h/.cpp

### Requirement: main.cpp 重构
main.cpp SHALL 移除临时 BootVC 类定义，改为 include `controllers/BootViewController.h`。初始化序列保持不变，push 改为使用正式的 BootViewController。

#### Scenario: 干净的 main.cpp
- **WHEN** 查看 main.cpp
- **THEN** 无内联 VC 类定义，仅有初始化代码和 push(BootViewController)
