## Context

ui_core 框架提供了完整的绘图原语（`ui_canvas`）和图标系统（`ui_icon`），但缺少可复用的 UI 组件层。Wireframe spec 中 8 个页面大量使用相同的 header、按钮、进度条、列表等模式。需要抽象一层轻量级 widget，避免每个页面重复实现。

现有 API 约定：函数前缀 `ui_xxx_`，fb 第一参数，坐标用 `int`，颜色用 `uint8_t`，静态分配，NULL 静默返回。

## Goals / Non-Goals

**Goals:**
- 提供 5 种通用 widget（header、button、progress、list、separator）
- 每种 widget 包含绘制函数和命中检测函数
- 遵循现有 API 约定，无堆分配
- widget 无内部状态，由调用方（页面）持有和管理状态

**Non-Goals:**
- 不实现 slider（滑块）组件 — 仅在 Reading Settings 中使用，可在该 change 中实现
- 不实现焦点管理或键盘导航
- 不实现动画或过渡效果
- 不实现页面级逻辑（属于各页面 change）

## Decisions

### 决策 1: 纯函数式 API，无内部状态

**选择:** Widget 以描述性结构体 + 纯函数形式提供，不持有内部状态。

```c
// 调用方持有状态
ui_header_t header = { .title = "Parchment", .icon_left = &UI_ICON_MENU, ... };
ui_widget_draw_header(fb, &header);
int hit = ui_widget_header_hit_test(&header, touch_x, touch_y);
```

**备选:** 面向对象风格（widget 内部维护状态）→ 增加复杂度，嵌入式不适合。

**理由:** 与 `ui_page_t` 回调模式一致。页面 `on_event` 处理触摸 → 更新自己的状态 → 标记 dirty → `on_render` 用 widget 函数绘制。简单、确定性、零堆分配。

### 决策 2: Header 固定在屏幕顶部

**选择:** `ui_widget_draw_header` 不接受 y 参数，始终绘制在 (0, 0)，尺寸 540×48。

**理由:** Wireframe spec 中所有页面的 header 均在 (0, 0)。固定位置简化 API，避免错误。header 的图标触摸区为 48×48（左侧 0-47，右侧 492-539）。

### 决策 3: List 使用回调绘制每项

**选择:** `ui_list_t` 包含 `draw_item` 回调，由调用方决定每项的绘制方式。

```c
typedef void (*ui_list_draw_item_fn)(uint8_t *fb, int index, int x, int y, int w, int h);
```

**备选:** 预定义列表项模板 → 不够灵活，Library 和 TOC 的列表项布局完全不同。

**理由:** Library 列表项包含封面缩略图 + 书名 + 作者 + 进度条（96px 高），TOC 列表项只有章节名 + 页码（40-44px 高），设置列表项是标签 + 值（56px 高）。统一模板无法覆盖。

### 决策 4: Button 支持文字 + 图标组合

**选择:** `ui_button_t` 同时包含 `label`（文字）和 `icon`（图标指针），均可为 NULL。

- 仅文字: 文字居中
- 仅图标: 图标居中（48×48 触摸区）
- 图标 + 文字: 图标在上，文字在下（Reading Toolbar 底部按钮样式）

**理由:** Wireframe spec 中按钮有三种形态，统一结构体覆盖所有场景。

### 决策 5: Progress 组件兼顾显示和交互

**选择:** `ui_progress_t` 同时支持：
- 纯显示模式: Library 中 200×8 的小进度条
- 可交互模式: Reading Toolbar 中 480×36 的可触摸跳转进度条

通过 `interactive` 标志区分。交互模式触摸返回新的百分比值。

## Risks / Trade-offs

- **[回调灵活性 vs 类型安全]** → `draw_item` 回调使用 `void*` 无法做类型检查。通过清晰的文档和示例代码缓解。
- **[header 固定位置]** → 如果未来需要非标准位置的 header 会不适用。当前所有页面均符合。
- **[无 slider 组件]** → Reading Settings 需要 slider，推迟到该 change。仅 1 个页面使用，不值得在此抽象。
