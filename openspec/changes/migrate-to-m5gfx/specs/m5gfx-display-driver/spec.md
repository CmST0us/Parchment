## ADDED Requirements

### Requirement: M5GFX 初始化
系统 SHALL 通过 `M5.begin()` 初始化 M5PaperS3 硬件，包括 Panel_EPD 显示驱动、GT911 触摸、电源管理。初始化后 SHALL 调用 `M5.Display.setAutoDisplay(false)` 关闭自动刷新，由 RenderEngine 手动控制刷新时机。

#### Scenario: 成功初始化
- **WHEN** 调用 `M5.begin(cfg)` 且 `cfg.clear_display = true`
- **THEN** 屏幕被清为白色，M5.Display 就绪，坐标系为 540×960 portrait

#### Scenario: 关闭自动刷新
- **WHEN** 初始化完成后设置 `M5.Display.setAutoDisplay(false)`
- **THEN** 后续 `endWrite()` 调用不会自动触发 EPD 刷新

### Requirement: 刷新模式切换
系统 SHALL 通过 `M5.Display.setEpdMode()` 切换 EPD 刷新模式：
- `epd_quality`: 16 级灰度，无闪黑（等价于原 MODE_GL16）
- `epd_text`: 文字优化，带擦除 LUT
- `epd_fast`: 单色快速（等价于原 MODE_DU）
- `epd_fastest`: 单色最快

#### Scenario: 灰度模式刷新
- **WHEN** 设置 `epd_quality` 并调用 `display(x, y, w, h)`
- **THEN** 指定区域以 16 级灰度模式刷新，无闪黑

#### Scenario: 快速模式刷新
- **WHEN** 设置 `epd_fast` 并调用 `display(x, y, w, h)`
- **THEN** 指定区域以单色直接更新模式快速刷新

### Requirement: 局部刷新
系统 SHALL 通过 `M5.Display.display(x, y, w, h)` 对指定区域执行 EPD 局部刷新。坐标使用逻辑坐标（540×960 portrait），M5GFX 内部自动处理 portrait→landscape 坐标变换。

#### Scenario: 局部刷新指定区域
- **WHEN** 调用 `M5.Display.display(0, 100, 540, 80)`
- **THEN** 逻辑坐标 (0,100) 到 (539,179) 的区域被刷新到 EPD

#### Scenario: 等待刷新完成
- **WHEN** 调用 `M5.Display.waitDisplay()`
- **THEN** 阻塞直到 EPD 刷新完成

### Requirement: 全屏清除
系统 SHALL 提供全屏清除功能：`M5.Display.clearDisplay()` 清除 framebuffer 并刷新整个屏幕。

#### Scenario: 启动时清屏
- **WHEN** 调用 `M5.Display.clearDisplay()`
- **THEN** 整个屏幕被清为白色并全屏刷新

### Requirement: 绘图 API 封装
Canvas SHALL 通过 `M5.Display` 的以下 API 执行绘图操作：
- `startWrite()` / `endWrite()`: 批量绘图事务
- `fillRect(x, y, w, h, color)`: 填充矩形
- `drawPixel(x, y, color)`: 单像素绘制
- `drawFastHLine(x, y, w, color)`: 水平线
- `drawFastVLine(x, y, h, color)`: 垂直线
- `drawLine(x0, y0, x1, y1, color)`: 任意直线
- `pushImage(x, y, w, h, data)`: 位图输出

颜色值使用 8bpp 灰度：0x00=黑，0xFF=白。

#### Scenario: 批量绘图
- **WHEN** 在 `startWrite()` 和 `endWrite()` 之间执行多个绘图操作
- **THEN** 所有操作绘制到内部 framebuffer，不触发 EPD 刷新

### Requirement: 电源自动管理
M5GFX Panel_EPD SHALL 自动管理 EPD 高压电源：刷新时自动 poweron，刷新完成后自动 poweroff。应用层代码 SHALL NOT 手动调用任何电源管理函数。

#### Scenario: 无需手动电源管理
- **WHEN** 调用 `display(x, y, w, h)` 触发刷新
- **THEN** 刷新过程中自动 poweron，完成后自动 poweroff，无需应用层干预
