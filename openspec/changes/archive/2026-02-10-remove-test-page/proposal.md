## Why

项目已完成 ui_core 框架的验证，交互测试页面（toggle 按钮、翻页指示器、进度条）已完成其使命。需要移除测试代码，为生产页面开发腾出干净的入口点。

## What Changes

- **移除** `main/main.c` 中全部交互测试页面代码（约 340 行），包括：
  - 测试布局常量（`BTN_*`、`DOT_*`、`BAR_*` 宏定义）
  - 测试状态变量（`s_buttons`、`s_page`、`s_level`）
  - 测试绘制函数（`draw_filled_circle`、`draw_button`、`draw_page_dots`、`draw_progress_bar` 等）
  - 测试页面回调（`test_page_on_enter`、`test_page_on_event`、`test_page_on_render`）
  - `test_page` 结构体定义
  - `app_main()` 中的 `ui_page_push(&test_page, NULL)` 调用
- **更新** 文件头注释，移除测试页面描述
- **保留** ui_core 框架作为生产框架不做任何修改
- **保留** `app_main()` 中的硬件初始化流程和 `ui_core_init()` / `ui_core_run()` 调用

## Capabilities

### New Capabilities

（无新增能力）

### Modified Capabilities

（无需求级别的变更——ui_core 各组件的 spec 保持不变，仅移除使用测试代码）

## Impact

- `main/main.c`：从 ~427 行缩减至 ~60 行，仅保留硬件初始化和 UI 框架启动
- 启动后 ui_core 事件循环运行但无页面被 push，处于空闲等待状态
- 不影响任何 component（`ui_core`、`epd_driver`、`gt911`、`sd_storage` 均不变）
