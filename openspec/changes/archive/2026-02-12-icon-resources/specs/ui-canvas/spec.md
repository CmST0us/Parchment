## ADDED Requirements

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
