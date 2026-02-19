## ADDED Requirements

### Requirement: 面积加权灰度缩放
字体缩放引擎 SHALL 实现面积加权灰度采样算法，将 32px 基础字号的 4bpp glyph bitmap 缩放到目标字号。

缩放因子 = target_size / 32.0，范围 SHALL 限制在 [0.5, 1.0]（对应 16px–32px 目标字号）。

#### Scenario: 缩放到 16px (0.5×)
- **WHEN** 目标字号为 16px，缩放因子为 0.5
- **THEN** 每个目标像素对应 2×2 源像素区域，灰度值为 4 个源像素的加权平均

#### Scenario: 缩放到 24px (0.75×)
- **WHEN** 目标字号为 24px，缩放因子为 0.75
- **THEN** 每个目标像素对应 ~1.33×1.33 源像素区域，使用面积加权混合

#### Scenario: 原始字号不缩放
- **WHEN** 目标字号为 32px，缩放因子为 1.0
- **THEN** 直接使用原始 bitmap，不执行缩放计算

### Requirement: 缩放输出格式
缩放后的 glyph bitmap SHALL 输出为 8bpp 灰度数据（0x00=黑, 0xFF=白），适合直接传递给 `M5.Display.pushImage()`。

#### Scenario: 缩放输出 8bpp
- **WHEN** 对 4bpp 源 bitmap 执行缩放
- **THEN** 输出 buffer 中每个像素为 1 字节灰度值，0x00 表示全黑，0xFF 表示全白

### Requirement: 缩放后尺寸计算
缩放引擎 SHALL 同时计算缩放后的 glyph 度量信息：
- scaled_width = round(original_width × scale)
- scaled_height = round(original_height × scale)
- scaled_advance_x = round(original_advance_x × scale)
- scaled_x_offset = round(original_x_offset × scale)
- scaled_y_offset = round(original_y_offset × scale)

#### Scenario: 尺寸按比例缩放
- **WHEN** 原始 glyph width=24, height=28, advance_x=26，缩放因子 0.75
- **THEN** 缩放后 width=18, height=21, advance_x=20（四舍五入）

### Requirement: 面积加权采样算法
对于每个目标像素 (dx, dy)：
1. 计算其在源 bitmap 中映射的浮点矩形 [sx0, sy0, sx1, sy1]
2. 遍历与该矩形重叠的所有源像素
3. 计算每个源像素与映射矩形的重叠面积
4. 目标灰度 = Σ(源灰度 × 重叠面积) / 总映射面积

源灰度从 4bpp (0-15) 映射到 8bpp (0-255)：`gray_8bit = source_4bit * 17`。

#### Scenario: 整数倍缩放 (0.5×)
- **WHEN** 缩放因子 0.5，源 2×2 区域灰度为 [0, 15, 15, 0]
- **THEN** 目标像素灰度 = (0×17 + 15×17 + 15×17 + 0×17) / 4 = 127（约 0x7F）

#### Scenario: 非整数倍缩放
- **WHEN** 缩放因子 0.75，目标像素映射到源区域跨越像素边界
- **THEN** 按面积权重混合所有重叠源像素的灰度值

### Requirement: 缩放结果缓存
缩放后的 glyph bitmap SHALL 缓存在页面缓存中。同一字符在同一字号下 SHALL NOT 重复执行缩放计算。

#### Scenario: 缩放结果复用
- **WHEN** 第二次请求同一字符的同一字号 glyph
- **THEN** 直接从缓存返回，不重新计算缩放
