## 1. FlexDirection::None 支持

- [x] 1.1 在 `FlexLayout.h` 的 `FlexDirection` 枚举中新增 `None` 值
- [x] 1.2 修改 `FlexLayout.cpp` 的 `flexLayout()` 函数：当 direction == None 时，跳过子节点定位，仅递归调用可见子 View 的 `onLayout()`

## 2. Screen View 树重构

- [x] 2.1 在 `Application.h` 中新增 `screenRoot_` 和 `overlayRoot_` 成员（`std::unique_ptr<View>` 和 `View*`）
- [x] 2.2 重构 `Application::buildWindowTree()`：创建 screenRoot_（FlexDirection::None, 540x960），将 windowRoot_ 和 overlayRoot_ 作为其子节点。overlayRoot_ 初始 hidden=true，bgColor=Clear
- [x] 2.3 修改 `Application::run()` 中的 renderCycle 入口：从 `windowRoot_.get()` 改为 `screenRoot_.get()`

## 3. 事件分发改造

- [x] 3.1 修改 `Application::dispatchTouch()`：hitTest 从 screenRoot_ 开始；当 isBlocking() 为 true 且目标不在 overlayRoot_ 子树中时吞掉触摸
- [x] 3.2 新增辅助函数判断 View 是否为 overlayRoot_ 的子孙节点
- [x] 3.3 修改 `Application::dispatchEvent()` 中 Swipe 分支：isBlocking() 时吞掉
- [x] 3.4 修改 `Application::dispatchEvent()` 中 Timer 分支：先交给 ModalPresenter::handleTimer()，消费则不传递给 VC

## 4. ShadowCardView 组件

- [x] 4.1 创建 `ShadowCardView.h`：声明类、阴影参数常量（shadowSpread, shadowOffsetX/Y）、onDraw/onLayout override
- [x] 4.2 创建 `ShadowCardView.cpp`：实现阴影灰度渐变绘制算法（白色填充 → 渐变阴影 → 白色卡片矩形）
- [x] 4.3 实现 ShadowCardView::onLayout()：计算卡片内部区域，对子 View 执行 FlexBox 布局
- [x] 4.4 实现 ShadowCardView::intrinsicSize()：内容尺寸 + 2 * shadowSpread

## 5. ModalPresenter 核心

- [x] 5.1 创建 `ModalPresenter.h`：声明 ModalChannel 枚举、ModalPriority 枚举、ModalPresenter 类和公共 API（showToast, showAlert, showActionSheet, showLoading, dismiss, isBlocking, handleTimer）
- [x] 5.2 创建 `ModalPresenter.cpp`：实现构造函数，接收 overlayRoot_、screenRoot_、RenderEngine&、Application& 依赖
- [x] 5.3 实现通道状态管理：每通道维护 active View 指针和 `std::deque<ModalRequest>` 队列
- [x] 5.4 实现 showToast()：将 View 添加到 overlayRoot_，计算顶部居中 frame，设置 overlayRoot_ visible，通过 postDelayed 注册自动消失定时器
- [x] 5.5 实现 showAlert() / showActionSheet() / showLoading()：将 View 添加到 overlayRoot_，计算屏幕居中 frame，设置 overlayRoot_ visible；处理优先级打断和排队逻辑
- [x] 5.6 实现 dismiss()：记录 damageRect，移除 View，显示队列中下一个（如有），overlayRoot_ 无子 View 时设为 hidden，调用 repairDamage 修复底层
- [x] 5.7 实现 handleTimer()：检查 timerId 范围（0x7F00 起），匹配则触发 Toast dismiss 并返回 true
- [x] 5.8 实现 isBlocking()：返回 Modal 通道是否有活跃模态

## 6. Application 集成

- [x] 6.1 在 `Application.h` 中新增 `std::unique_ptr<ModalPresenter> modalPresenter_` 成员和 `modalPresenter()` 访问器
- [x] 6.2 在 `Application::init()` 中，buildWindowTree() 之后创建 ModalPresenter 实例
- [x] 6.3 在 `ink_ui/CMakeLists.txt` 中注册新源文件：ModalPresenter.cpp、ShadowCardView.cpp
