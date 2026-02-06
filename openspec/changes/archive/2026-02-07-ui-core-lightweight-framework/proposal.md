## Why

Parchment 项目已完成 HAL 层（epd_driver、gt911、sd_storage），但目前缺少 UI 框架来承载阅读器的各个页面。项目将持续迭代添加功能（阅读、书架、设置等），需要一个轻量的自建 UI 框架作为所有上层功能的基础，提供统一的页面管理、事件分发、触摸路由和 framebuffer 绘图能力。

## What Changes

- 新增 `ui_core` 组件，提供事件驱动的主循环和页面栈导航机制
- 新增统一事件系统：触摸手势（点击、滑动、长按）、定时器、自定义事件通过同一队列分发
- 新增触摸采集任务，从 GT911 读取原始触摸数据，识别手势后生成事件
- 新增 Canvas 绘图 API，在 framebuffer 上提供矩形填充、线条、位图绘制等基础原语
- 新增脏区域追踪机制，支持局部刷新以减少 E-Ink 全屏刷新等待
- 新增页面接口定义：每个页面实现 `on_enter` / `on_exit` / `on_event` / `on_render` 四个回调

## Capabilities

### New Capabilities
- `ui-event-system`: 统一事件队列、事件类型定义、事件分发机制
- `ui-page-stack`: 页面栈管理（push/pop/replace）、页面生命周期回调
- `ui-canvas`: framebuffer 绘图原语（矩形、线条、位图等）
- `ui-touch-gesture`: 触摸手势识别（点击、滑动、长按），从原始触摸数据到语义事件的转换
- `ui-render-pipeline`: 脏区域追踪、按需刷新策略（全刷 vs 局部刷）、渲染调度

### Modified Capabilities

（无已有 spec 需要修改）

## Impact

- **新增组件**: `components/ui_core/`
- **依赖关系**: ui_core 依赖 epd_driver（framebuffer 和刷屏）、gt911（触摸数据读取）
- **FreeRTOS 资源**: 新增 1 个事件队列、1 个触摸采集任务
- **内存预算**: 事件队列约 1KB，页面栈（最大 8 层）约 256B，手势识别状态约 64B
- **后续页面**: 各具体页面（boot、library、reading 等）将基于此框架实现，不在本次变更范围内
