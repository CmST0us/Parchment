## 1. 板级定义与电池组件

- [x] 1.1 在 `board.h` 中添加 `BOARD_BAT_ADC` (GPIO_NUM_3) 引脚定义
- [x] 1.2 创建 `components/battery/` 组件目录结构（CMakeLists.txt, battery.h, battery.c）
- [x] 1.3 实现 `battery_init()`: ADC oneshot 初始化 + 校准方案（curve fitting fallback line fitting）
- [x] 1.4 实现 `battery_get_percent()`: 8 次采样均值 → 校准 mV → ×2 还原电压 → 查找表插值 → 0-100 百分比

## 2. StatusBarView 电池图标

- [x] 2.1 在 `StatusBarView` 中添加 `battery_percent_` 成员和 `updateBattery(int percent)` 方法
- [x] 2.2 在 `StatusBarView::onDraw` 中实现电池图标绘制（右侧，canvas drawRect + fillRect）

## 3. 集成与启动

- [x] 3.1 在 `main.cpp` 的 `app_main()` 中调用 `battery_init()`
- [x] 3.2 在 `Application::run()` 事件循环中添加电池电量定期更新逻辑（≥30 秒间隔）
