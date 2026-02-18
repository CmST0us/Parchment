## ADDED Requirements

### Requirement: Canvas 构造与裁剪区域
`ink::Canvas` 对象 SHALL 通过 framebuffer 指针和裁剪 Rect（屏幕绝对坐标）构造。所有绘图操作使用相对于裁剪区域左上角的局部坐标，超出裁剪区域的像素 SHALL 被自动丢弃。

#### Scenario: 构造 Canvas 并设置裁剪区域
- **WHEN** 使用 `Canvas(fb, Rect{100, 200, 80, 40})` 构造
- **THEN** `clipRect()` 返回 `{100, 200, 80, 40}`，局部坐标 `(0,0)` 映射到屏幕坐标 `(100, 200)`

#### Scenario: 绘图超出裁剪区域被自动丢弃
- **WHEN** 在 clip 为 `{100, 200, 80, 40}` 的 Canvas 上执行 `fillRect({-10, -10, 50, 50}, BLACK)`
- **THEN** 只有 `(0,0)` 到 `(39,39)` 范围内的像素被写入 framebuffer，`(-10,-10)` 到 `(-1,-1)` 的像素被丢弃

### Requirement: 子 Canvas 裁剪嵌套
`clipped(subRect)` SHALL 返回新的 Canvas，其裁剪区域为当前 clip 与 subRect（局部坐标）转为绝对坐标后的交集。

#### Scenario: 创建嵌套裁剪的子 Canvas
- **WHEN** 父 Canvas clip 为 `{100, 200, 80, 40}`，调用 `clipped({10, 5, 200, 200})`
- **THEN** 返回的子 Canvas clip 为 `{110, 205, 70, 35}`（交集后被父 clip 截断）

#### Scenario: 子 Canvas 完全超出父 clip
- **WHEN** 父 Canvas clip 为 `{100, 200, 80, 40}`，调用 `clipped({100, 0, 50, 50})`
- **THEN** 返回的子 Canvas clip 为空矩形，所有绘图操作无效果

### Requirement: 清除操作
`clear(gray)` SHALL 将整个裁剪区域填充为指定灰度值。默认参数为 `0xFF`（白色）。

#### Scenario: 清除为白色
- **WHEN** 对 clip 为 `{0, 0, 540, 48}` 的 Canvas 调用 `clear()`
- **THEN** 屏幕坐标 `(0,0)` 到 `(539,47)` 的所有像素设为 `0xFF`

### Requirement: 几何图形绘制
Canvas SHALL 提供以下几何绘图方法，所有坐标为局部坐标，自动裁剪：
- `fillRect(Rect, gray)`: 填充矩形
- `drawRect(Rect, gray, thickness)`: 矩形边框
- `drawHLine(x, y, w, gray)`: 水平线
- `drawVLine(x, y, h, gray)`: 垂直线
- `drawLine(from, to, gray)`: Bresenham 任意直线

#### Scenario: fillRect 正常绘制
- **WHEN** 在 clip 为 `{0, 0, 540, 960}` 的 Canvas 上调用 `fillRect({10, 20, 100, 50}, 0x00)`
- **THEN** 屏幕坐标 `(10,20)` 到 `(109,69)` 的像素被设为黑色

#### Scenario: drawRect 绘制边框
- **WHEN** 调用 `drawRect({0, 0, 100, 50}, 0x00, 2)`
- **THEN** 绘制 2px 粗的黑色矩形边框，内部不填充

### Requirement: 位图绘制
Canvas SHALL 提供两种位图绘制方法：
- `drawBitmap(data, x, y, w, h)`: 4bpp 灰度位图直接绘制
- `drawBitmapFg(data, x, y, w, h, fgColor)`: 4bpp alpha 位图，以 alpha 值混合前景色与背景

#### Scenario: drawBitmapFg alpha 混合
- **WHEN** 位图像素 alpha 为 0x0F（完全不透明），前景色为 BLACK
- **THEN** 该像素写入 BLACK
- **WHEN** 位图像素 alpha 为 0x00（完全透明）
- **THEN** framebuffer 原值保持不变
- **WHEN** 位图像素 alpha 为中间值
- **THEN** 像素值为 `bg + alpha * (fg - bg) / 15` 线性插值

### Requirement: Color::Clear 透明色常量
`ink::Color` 命名空间 SHALL 包含 `Clear` 常量，值为 `0x01`。此值作为哨兵表示"透明，不绘制背景"。

`0x01` 不是 4bpp 灰度中的有效灰度级（有效值为 0x00, 0x10, 0x20, ..., 0xF0），仅用于 `backgroundColor` 属性的透明判断。

#### Scenario: Color::Clear 值
- **WHEN** 读取 `Color::Clear`
- **THEN** 值为 `0x01`

#### Scenario: Color::Clear 与 Color::Black 区分
- **WHEN** 比较 `Color::Clear` 与 `Color::Black`
- **THEN** 两者不相等（`0x01 != 0x00`）

### Requirement: 文字渲染
Canvas SHALL 提供文字绘制方法，接受 `const EpdFont*` 指针：
- `drawText(font, text, x, y, color)`: 绘制 UTF-8 单行文字，y 为基线坐标
- `drawTextN(font, text, maxBytes, x, y, color)`: 绘制指定最大字节数的文字
- `measureText(font, text)`: 返回文字渲染后的像素宽度（不写入 framebuffer）

文字渲染 SHALL 支持 zlib 压缩的 glyph bitmap 解压。

字符渲染的 alpha 混合 SHALL 读取 framebuffer 中的实际像素值作为背景色，使用 `bg + alpha * (fg - bg) / 15` 公式进行线性插值。此行为 SHALL 与 `drawBitmapFg()` 的 alpha 混合逻辑一致。

#### Scenario: 绘制中文文字
- **WHEN** 使用已加载的 `EpdFont*`（24px 中文字体），调用 `drawText(font, "你好", 10, 30, 0x00)`
- **THEN** "你好" 两个字在局部坐标 `(10, 30)` 基线位置渲染，使用抗锯齿 alpha 混合

#### Scenario: measureText 度量宽度
- **WHEN** 调用 `measureText(font, "ABC")`
- **THEN** 返回三个字符 advance_x 的累计值

#### Scenario: 文字被裁剪区域截断
- **WHEN** 文字渲染位置接近裁剪区域右边界
- **THEN** 超出裁剪区域的字符像素被自动丢弃，不写入 framebuffer

#### Scenario: 非白色背景上的文字反走样
- **WHEN** 在灰色背景 (0x80) 上绘制黑色文字
- **THEN** 文字边缘的半透明像素与灰色背景正确混合，无白色光晕

#### Scenario: 白色背景上的文字反走样
- **WHEN** 在白色背景 (0xF0) 上绘制黑色文字
- **THEN** alpha 混合结果与此前行为一致（背景为白色时 getPixel 返回 0xF0）

### Requirement: 坐标变换
Canvas 内部 SHALL 将逻辑坐标（540×960 portrait）正确变换到物理 framebuffer（960×540 landscape）。变换规则：`px = ly, py = 539 - lx`。4bpp 打包格式：`buf[py * 480 + px/2]`，偶数 px 在低 4 位，奇数在高 4 位。

#### Scenario: 逻辑坐标到物理映射
- **WHEN** 在逻辑坐标 `(0, 0)`（左上角）写入像素
- **THEN** 物理坐标为 `(0, 539)`
- **WHEN** 在逻辑坐标 `(539, 959)`（右下角）写入像素
- **THEN** 物理坐标为 `(959, 0)`

### Requirement: framebuffer 访问
Canvas SHALL 提供 `framebuffer()` 方法返回底层 framebuffer 指针，供 RenderEngine 和 OverlayView backing store 使用。

#### Scenario: 获取 framebuffer 指针
- **WHEN** 调用 `canvas.framebuffer()`
- **THEN** 返回构造时传入的 `uint8_t*` 指针
