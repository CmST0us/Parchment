## 1. ViewController 基类扩展

- [x] 1.1 在 `ViewController.h` 中添加 `virtual bool prefersStatusBarHidden() const { return false; }` 虚方法

## 2. NavigationController 回调机制

- [x] 2.1 在 `NavigationController.h` 中添加 `std::function<void(ViewController*)> onVCChange_` 成员和 `setOnViewControllerChange()` setter
- [x] 2.2 在 `NavigationController.cpp` 的 `push()` 中，`getView()` 之后、`viewWillAppear()` 之前触发回调
- [x] 2.3 在 `NavigationController.cpp` 的 `pop()` 中，弹出栈顶后、新栈顶 `viewWillAppear()` 之前触发回调
- [x] 2.4 在 `NavigationController.cpp` 的 `replace()` 中，新 VC `getView()` 之后、`viewWillAppear()` 之前触发回调

## 3. Application Window 树构建

- [x] 3.1 在 `Application.h` 中添加 `windowRoot_`, `statusBar_`, `contentArea_` 成员指针，以及 `mountViewController()` 私有方法声明
- [x] 3.2 在 `Application.cpp` 的 `init()` 末尾创建 Window View 树：windowRoot_(540×960 Column) → statusBar_(flexBasis=20) + contentArea_(flexGrow=1)
- [x] 3.3 在 `init()` 中创建 StatusBarView，设置字体，调用 `updateTime()`
- [x] 3.4 在 `init()` 中设置 NavigationController 的 `onViewControllerChange` 回调，指向 `mountViewController()`
- [x] 3.5 实现 `mountViewController(ViewController* vc)`：清除 contentArea_ 子 View → 挂载新 VC 的 root view (flexGrow=1) → 根据 `prefersStatusBarHidden()` 设置状态栏 hidden → 触发布局

## 4. Application 渲染与事件分发适配

- [x] 4.1 修改 `Application::run()` 的 renderCycle 调用：从 `vc->getView()` 改为 `windowRoot_.get()`
- [x] 4.2 在 `run()` 的循环中添加时间更新检查：记录上次分钟数，变化时调用 `statusBar_->updateTime()`
- [x] 4.3 修改 `dispatchTouch()`：hitTest 从 `windowRoot_` 开始而非当前 VC 的 root view

## 5. 现有 ViewController 适配

- [x] 5.1 `BootViewController`：覆写 `prefersStatusBarHidden()` 返回 true；root view 从 `setFrame({0,0,540,960})` 改为 `flexGrow_=1`，不设 frame
- [x] 5.2 `LibraryViewController`：删除 StatusBarView 创建代码（第 58-63 行）；root view 从 `setFrame({0,0,540,960})` 改为 `flexGrow_=1`，不设 frame
- [x] 5.3 `ReaderViewController`：root view 从 `setFrame({0,0,540,960})` 改为 `flexGrow_=1`，不设 frame；调整 `contentAreaHeight_` 计算（移除硬编码的屏幕高度）

## 6. 验证

- [x] 6.1 构建验证：`idf.py build` 编译通过
- [ ] 6.2 功能验证：Boot 页面无状态栏 → Library 页面有状态栏 → 进入 Reader 状态栏保持 → 退出 Reader 状态栏保持
