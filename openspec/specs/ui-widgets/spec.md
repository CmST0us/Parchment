## Requirements

### Requirement: 通用 UI Widget 组件层
系统 SHALL 在 `ui_widget.h` 中提供 header、button、progress、list、separator、slider、selection group、modal dialog 八种可复用组件的绘制和命中检测 API。所有 widget 采用纯函数式接口，不持有内部状态。

#### Scenario: 头文件可用
- **WHEN** 包含 `ui_widget.h`
- **THEN** SHALL 可使用所有 8 种 widget 的 struct 类型和 draw/hit_test 函数

#### Scenario: 新增 widget 函数可调用
- **WHEN** 链接 ui_core 组件
- **THEN** SHALL 可调用 `ui_widget_draw_slider()`、`ui_widget_slider_touch()`、`ui_widget_draw_sel_group()`、`ui_widget_sel_group_hit_test()`、`ui_widget_draw_dialog()`、`ui_widget_dialog_hit_test()`

### Requirement: Header 组件绘制
系统 SHALL 提供 `ui_widget_draw_header(uint8_t *fb, const ui_header_t *header)` 函数。

Header 固定绘制在 (0, 0)，尺寸 540×48，BLACK 底色。标题文字 SHALL 使用 24px 字体 WHITE 色居中显示。左侧图标（若非 NULL）SHALL 绘制在 (8, 8) 位置，WHITE 色。右侧图标（若非 NULL）SHALL 绘制在 (500, 8) 位置，WHITE 色。

`ui_header_t` 结构体 SHALL 包含：
- `title`: 标题文字 (`const char *`)
- `icon_left`: 左侧图标 (`const ui_icon_t *`，可为 NULL)
- `icon_right`: 右侧图标 (`const ui_icon_t *`，可为 NULL)

#### Scenario: 完整 header
- **WHEN** 调用 `ui_widget_draw_header(fb, &header)`，header 包含标题、左右图标
- **THEN** SHALL 绘制黑色 48px 高的标题栏，左侧图标、居中标题、右侧图标均为白色

#### Scenario: 仅标题无图标
- **WHEN** icon_left 和 icon_right 均为 NULL
- **THEN** SHALL 仅绘制标题文字，无图标

#### Scenario: NULL 保护
- **WHEN** fb 或 header 为 NULL
- **THEN** SHALL 静默返回

### Requirement: Header 命中检测
系统 SHALL 提供 `ui_widget_header_hit_test(const ui_header_t *header, int x, int y)` 函数。

返回值：
- `0`: 未命中 header 区域或命中空白区域
- `1`: 命中左侧图标区域 (x: 0-47, y: 0-47)
- `2`: 命中右侧图标区域 (x: 492-539, y: 0-47)

仅当对应图标不为 NULL 时 SHALL 返回命中。

#### Scenario: 点击左侧图标
- **WHEN** icon_left 非 NULL，触摸坐标 (20, 24)
- **THEN** SHALL 返回 1

#### Scenario: 点击右侧图标
- **WHEN** icon_right 非 NULL，触摸坐标 (510, 24)
- **THEN** SHALL 返回 2

#### Scenario: 点击标题区域
- **WHEN** 触摸坐标 (270, 24)
- **THEN** SHALL 返回 0

#### Scenario: 点击 header 外
- **WHEN** 触摸坐标 y > 47
- **THEN** SHALL 返回 0

### Requirement: Button 组件绘制
系统 SHALL 提供 `ui_widget_draw_button(uint8_t *fb, const ui_button_t *btn)` 函数。

`ui_button_t` 结构体 SHALL 包含：
- `x, y, w, h`: 按钮位置和尺寸
- `label`: 文字标签 (`const char *`，可为 NULL)
- `icon`: 图标 (`const ui_icon_t *`，可为 NULL)
- `style`: 按钮样式 (`ui_button_style_t`)

样式枚举 SHALL 包含：
- `UI_BTN_PRIMARY`: BLACK 底 WHITE 字
- `UI_BTN_SECONDARY`: WHITE 底 BLACK 字 BLACK 边框 (2px)
- `UI_BTN_ICON`: 透明底（不填充背景），仅绘制图标
- `UI_BTN_SELECTED`: LIGHT 底 BLACK 字

仅文字时文字 SHALL 水平垂直居中。仅图标时图标 SHALL 居中。图标 + 文字时图标在上、文字在下，整体垂直居中。

#### Scenario: 主按钮绘制
- **WHEN** style 为 UI_BTN_PRIMARY，label 为 "确定"
- **THEN** SHALL 绘制黑底白字按钮，文字居中

#### Scenario: 图标按钮绘制
- **WHEN** style 为 UI_BTN_ICON，icon 非 NULL，label 为 NULL
- **THEN** SHALL 仅绘制图标，无背景填充

#### Scenario: 图标加文字按钮
- **WHEN** icon 和 label 均非 NULL
- **THEN** SHALL 图标在上、文字在下排列

### Requirement: Button 命中检测
系统 SHALL 提供 `ui_widget_button_hit_test(const ui_button_t *btn, int x, int y)` 函数。

触摸坐标在 (btn->x, btn->y) 到 (btn->x+btn->w-1, btn->y+btn->h-1) 范围内 SHALL 返回 `true`，否则返回 `false`。

#### Scenario: 点击按钮内部
- **WHEN** 触摸坐标在按钮矩形范围内
- **THEN** SHALL 返回 true

#### Scenario: 点击按钮外部
- **WHEN** 触摸坐标在按钮矩形范围外
- **THEN** SHALL 返回 false

### Requirement: Progress 组件绘制
系统 SHALL 提供 `ui_widget_draw_progress(uint8_t *fb, const ui_progress_t *prog)` 函数。

`ui_progress_t` 结构体 SHALL 包含：
- `x, y, w, h`: 进度条位置和尺寸
- `value`: 进度值 (0-100)
- `show_label`: 是否在进度条右侧显示百分比文字

进度条 SHALL 由两部分组成：已完成区域 (BLACK) 和未完成区域 (LIGHT)。已完成宽度 = w * value / 100。

当 `show_label` 为 true 时，SHALL 在进度条右侧外 8px 处绘制 "{value}%" 文字，20px 字号，DARK 色。

#### Scenario: 50% 进度条
- **WHEN** value 为 50，w 为 200
- **THEN** SHALL 绘制左半 100px BLACK，右半 100px LIGHT

#### Scenario: 带标签
- **WHEN** show_label 为 true，value 为 80
- **THEN** SHALL 在进度条右侧显示 "80%"

#### Scenario: 边界值
- **WHEN** value 为 0
- **THEN** SHALL 全部显示 LIGHT (未完成色)

### Requirement: Progress 触摸映射
系统 SHALL 提供 `ui_widget_progress_touch(const ui_progress_t *prog, int x, int y)` 函数。

若触摸点在进度条 y 方向扩展触摸区内（y ± 20px），SHALL 根据 x 坐标计算并返回对应的 value (0-100)。未命中 SHALL 返回 -1。

#### Scenario: 触摸进度条中点
- **WHEN** 触摸 x 在进度条水平中点位置
- **THEN** SHALL 返回 50

#### Scenario: 触摸在进度条外
- **WHEN** 触摸 y 距离进度条超过 20px
- **THEN** SHALL 返回 -1

### Requirement: List 组件绘制
系统 SHALL 提供 `ui_widget_draw_list(uint8_t *fb, const ui_list_t *list)` 函数。

`ui_list_t` 结构体 SHALL 包含：
- `x, y, w, h`: 列表可视区域
- `item_height`: 每项高度（固定值）
- `item_count`: 总项数
- `scroll_offset`: 当前滚动偏移（像素，≥0）
- `draw_item`: 绘制回调 (`ui_list_draw_item_fn`)

回调签名: `void (*)(uint8_t *fb, int index, int x, int y, int w, int h)`

该函数 SHALL 仅对可视区域内的项调用 `draw_item` 回调。首个可见项的 index SHALL 为 `scroll_offset / item_height`。部分可见的项也 SHALL 调用回调（裁剪由回调方或 canvas 自动处理）。

#### Scenario: 9 个可见项
- **WHEN** item_height=96, h=876, scroll_offset=0, item_count=20
- **THEN** SHALL 对 index 0-9 调用 draw_item（index 9 部分可见）

#### Scenario: 滚动后
- **WHEN** scroll_offset=192 (2 项高度), item_height=96
- **THEN** SHALL 从 index 2 开始绘制

#### Scenario: 空列表
- **WHEN** item_count=0
- **THEN** SHALL 不调用 draw_item

### Requirement: List 命中检测
系统 SHALL 提供 `ui_widget_list_hit_test(const ui_list_t *list, int x, int y)` 函数。

若触摸在列表可视区域内，SHALL 返回对应的 item index。考虑 scroll_offset 计算: `index = (y - list->y + scroll_offset) / item_height`。若 index >= item_count 或触摸在可视区域外，SHALL 返回 -1。

#### Scenario: 点击第一项
- **WHEN** scroll_offset=0, 触摸 y 在列表区域第一项范围内
- **THEN** SHALL 返回 0

#### Scenario: 滚动后点击
- **WHEN** scroll_offset=192, 触摸 y 在列表顶部
- **THEN** SHALL 返回 2

#### Scenario: 点击列表外
- **WHEN** 触摸 y 在列表可视区域外
- **THEN** SHALL 返回 -1

### Requirement: List 滚动
系统 SHALL 提供 `ui_widget_list_scroll(ui_list_t *list, int delta_y)` 函数。

该函数 SHALL 更新 `list->scroll_offset`，增加 `delta_y`（正值向下滚动，负值向上）。结果 SHALL 被 clamp 到 `[0, max_scroll]`，其中 `max_scroll = max(0, item_count * item_height - h)`。

#### Scenario: 向下滚动
- **WHEN** delta_y=96, 当前 scroll_offset=0
- **THEN** scroll_offset SHALL 变为 96

#### Scenario: 滚动到底
- **WHEN** delta_y 导致 scroll_offset 超过 max_scroll
- **THEN** scroll_offset SHALL 被 clamp 到 max_scroll

#### Scenario: 向上滚动越界
- **WHEN** delta_y=-100, 当前 scroll_offset=50
- **THEN** scroll_offset SHALL 被 clamp 到 0

### Requirement: Separator 组件
系统 SHALL 提供 `ui_widget_draw_separator(uint8_t *fb, int x, int y, int w)` 函数。

该函数 SHALL 在 (x, y) 绘制一条 1px 高、w 像素宽的水平线，颜色为 MEDIUM (0x80)。

#### Scenario: 绘制分隔线
- **WHEN** 调用 `ui_widget_draw_separator(fb, 16, 179, 508)`
- **THEN** SHALL 在 y=179 绘制从 x=16 到 x=523 的 MEDIUM 色水平线
