## Context

当前字体系统将 Noto Sans CJK SC 的 6 个字号（20-40px）以 C header 嵌入固件 ROM，每个字号仅含 312 个手选字符。作为阅读器需要完整 CJK 覆盖（GB18030 ~27,564 字符），嵌入方式在 ROM 体积和字符覆盖上均不可行。

项目已有 esp_littlefs submodule（v1.20.4）但未使用。Flash 16MB 中 `storage` FAT 分区（12MB）实际未被任何代码使用（SD 卡通过独立 SPI 挂载），可直接替换。当前固件仅 0.8MB，4MB factory 分区空间充裕。

PSRAM 8MB，扣除 framebuffer（~260KB）和 epdiy 内部缓冲（~200KB），可用约 7.5MB。一个 GB18030 字号的压缩 bitmap 数据约 4.5MB，加载一个阅读字体后仍余 ~2.5MB。

## Goals / Non-Goals

**Goals:**
- 支持 GB18030 完整字符集的阅读字体，从 LittleFS 分区动态加载
- UI 字体（ASCII + GB2312 一级）继续嵌入 ROM，确保启动画面等无需等待文件系统
- 自定义 `.pfnt` 二进制格式，用户可替换字体文件
- 字体构建完全集成到 `idf.py build` 流程
- `ui_font_get()` API 签名不变，调用方零修改

**Non-Goals:**
- 运行时 TTF/OTF 光栅化（不引入 FreeType/stb_truetype）
- SD 卡自定义字体加载（本次仅预留接口）
- 字体 fallback 链（多字体混合渲染）
- OTA 字体更新

## Decisions

### 1. 分区方案：替换 storage 为 fonts LittleFS

**选择**: 将 `storage` FAT 分区（12MB）替换为 `fonts` LittleFS 分区（10MB）。

**替代方案**:
- 缩小 factory 分区腾出空间 → 不必要，storage 完全未使用
- 在 SD 卡上存储字体 → 读取速度慢，且 SD 卡可能不在位

**新分区表**:
```
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x400000,
fonts,    data, spiffs,  0x410000,0xA00000,
```

注意：esp_littlefs 使用 `spiffs` subtype 注册分区，这是其实现约定。剩余约 2MB（0xE10000-0xFFFFFF）未分配，预留后续扩展。

### 2. 二进制字体格式 (.pfnt)

**选择**: 自定义二进制格式，结构与 EpdFont 一一对应。

**格式布局**:
```
Offset  内容
0x00    Header (32 bytes)
          magic: "PFNT" (4B)
          version: u8 (当前 = 1)
          flags: u8 (bit0: compressed)
          font_size_px: u16
          advance_y: u16
          ascender: i16
          descender: i16
          interval_count: u32
          glyph_count: u32
          reserved: 8B (全零)
0x20    Unicode Intervals [interval_count × 12B]
          first: u32, last: u32, glyph_offset: u32
var     Glyph Table [glyph_count × 20B]
          width: u16, height: u16, advance_x: u16,
          left: i16, top: i16,
          compressed_size: u32, data_offset: u32
var     Bitmap Data (zlib 压缩的 4bpp glyph 位图)
```

**替代方案**:
- 直接使用 raw flash mmap → LittleFS 文件不连续，无法 mmap
- PCF/BDF 标准格式 → 解析复杂，不匹配 EpdFont 结构

**理由**: 格式字段与 `EpdFont`/`EpdGlyph`/`EpdUnicodeInterval` 完全对齐，加载时直接 memcpy 到结构体数组，无需转换。

### 3. 运行时加载策略：全量加载到 PSRAM

**选择**: 将整个 .pfnt 文件（metadata + bitmap data）一次性读入 PSRAM，构建标准 `EpdFont` 结构体。

**内存预算** (单个阅读字体):
- intervals 数组: ~数百字节
- glyph 数组: ~27,564 × 20B = ~540KB
- bitmap 数据: ~4MB (压缩)
- 合计: ~4.5MB PSRAM

**替代方案**:
- 按需读取 glyph bitmap + LRU cache → 实现复杂，且 e-ink 翻页渲染是 burst 操作不适合 cache
- 分块加载 → 增加 API 复杂度

**理由**: 全量加载后 `EpdFont*` 指向 PSRAM，所有下游代码（ui_canvas、ui_text、epd_get_glyph）无需任何修改。一次只加载一个阅读字号，切换时 unload + reload。

### 4. 字体分层：UI 嵌入 + 阅读动态

**选择**: 两层字体。

| 层级 | 存储 | 字符集 | 字号 | 加载方式 |
|------|------|--------|------|----------|
| UI 字体 | ROM (.h) | ASCII + GB2312 一级 (~3,850) | 20px, 28px | 编译时嵌入 |
| 阅读字体 | LittleFS (.pfnt) | GB18030 (~27,564) | 默认 24px, 32px | 运行时 PSRAM |

**替代方案**:
- 全部从 LittleFS 加载 → boot screen 需要等文件系统挂载，体验差
- 全部嵌入 ROM → GB18030 太大，4MB factory 放不下

### 5. 构建集成：CMake littlefs_create_partition_image

**选择**: 使用 esp_littlefs 提供的 `littlefs_create_partition_image()` CMake 函数，在 build 阶段自动将 `fonts_data/` 目录打包为 LittleFS 镜像。

**流程**:
```
fonts_data/
  noto_cjk_24.pfnt
  noto_cjk_32.pfnt
      │
      ▼  CMake: littlefs_create_partition_image(fonts ...)
  build/fonts.bin
      │
      ▼  idf.py flash
  Flash 0x410000 (fonts partition)
```

`littlefs_create_partition_image` 需要在使用 esp_littlefs 的组件的 CMakeLists.txt 中调用（通常是 main 或 ui_core）。

### 6. fontconvert.py 扩展

**选择**: 在现有脚本中新增 `--output-format` 参数（`header` 或 `pfnt`），以及 `--charset` 参数（`custom`/`gb2312-1`/`gbk`/`gb18030`）。

默认行为不变（`--output-format header --charset custom`），保持向后兼容。

GB18030 字符集定义以 Unicode 码点范围表示，内置在脚本中。

## Risks / Trade-offs

**[PSRAM 紧张]** → 加载阅读字体后仅余 ~2.5MB 自由 PSRAM。
→ 缓解: 监控 `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`；后续如有需要可改为按需加载策略。

**[LittleFS 分区首次烧录]** → 用户更新固件时若仅 flash factory 分区，fonts 分区不受影响。但首次或分区表变更时需完整 flash。
→ 缓解: `idf.py flash` 默认会写入所有分区；文档说明分区表变更后需完整 flash。

**[GB18030 .pfnt 文件生成耗时]** → 27,564 个字符的 FreeType 光栅化 + zlib 压缩可能需要数分钟。
→ 缓解: 这是离线一次性操作（换字体时才需要），预生成文件提交到 `fonts_data/`。

**[10MB 分区容量]** → GB18030 × 2 尺寸 ≈ 9MB，余量不大。
→ 缓解: 用户如需更多字号可删除不用的字号文件释放空间。

**[旧 .h 字体文件删除]** → 删除 6 个旧字体头文件会导致任何直接引用它们的代码编译失败。
→ 缓解: 替换为新 UI 字体头文件，`ui_font_get()` 接口不变。

## Open Questions

- GB18030 精确覆盖范围：是否包含 CJK 扩展 B 区（rare 字符，会显著增大文件）？建议首版仅覆盖 GB18030 强制部分（BMP 内约 27,564 字符），不含扩展 B/C/D。
