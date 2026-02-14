## 1. FlexLayout 布局算法

- [x] 1.1 实现 `ink::flexLayout(View* container)` 自由函数：测量阶段（区分固定/弹性子 View）、分配阶段（剩余空间按 flexGrow 比例分配）、定位阶段（设置每个子 View 的 frame 并递归 onLayout）
- [x] 1.2 实现 gap 间距：相邻可见子 View 之间插入 flexStyle_.gap 像素，gap 从可分配空间中扣除
- [x] 1.3 实现 padding 内边距：从容器 bounds 扣除 padding 后在内区域排列子 View
- [x] 1.4 实现 alignItems 交叉轴对齐（Stretch/Start/Center/End）和 alignSelf 覆盖
- [x] 1.5 实现 justifyContent 主轴对齐（Start/Center/End/SpaceBetween/SpaceAround）
- [x] 1.6 实现 intrinsicSize 查询：flexBasis==-1 且 flexGrow==0 时使用 intrinsicSize() 作为基础尺寸
- [x] 1.7 修改 `View::onLayout()` 默认实现调用 `flexLayout(this)`
- [x] 1.8 在 CMakeLists.txt 中注册 `src/core/FlexLayout.cpp`

## 2. RenderEngine 渲染引擎

- [x] 2.1 创建 `RenderEngine.h` 头文件：DirtyEntry 结构体、MAX_DIRTY_REGIONS=8、GHOST_THRESHOLD=10、公共 API（renderCycle / repairDamage / forceFullClear）
- [x] 2.2 实现 Phase 1 Layout Pass：检查 rootView needsLayout，递归调用 onLayout()
- [x] 2.3 实现 Phase 2 Collect Dirty：遍历 View 树，收集 needsDisplay==true 的 View 的 screenFrame 和 refreshHint
- [x] 2.4 实现 Phase 3 Draw Dirty：创建 Canvas(fb, screenFrame)，clear + onDraw + 强制重绘子 View 子树；仅 subtreeNeedsDisplay 时递归不重绘父 View
- [x] 2.5 实现 Phase 4 Flush：调用 mergeOverlappingRegions() 合并相邻同 hint 区域，逐区域调用 EpdDriver::updateArea()
- [x] 2.6 实现 Phase 5 Ghost Management：partialCount 累加，达到 GHOST_THRESHOLD 时全屏 GC16
- [x] 2.7 实现脏区域合并策略：相邻 + 同 hint 合并；不同 hint 独立；合并面积 > 60% 退化全屏
- [x] 2.8 实现 repairDamage()：指定矩形区域裁剪重绘
- [x] 2.9 在 CMakeLists.txt 中注册 `src/core/RenderEngine.cpp`

## 3. GestureRecognizer 手势识别

- [x] 3.1 创建 `GestureRecognizer.h` 头文件：构造接受 QueueHandle_t，start()/stop() API
- [x] 3.2 实现 GT911 INT 中断配置（GPIO 48 下降沿）和信号量唤醒
- [x] 3.3 实现独立 FreeRTOS 任务：中断等待 → I2C 读取 → 20ms 轮询追踪 → 释放后回到中断等待
- [x] 3.4 实现坐标映射（GT911 直通，无需翻转）
- [x] 3.5 实现 Tap 识别：时间 < 500ms + 移动 < 20px → TouchEvent{type=Tap}
- [x] 3.6 实现 LongPress 识别：持续 > 800ms + 移动 < 20px → TouchEvent{type=LongPress}，释放后不产生 Tap
- [x] 3.7 实现 Swipe 识别：移动 > 40px → SwipeEvent{direction=主方向}
- [x] 3.8 实现 Down/Move/Up 原始事件发送
- [x] 3.9 在 CMakeLists.txt 中注册 `src/core/GestureRecognizer.cpp`

## 4. ViewController 页面生命周期

- [x] 4.1 创建 `ViewController.h` 头文件：纯虚 viewDidLoad()、虚函数 viewWillAppear/viewDidAppear/viewWillDisappear/viewDidDisappear/handleEvent、getView() 懒加载、title_ 成员
- [x] 4.2 实现 `ViewController.cpp`：getView() 懒加载逻辑（首次调用触发 viewDidLoad，设置 viewLoaded_=true）
- [x] 4.3 在 CMakeLists.txt 中注册 `src/core/ViewController.cpp`

## 5. NavigationController 页面栈

- [x] 5.1 创建 `NavigationController.h` 头文件：push/pop/replace/current/depth API、MAX_DEPTH=8
- [x] 5.2 实现 push()：旧栈顶 viewWillDisappear/viewDidDisappear → 推入新 VC → getView() → viewWillAppear/viewDidAppear
- [x] 5.3 实现 pop()：栈顶 viewWillDisappear/viewDidDisappear → 销毁 → 新栈顶 viewWillAppear/viewDidAppear；栈只剩 1 个时拒绝 pop
- [x] 5.4 实现 replace()：旧栈顶 viewWillDisappear/viewDidDisappear → 销毁替换 → 新 VC 生命周期
- [x] 5.5 在 CMakeLists.txt 中注册 `src/core/NavigationController.cpp`

## 6. Application 主循环

- [x] 6.1 创建 `Application.h` 头文件：init()/run()/postEvent()/postDelayed()/navigator()/renderer() API
- [x] 6.2 实现 init()：EpdDriver 初始化 → 创建事件队列 → 创建 RenderEngine → 创建并启动 GestureRecognizer
- [x] 6.3 实现 run() 主循环：xQueueReceive(30s 超时) → dispatchEvent → renderCycle
- [x] 6.4 实现 dispatchTouch()：hitTest → onTouchEvent 冒泡链
- [x] 6.5 实现 dispatchEvent()：TouchEvent → dispatchTouch；SwipeEvent/TimerEvent → vc.handleEvent()
- [x] 6.6 实现 postEvent() 和 postDelayed()（esp_timer 延迟投递）
- [x] 6.7 在 CMakeLists.txt 中注册 `src/core/Application.cpp`

## 7. 集成与验证

- [x] 7.1 更新 `main.cpp`：移除旧 ui_core 调用，使用 `ink::Application` + `ink::NavigationController` 启动
- [x] 7.2 创建最小验证 ViewController（显示灰色矩形 + 响应触摸 log），验证完整链路：touch → event → hitTest → onTouchEvent → setNeedsDisplay → renderCycle → EPD 刷新
- [x] 7.3 验证 FlexBox 布局：创建 Column 容器 + 多个子 View，确认自动排列正确
- [x] 7.4 验证页面栈：push 第二个 VC，触摸返回 pop，确认生命周期回调正确
- [x] 7.5 确认 `idf.py build` 通过，固件大小合理
