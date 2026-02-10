## ADDED Requirements

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
