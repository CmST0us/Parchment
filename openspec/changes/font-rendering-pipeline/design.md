## Context

Parchment 阅读器的 UI 框架层已完成（canvas、event、page stack、render pipeline、touch），但缺少文字渲染能力。当前 `ui_canvas` 只提供几何图形绘制。所有正式 UI 页面（书库、阅读、设置、目录）都依赖文字渲染，因此字体管线是下一阶段开发的前置依赖。

参考项目 [M5ReadPaper](https://github.com/shinemoon/M5ReadPaper) 已验证：在 ESP32 + E-Ink 设备上，预渲染 binary bitmap font 是成熟可行的方案。

### 硬件约束

- ESP32-S3R8：8MB PSRAM，16MB Flash
- ED047TC2：540×960 逻辑分辨率，4bpp 灰度 (16 级)
- Framebuffer：~253KB (PSRAM)
- SD 卡：SPI 接口，标准 VFS 文件操作

## Goals / Non-Goals

**Goals:**

- 从 SD 卡加载预渲染的 .bin 字体文件并渲染到 framebuffer
- 支持 4 档预设字号：24/28/32/36 像素
- 支持 UTF-8 编码的中英文混排文本
- 提供文本测量 API（不渲染，仅计算宽高）
- 提供 Python 工具将 TTF/OTF 转换为 .bin 格式
- 将测试页面替换为字体渲染验证页面

**Non-Goals:**

- EPUB / HTML 解析（后续 change）
- 完整的文本排版引擎（分页、段落间距等，后续 change）
- 竖排文字
- 繁简转换 / GBK 编码
- 字体缓存的高级策略（5 页滑动窗口等，后续优化）
- 运行时 TTF 光栅化
- 反锯齿 / 灰度字体渲染（1-bit 黑白足够用于 E-Ink）

## Decisions

### D1: 使用预渲染 1-bit binary font 格式

**选择**: 离线将 TTF 渲染为 1-bit bitmap .bin 文件，运行时直接读取 bitmap 数据。

**备选方案**:
- (A) stb_truetype 实时光栅化：ESP32 CPU 性能不足以实时渲染 CJK 字符，单个汉字渲染可能需要数十毫秒
- (B) 固定点阵字体：无法支持多字号
- (C) 预渲染灰度/2-bit font：文件体积更大，且 E-Ink 黑白对比足够清晰

**理由**: M5ReadPaper 已验证此方案在同款硬件上可行。1-bit 格式文件最小，解码最快。

### D2: .bin 文件格式兼容 M5ReadPaper V2

**选择**: 采用 M5ReadPaper 的 V2 (1-bit) 字体文件格式。

**格式详情**:
```
Header (134 bytes):
  char_count    uint32   字符总数
  font_height   uint8    像素高度
  version       uint8    固定为 2
  family_name   char[64] 字体族名 (UTF-8, null 结尾)
  style_name    char[64] 字体样式名

Glyph Table (20 bytes × char_count):
  unicode       uint16   Unicode 码位
  advance_w     uint16   步进宽度
  bitmap_w      uint8    bitmap 宽度
  bitmap_h      uint8    bitmap 高度
  x_offset      int8     水平偏移
  y_offset      int8     垂直偏移
  data_offset   uint32   bitmap 数据在文件中的字节偏移
  data_size     uint32   bitmap 数据长度
  reserved      uint32   保留字段

Bitmap Data:
  1-bit packed, MSB first, 每行 ceil(bitmap_w/8) bytes
  1=前景(黑), 0=背景(透明)
```

**理由**: 复用已验证的格式和生成工具，无需自定义格式。

### D3: Glyph 索引使用 hash map 存储在 PSRAM

**选择**: 加载字体时，将所有 glyph 元数据（不含 bitmap）读入 PSRAM，使用 hash map 按 unicode 码位索引。

**内存估算**:
- 完整 CJK 字符集 ~20,000 字符
- 每字符索引：16 bytes (unicode + advance + dimensions + file offset)
- 总计：~320KB PSRAM
- Hash map 开销：~80KB
- 合计：~400KB（PSRAM 总量 8MB 的 5%）

**备选方案**:
- (A) 排序数组 + 二分查找：内存更小，但查找 O(log N) 比 O(1) 慢
- (B) 完全按需从 SD 读取：无内存占用但每个字符都要 SD I/O，太慢

**理由**: PSRAM 充裕，O(1) 查找对渲染性能至关重要。

### D4: Bitmap 数据按需从 SD 卡读取

**选择**: Glyph bitmap 不预加载到内存，每次渲染时从 SD 卡按偏移量读取。

**理由**: bitmap 数据总量可能数十 MB，无法全部加载。SD 卡随机读取速度足够（单个 glyph ~数十到数百 bytes，读取时间 <1ms）。后续可加缓存层优化。

### D5: UTF-8 解码内联实现

**选择**: 在 `ui_font.c` 中实现轻量 UTF-8 decoder，逐字符解码 codepoint。

**理由**: UTF-8 解码逻辑简单（~30 行代码），无需引入外部库。仅需支持 BMP (U+0000~U+FFFF)，覆盖所有 CJK 和常用字符。

### D6: 换行策略

**选择**: 逐字符推进 x 坐标，超出行宽时换行。CJK 字符逐字可断；ASCII 单词尽量不拆分。

**理由**: 这是最基本的换行规则，对阅读器 MVP 足够。后续可增强为更精细的排版算法。

### D7: ui_canvas 新增 1-bit bitmap 绘制函数

**选择**: 在 `ui_canvas` 中新增 `ui_canvas_draw_bitmap_1bit()` 函数，将 1-bit packed bitmap 绘制到 4bpp framebuffer。

**映射规则**: bit=1 → 写入指定前景色（如 0x00 黑色）；bit=0 → 跳过（保留背景色，实现透明效果）。

**理由**: 字体渲染是透明叠加（只画黑色笔画，不画白色背景），需要和现有的 `ui_canvas_draw_bitmap`（4bpp 不透明位图）区分。

### D8: Python 工具改造自 M5ReadPaper

**选择**: 基于 M5ReadPaper 的 `generate_1bit_font_bin.py` 改造，精简功能。

**保留**: TTF→1-bit bitmap 核心流程、edge smoothing、字符集选择
**移除**: Huffman V3 编码、PROGMEM 导出、GBK 表生成
**修改**: 默认字符集改为 ASCII + Unicode CJK 统一汉字基本区 (U+4E00~U+9FFF) + 常用标点

## Risks / Trade-offs

- **[每字号一个 .bin 文件]** → 4 个字号 × 20K 字符 ≈ 每文件 5-10MB，SD 卡需 20-40MB 空间。对于 GB 级 SD 卡可接受。
- **[首次加载字体较慢]** → 读取 20K × 20 bytes 索引 ≈ 400KB，SD 卡读取约 0.5-1 秒。可在启动画面期间完成，用户无感知。
- **[无 glyph bitmap 缓存]** → 每次渲染都从 SD 读取。单页可能有 200-300 个不同字符，每个读取 <1ms，总计 <300ms。对于 E-Ink ~600ms 刷新周期可接受，但后续应加缓存。
- **[Unicode BMP 限制]** → 不支持 Emoji 和稀有汉字（扩展区）。对阅读器 MVP 足够。
- **[1-bit 无反锯齿]** → 在 235PPI 密度下，1-bit 黑白文字清晰度足够。E-Ink 本身对比度高，反锯齿收益有限。
