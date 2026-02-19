## MODIFIED Requirements

### Requirement: Canvas 构造与裁剪区域
`ink::Canvas` 对象 SHALL 通过 `M5GFX*` 显示设备指针和裁剪 Rect（逻辑坐标）构造。所有绘图操作使用相对于裁剪区域左上角的局部坐标，超出裁剪区域的像素 SHALL 被自动丢弃。

Canvas 构造时 SHALL 调用 `display->setClipRect(clip.x, clip.y, clip.w, clip.h)` 设置 M5GFX 裁剪区域。

#### Scenario: 构造 Canvas 并设置裁剪区域
- **WHEN** 使用 `Canvas(display, Rect{100, 200, 80, 40})` 构造
- **THEN** `clipRect()` 返回 `{100, 200, 80, 40}`，局部坐标 `(0,0)` 映射到逻辑屏幕坐标 `(100, 200)`

#### Scenario: 绘图超出裁剪区域被自动丢弃
- **WHEN** 在 clip 为 `{100, 200, 80, 40}` 的 Canvas 上执行 `fillRect({-10, -10, 50, 50}, BLACK)`
- **THEN** 只有 `(0,0)` 到 `(39,39)` 范围内的像素被写入，`(-10,-10)` 到 `(-1,-1)` 的像素被丢弃

### Requirement: 清除操作
`clear(gray)` SHALL 调用 `display->fillRect(clip.x, clip.y, clip.w, clip.h, gray_8bit)` 将整个裁剪区域填充为指定灰度值。默认参数为 `0xFF`（白色）。

颜色值使用 8bpp 灰度：0x00=黑，0xFF=白。

#### Scenario: 清除为白色
- **WHEN** 对 clip 为 `{0, 0, 540, 48}` 的 Canvas 调用 `clear()`
- **THEN** 调用 `display->fillRect(0, 0, 540, 48, 0xFF)`

### Requirement: 几何图形绘制
Canvas SHALL 通过 M5GFX API 提供以下几何绘图方法，所有坐标为局部坐标：
- `fillRect(Rect, gray)`: 调用 `display->fillRect()`
- `drawRect(Rect, gray, thickness)`: 调用 `display->drawRect()`
- `drawHLine(x, y, w, gray)`: 调用 `display->drawFastHLine()`
- `drawVLine(x, y, h, gray)`: 调用 `display->drawFastVLine()`
- `drawLine(from, to, gray)`: 调用 `display->drawLine()`

颜色值 SHALL 使用 8bpp 灰度（0x00=黑, 0xFF=白）。Canvas 内部 SHALL 将局部坐标加上 clip 偏移量转换为屏幕绝对坐标后调用 M5GFX API。

#### Scenario: fillRect 正常绘制
- **WHEN** 在 clip 为 `{0, 0, 540, 960}` 的 Canvas 上调用 `fillRect({10, 20, 100, 50}, 0x00)`
- **THEN** 调用 `display->fillRect(10, 20, 100, 50, 0x00)`

#### Scenario: drawRect 绘制边框
- **WHEN** 调用 `drawRect({0, 0, 100, 50}, 0x00, 2)`
- **THEN** 通过 M5GFX API 绘制 2px 粗的黑色矩形边框

### Requirement: 位图绘制
Canvas SHALL 通过 `display->pushImage()` 提供位图绘制：
- `drawBitmap(data, x, y, w, h)`: 8bpp 灰度位图直接绘制
- `drawBitmapFg(data, x, y, w, h, fgColor)`: 8bpp alpha 位图，以 alpha 值混合前景色与白色背景

#### Scenario: drawBitmap 输出 8bpp 位图
- **WHEN** 传入 8bpp 灰度 bitmap 数据
- **THEN** 通过 `display->pushImage()` 输出到屏幕

### Requirement: 文字渲染（字体引擎集成）
Canvas SHALL 提供文字绘制方法，通过新字体引擎获取 glyph：
- `drawText(fontEngine, text, x, y, fontSize, color)`: 绘制 UTF-8 单行文字
- `measureText(fontEngine, text, fontSize)`: 返回文字渲染后的像素宽度

文字渲染流程：UTF-8 解码 → HashMap 查找 glyph → 缓存命中/解码 → 缩放到目标字号 → `pushImage()` 输出。

#### Scenario: 绘制中文文字
- **WHEN** 调用 `drawText(engine, "你好", 10, 30, 24, 0x00)`
- **THEN** 从字体引擎获取 '你' 和 '好' 的 24px 缩放 bitmap，通过 pushImage 输出到屏幕

#### Scenario: measureText 度量宽度
- **WHEN** 调用 `measureText(engine, "ABC", 20)`
- **THEN** 返回三个字符缩放后 advance_x 的累计值

### Requirement: Color 常量更新
`ink::Color` 命名空间 SHALL 更新为 8bpp 灰度值：
- `Black  = 0x00`
- `Dark   = 0x44`
- `Medium = 0x88`
- `Light  = 0xCC`
- `White  = 0xFF`
- `Clear  = 0x01`（哨兵值，表示透明）

#### Scenario: Color::White 值
- **WHEN** 读取 `Color::White`
- **THEN** 值为 `0xFF`

#### Scenario: Color::Clear 仍为哨兵
- **WHEN** 比较 `Color::Clear` 与 `Color::Black`
- **THEN** 两者不相等（`0x01 != 0x00`）

## REMOVED Requirements

### Requirement: 坐标变换
**Reason**: M5GFX 内部通过 `setRotation()` 自动处理 portrait↔landscape 坐标变换，无需手动 `px = ly, py = 539 - lx` 和 4bpp 打包。
**Migration**: Canvas 直接使用逻辑坐标调用 M5GFX API，所有坐标变换由 M5GFX 内部完成。

### Requirement: framebuffer 访问
**Reason**: M5GFX 管理自己的内部 framebuffer，应用层不直接操作 raw framebuffer 指针。
**Migration**: 所有绘图通过 M5GFX API 执行，RenderEngine 通过 `display()` 方法触发刷新。

### Requirement: 子 Canvas 裁剪嵌套
**Reason**: M5GFX 的 `setClipRect()` 自动处理裁剪区域。Canvas 的 `clipped()` 方法仍存在但实现改为设置 M5GFX 的 clip rect。
**Migration**: `clipped(subRect)` 调用 `display->setClipRect()` 设置新裁剪区域。

### Requirement: Color::Clear 透明色常量
**Reason**: 常量保留但值和语义不变，仅迁入新的 Color 常量体系。
**Migration**: 无需迁移，Color::Clear 仍为 0x01 哨兵值。
