## ADDED Requirements

### Requirement: 竖屏坐标系
Canvas API 的所有坐标参数 SHALL 使用竖屏逻辑坐标系：原点 (0,0) 在左上角，x 轴向右 (0~539)，y 轴向下 (0~959)。Canvas 内部 SHALL 负责将逻辑坐标映射到物理 framebuffer 位置。

#### Scenario: 左上角像素
- **WHEN** 调用 `ui_canvas_draw_pixel(fb, 0, 0, 0x00)`
- **THEN** framebuffer 中对应竖屏左上角的物理位置 SHALL 被设置为黑色

#### Scenario: 右下角像素
- **WHEN** 调用 `ui_canvas_draw_pixel(fb, 539, 959, 0xF0)`
- **THEN** framebuffer 中对应竖屏右下角的物理位置 SHALL 被设置为白色

### Requirement: 画像素
系统 SHALL 提供 `ui_canvas_draw_pixel(uint8_t *fb, int x, int y, uint8_t gray)` 函数，在 framebuffer 上绘制单个像素。灰度值高 4 位有效。

#### Scenario: 绘制像素
- **WHEN** 调用 `ui_canvas_draw_pixel(fb, 100, 200, 0x80)`
- **THEN** framebuffer 中 (100, 200) 逻辑位置 SHALL 被设置为对应灰度值

#### Scenario: 坐标越界
- **WHEN** 调用时 x 或 y 超出屏幕范围
- **THEN** SHALL 静默忽略，不写入 framebuffer

### Requirement: 填充矩形
系统 SHALL 提供 `ui_canvas_fill_rect(uint8_t *fb, int x, int y, int w, int h, uint8_t gray)` 函数，填充矩形区域。

#### Scenario: 正常填充
- **WHEN** 调用 `ui_canvas_fill_rect(fb, 10, 20, 100, 50, 0x00)`
- **THEN** 从 (10,20) 到 (109,69) 的所有像素 SHALL 被设为黑色

#### Scenario: 部分越界裁剪
- **WHEN** 矩形区域部分超出屏幕
- **THEN** SHALL 仅绘制屏幕范围内的部分，不发生越界写入

### Requirement: 全屏填充
系统 SHALL 提供 `ui_canvas_fill(uint8_t *fb, uint8_t gray)` 函数，将整个 framebuffer 填充为指定灰度。

#### Scenario: 填充白色
- **WHEN** 调用 `ui_canvas_fill(fb, 0xF0)`
- **THEN** framebuffer 所有像素 SHALL 被设为白色

### Requirement: 绘制矩形边框
系统 SHALL 提供 `ui_canvas_draw_rect(uint8_t *fb, int x, int y, int w, int h, uint8_t gray, int thickness)` 函数，绘制矩形边框（非填充）。

#### Scenario: 1像素边框
- **WHEN** 调用 `ui_canvas_draw_rect(fb, 0, 0, 540, 960, 0x00, 1)`
- **THEN** SHALL 绘制一个黑色 1 像素宽的矩形边框

### Requirement: 绘制水平线
系统 SHALL 提供 `ui_canvas_draw_hline(uint8_t *fb, int x, int y, int w, uint8_t gray)` 函数。

#### Scenario: 绘制分隔线
- **WHEN** 调用 `ui_canvas_draw_hline(fb, 0, 100, 540, 0x80)`
- **THEN** 从 (0,100) 到 (539,100) 的像素 SHALL 被设为指定灰度

### Requirement: 绘制垂直线
系统 SHALL 提供 `ui_canvas_draw_vline(uint8_t *fb, int x, int y, int h, uint8_t gray)` 函数。

#### Scenario: 绘制垂直分隔线
- **WHEN** 调用 `ui_canvas_draw_vline(fb, 270, 0, 960, 0x80)`
- **THEN** 从 (270,0) 到 (270,959) 的像素 SHALL 被设为指定灰度

### Requirement: 绘制位图
系统 SHALL 提供 `ui_canvas_draw_bitmap(uint8_t *fb, int x, int y, int w, int h, const uint8_t *bitmap)` 函数，将 4bpp 灰度位图绘制到 framebuffer。bitmap 数据格式 SHALL 与 framebuffer 格式一致（每字节 2 像素，高 4 位为左像素）。

#### Scenario: 绘制图标
- **WHEN** 传入一个 32×32 的位图数据
- **THEN** framebuffer 中 (x,y) 到 (x+31,y+31) 区域 SHALL 反映位图内容

### Requirement: 绘制单行文字
系统 SHALL 在 `ui_canvas.h` 中声明 `ui_canvas_draw_text(uint8_t *fb, int x, int y, const EpdFont *font, const char *text, uint8_t fg_color)` 函数。该函数在逻辑坐标系下绘制一行文字，y 为基线位置。

#### Scenario: API 可用
- **WHEN** 包含 `ui_canvas.h`
- **THEN** SHALL 能够调用 `ui_canvas_draw_text` 函数

### Requirement: 文字宽度度量
系统 SHALL 在 `ui_canvas.h` 中声明 `ui_canvas_measure_text(const EpdFont *font, const char *text)` 函数，返回文字渲染后的像素宽度。

#### Scenario: API 可用
- **WHEN** 包含 `ui_canvas.h`
- **THEN** SHALL 能够调用 `ui_canvas_measure_text` 函数

### Requirement: 着色位图绘制
系统 SHALL 提供 `ui_canvas_draw_bitmap_fg(uint8_t *fb, int x, int y, int w, int h, const uint8_t *bitmap, uint8_t fg_color)` 函数。

该函数将 4bpp 位图数据作为 alpha 蒙版绘制到 framebuffer：
- alpha = 0xF0 (最大) 时，像素 SHALL 设为 fg_color
- alpha = 0x00 (最小) 时，像素 SHALL 保留 framebuffer 原值（透明）
- 中间 alpha 值 SHALL 在 fg_color 与 framebuffer 原值之间线性插值

参数顺序 SHALL 与现有 `draw_bitmap` 一致，追加 `fg_color` 参数。

#### Scenario: 全不透明像素
- **WHEN** bitmap 中某像素 alpha 为 0xF0，fg_color 为 0x00
- **THEN** framebuffer 中对应像素 SHALL 被设为 0x00 (黑色)

#### Scenario: 全透明像素
- **WHEN** bitmap 中某像素 alpha 为 0x00
- **THEN** framebuffer 中对应像素 SHALL 保持调用前的值不变

#### Scenario: 半透明插值
- **WHEN** bitmap 中某像素 alpha 为 0x80，fg_color 为 0x00，framebuffer 原值为 0xF0
- **THEN** 该像素 SHALL 被设为约 0x70~0x80 之间的灰度值（线性插值结果）

#### Scenario: 边界裁剪
- **WHEN** 位图区域部分超出屏幕范围
- **THEN** SHALL 仅绘制屏幕范围内的像素，不发生越界写入

#### Scenario: NULL 参数保护
- **WHEN** fb 或 bitmap 为 NULL
- **THEN** SHALL 静默返回，不发生崩溃
