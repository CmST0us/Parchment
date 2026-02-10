## ADDED Requirements

### Requirement: 单行文字绘制
系统 SHALL 提供 `ui_canvas_draw_text(uint8_t *fb, int x, int y, const EpdFont *font, const char *text, uint8_t fg_color)` 函数。该函数在 framebuffer 上从逻辑坐标 (x, y) 开始绘制一行文字。y 坐标为文字基线（baseline）位置。不处理换行符（遇到 `\n` 停止）。

#### Scenario: 渲染 ASCII 文字
- **WHEN** 调用 `ui_canvas_draw_text(fb, 24, 100, font, "sunshine", 0x00)`
- **THEN** framebuffer 中从 (24, 100) 基线位置 SHALL 渲染出 "sunshine" 的黑色文字

#### Scenario: 渲染 CJK 文字
- **WHEN** 调用 `ui_canvas_draw_text(fb, 24, 100, font, "奇迹闪耀", 0x00)`
- **THEN** framebuffer 中 SHALL 渲染出 4 个汉字，每个字符宽度约等于字号大小

#### Scenario: 逻辑坐标系正确映射
- **WHEN** 在逻辑坐标 (0, 50) 绘制文字
- **THEN** 文字 SHALL 出现在屏幕竖屏模式的左上区域，与 `ui_canvas_draw_pixel` 的坐标系一致

#### Scenario: NULL 或空字符串
- **WHEN** text 为 NULL 或空字符串
- **THEN** SHALL 安全返回，不写入 framebuffer

### Requirement: 文字宽度度量
系统 SHALL 提供 `ui_canvas_measure_text(const EpdFont *font, const char *text)` 函数，返回文字渲染后占用的像素宽度（不实际写入 framebuffer）。

#### Scenario: 度量英文文字
- **WHEN** 调用 `ui_canvas_measure_text(font_20, "sun")`
- **THEN** SHALL 返回 3 个字符 advance_x 之和

#### Scenario: 度量 CJK 文字
- **WHEN** 调用 `ui_canvas_measure_text(font_20, "你好")`
- **THEN** SHALL 返回 2 个汉字 advance_x 之和（通常约等于 2 × 字号）

#### Scenario: 空字符串
- **WHEN** text 为 NULL 或空字符串
- **THEN** SHALL 返回 0

### Requirement: 灰度 alpha 混合
字符渲染时 SHALL 使用 glyph 的 4bpp alpha 值（0-15）在前景色和背景色（白色 0xF0）之间做线性插值：`color = bg + alpha * (fg - bg) / 15`。alpha=0 时不写入像素，保留 framebuffer 原有内容。

#### Scenario: 全黑前景抗锯齿
- **WHEN** fg_color = 0x00，glyph 某像素 alpha = 8
- **THEN** 该像素 SHALL 渲染为约 0x70（中间灰度）

#### Scenario: 完全透明像素
- **WHEN** glyph 某像素 alpha = 0
- **THEN** framebuffer 对应位置 SHALL 不被修改

### Requirement: 压缩字形解码
对于使用 zlib 压缩的字体（`EpdFont.compressed == true`），渲染器 SHALL 在绘制前动态解压 glyph bitmap，渲染后释放临时内存。

#### Scenario: 压缩字形正确渲染
- **WHEN** 使用压缩字体渲染一个字符
- **THEN** 渲染结果 SHALL 与未压缩版本完全一致

#### Scenario: 内存释放
- **WHEN** 渲染一个压缩字形完成后
- **THEN** 解压用的临时 buffer SHALL 被释放，不产生内存泄漏

### Requirement: 坐标越界安全
当字符的 glyph 像素超出屏幕范围（0-539 × 0-959）时，渲染器 SHALL 跳过越界像素，不发生 framebuffer 越界写入。

#### Scenario: 文字部分超出右侧
- **WHEN** 在 x=520 渲染一个 20px 宽的汉字
- **THEN** 超出 x=539 的像素 SHALL 被裁剪，不写入 framebuffer

#### Scenario: 文字超出底部
- **WHEN** 在 y=955 渲染文字
- **THEN** 超出 y=959 的像素 SHALL 被裁剪
