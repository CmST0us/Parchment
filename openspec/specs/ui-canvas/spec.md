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
