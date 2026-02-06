## 1. 组件骨架

- [x] 1.1 创建 `components/ui_core/` 目录结构和 CMakeLists.txt，声明对 epd_driver 和 gt911 的依赖
- [x] 1.2 创建公共头文件 `include/ui_core.h` 作为统一入口，包含所有子模块头文件

## 2. 事件系统

- [x] 2.1 实现 `ui_event.h`：定义 `ui_event_type_t` 枚举和 `ui_event_t` 结构体
- [x] 2.2 实现 `ui_event.c`：`ui_event_init()` 创建 FreeRTOS 队列，`ui_event_send()` 和 `ui_event_receive()` 函数

## 3. 页面栈

- [x] 3.1 实现 `ui_page.h`：定义 `ui_page_t` 结构体（name, on_enter, on_exit, on_event, on_render 回调）
- [x] 3.2 实现 `ui_page.c`：`ui_page_stack_init()`、`ui_page_push()`、`ui_page_pop()`、`ui_page_replace()`、`ui_page_current()`，最大深度 8 层

## 4. Canvas 绘图

- [x] 4.1 实现 `ui_canvas.h`：声明竖屏坐标系下的绘图 API
- [x] 4.2 实现 `ui_canvas.c`：坐标旋转映射（逻辑竖屏→物理横屏 framebuffer），`ui_canvas_draw_pixel()`、`ui_canvas_fill()`、`ui_canvas_fill_rect()`、`ui_canvas_draw_rect()`、`ui_canvas_draw_hline()`、`ui_canvas_draw_vline()`、`ui_canvas_draw_bitmap()`，包含越界裁剪

## 5. 触摸手势识别

- [x] 5.1 实现 `ui_touch.h`：声明 `ui_touch_start()` 和 `ui_touch_stop()`
- [x] 5.2 实现 `ui_touch.c`：INT 引脚 GPIO 48 下降沿中断配置，ISR 释放信号量唤醒触摸任务；触摸任务空闲时等待信号量，按下期间 20ms 轮询追踪；物理坐标到竖屏逻辑坐标的旋转映射
- [x] 5.3 实现手势状态机：点击（< 500ms, < 20px）、滑动（> 40px，判断方向）、长按（> 800ms, < 20px，立即触发）

## 6. 渲染管线

- [x] 6.1 实现 `ui_render.h`：声明脏区域管理和刷新 API
- [x] 6.2 实现 `ui_render.c`：`ui_render_mark_dirty()`（bounding box 合并）、`ui_render_mark_full_dirty()`、`ui_render_is_dirty()`、`ui_render_flush()`（面积 > 60% 全刷，否则局部刷新）

## 7. 主循环集成

- [x] 7.1 实现 `ui_core.c`：`ui_core_init()` 初始化事件系统+页面栈+渲染状态，`ui_core_run()` 启动 UI 主循环任务（事件等待→分发→渲染）
- [x] 7.2 更新 `main/main.c`：调用 `ui_core_init()`、`ui_touch_start()`、`ui_core_run()`，创建一个简单的测试页面验证整体流程

## 8. 编译验证

- [x] 8.1 ESP-IDF 5.5.1 编译通过，无 warning
