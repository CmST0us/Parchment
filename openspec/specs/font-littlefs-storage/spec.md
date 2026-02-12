## Requirements

### Requirement: LittleFS 分区定义
系统 SHALL 在 `partitions.csv` 中定义名为 `fonts` 的 LittleFS 数据分区，大小为 15.6MB（0xEF0000），起始偏移 0x110000，subtype 为 `spiffs`。

#### Scenario: 分区表包含 fonts 分区
- **WHEN** 查看 `partitions.csv`
- **THEN** SHALL 包含 `fonts, data, spiffs, 0x110000, 0xEF0000,` 条目

#### Scenario: factory 分区大小
- **WHEN** 查看 `partitions.csv`
- **THEN** factory 分区大小 SHALL 为 1MB（0x100000），固件 ~466KB 有充足余量

### Requirement: LittleFS 挂载
系统 SHALL 提供 `ui_font_init()` 函数，在应用启动时将 `fonts` 分区以 LittleFS 格式挂载到 VFS 路径 `/fonts`。

#### Scenario: 成功挂载
- **WHEN** 调用 `ui_font_init()`
- **THEN** LittleFS SHALL 成功挂载，`/fonts` 路径可用于文件操作

#### Scenario: 分区为空或未格式化
- **WHEN** fonts 分区未包含有效 LittleFS 镜像
- **THEN** `ui_font_init()` SHALL 格式化分区并挂载，不崩溃

### Requirement: .pfnt 二进制格式
系统 SHALL 定义 Parchment Font (.pfnt) 二进制格式。每个 .pfnt 文件自包含一个字体的一个字号的全部数据。

#### Scenario: 文件头结构
- **WHEN** 读取 .pfnt 文件前 32 字节
- **THEN** SHALL 包含: magic "PFNT"（4B）、version u8、flags u8（bit0=compressed）、font_size_px u16、advance_y u16、ascender i16、descender i16、interval_count u32、glyph_count u32、reserved 10B

#### Scenario: Unicode intervals 段
- **WHEN** 读取 header 之后的数据
- **THEN** SHALL 包含 interval_count 个 12 字节记录，每个含 first u32、last u32、glyph_offset u32

#### Scenario: Glyph table 段
- **WHEN** 读取 intervals 之后的数据
- **THEN** SHALL 包含 glyph_count 个 20 字节记录，每个含 width u16、height u16、advance_x u16、left i16、top i16、compressed_size u32、data_offset u32、reserved u16

#### Scenario: Bitmap data 段
- **WHEN** 读取 glyph table 之后的数据
- **THEN** SHALL 包含所有 glyph 的 zlib 压缩 4bpp bitmap 数据

### Requirement: 字体文件加载到 PSRAM
系统 SHALL 能够从 LittleFS 读取 .pfnt 文件，将全部数据加载到 PSRAM 中，并构建标准 `EpdFont` 结构体。

#### Scenario: 加载成功
- **WHEN** 调用 `pfnt_load()` 传入有效 .pfnt 路径
- **THEN** SHALL 返回指向 PSRAM 中 `EpdFont` 结构体的指针

#### Scenario: 文件不存在
- **WHEN** 传入不存在的路径
- **THEN** SHALL 返回 NULL，不崩溃

#### Scenario: PSRAM 分配失败
- **WHEN** PSRAM 剩余空间不足
- **THEN** SHALL 返回 NULL，记录错误日志，不触发 abort

### Requirement: 字体生命周期管理
UI 字体（`ui_font_*` 前缀）SHALL 在 boot 时常驻加载，永不卸载。阅读字体按需加载，同一时间最多一个处于加载状态。

#### Scenario: Boot 加载 UI 字体
- **WHEN** `ui_font_init()` 扫描到 `ui_font_*.pfnt` 文件
- **THEN** SHALL 立即加载到 PSRAM，作为常驻字体

#### Scenario: 切换阅读字体
- **WHEN** 当前已加载 24px 阅读字体，用户请求切换到 32px
- **THEN** SHALL 先释放 24px 字体的全部 PSRAM 内存，再加载 32px 字体

### Requirement: PSRAM 预算
系统 PSRAM 总计 8MB。UI 字体常驻 ~1.5MB + framebuffer ~1MB + 阅读字体最大 ~3.3MB = 峰值 ~5.8MB，SHALL 留有余量。
