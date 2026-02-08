## ADDED Requirements

### Requirement: 绘制 1-bit 位图
系统 SHALL 提供 `ui_canvas_draw_bitmap_1bit(uint8_t *fb, int x, int y, int w, int h, const uint8_t *bitmap, uint8_t color)` 函数，将 1-bit packed bitmap 绘制到 4bpp framebuffer。bitmap 中 bit=1 的像素 SHALL 写入指定前景色，bit=0 的像素 SHALL 跳过（保持背景色不变，实现透明效果）。

#### Scenario: 渲染黑色 glyph
- **WHEN** 调用 `ui_canvas_draw_bitmap_1bit(fb, 100, 200, 24, 24, glyph_data, 0x00)` 且 glyph_data 中部分 bit 为 1
- **THEN** bit=1 的位置 SHALL 被写入 0x00 (BLACK)
- **AND** bit=0 的位置 SHALL 保持 framebuffer 原有值不变

#### Scenario: 渲染指定灰度
- **WHEN** 使用 color=0x80 (MEDIUM) 调用
- **THEN** bit=1 的像素 SHALL 被写入 0x80

#### Scenario: bitmap 行对齐
- **WHEN** bitmap 宽度不是 8 的倍数（如 w=13）
- **THEN** 每行 SHALL 使用 ceil(13/8)=2 字节，高位在前（MSB first），尾部未使用的 bit SHALL 被忽略

#### Scenario: 越界裁剪
- **WHEN** bitmap 部分超出屏幕边界
- **THEN** SHALL 仅绘制屏幕范围内的部分，不发生越界写入
