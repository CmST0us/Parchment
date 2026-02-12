## ADDED Requirements

### Requirement: LittleFS 分区定义
系统 SHALL 在 `partitions.csv` 中定义名为 `fonts` 的 LittleFS 数据分区，大小为 10MB（0xA00000），起始偏移 0x410000，subtype 为 `spiffs`。

#### Scenario: 分区表包含 fonts 分区
- **WHEN** 查看 `partitions.csv`
- **THEN** SHALL 包含 `fonts, data, spiffs, 0x410000, 0xA00000,` 条目

#### Scenario: 原 storage 分区移除
- **WHEN** 查看 `partitions.csv`
- **THEN** SHALL 不再包含名为 `storage` 的分区

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
- **THEN** SHALL 包含: magic "PFNT"（4B）、version u8、flags u8（bit0=compressed）、font_size_px u16、advance_y u16、ascender i16、descender i16、interval_count u32、glyph_count u32、reserved 8B

#### Scenario: Unicode intervals 段
- **WHEN** 读取 header 之后的数据
- **THEN** SHALL 包含 interval_count 个 12 字节记录，每个含 first u32、last u32、glyph_offset u32

#### Scenario: Glyph table 段
- **WHEN** 读取 intervals 之后的数据
- **THEN** SHALL 包含 glyph_count 个 20 字节记录，每个含 width u16、height u16、advance_x u16、left i16、top i16、compressed_size u32、data_offset u32

#### Scenario: Bitmap data 段
- **WHEN** 读取 glyph table 之后的数据
- **THEN** SHALL 包含所有 glyph 的 zlib 压缩 4bpp bitmap 数据，每个 glyph 的数据位于其 data_offset 处，长度为 compressed_size

### Requirement: 字体文件加载到 PSRAM
系统 SHALL 能够从 LittleFS 读取 .pfnt 文件，将全部数据加载到 PSRAM 中，并构建标准 `EpdFont` 结构体。

#### Scenario: 加载成功
- **WHEN** 调用内部加载函数传入有效 .pfnt 路径
- **THEN** SHALL 返回指向 PSRAM 中 `EpdFont` 结构体的指针，其 `bitmap`、`glyph`、`intervals` 字段均指向 PSRAM 分配的内存

#### Scenario: 文件不存在
- **WHEN** 传入不存在的路径
- **THEN** SHALL 返回 NULL，不崩溃

#### Scenario: magic 不匹配
- **WHEN** 文件前 4 字节不为 "PFNT"
- **THEN** SHALL 返回 NULL，记录错误日志

#### Scenario: PSRAM 分配失败
- **WHEN** PSRAM 剩余空间不足以容纳字体数据
- **THEN** SHALL 返回 NULL，记录错误日志，不触发 abort

### Requirement: 字体生命周期管理
系统 SHALL 支持加载和卸载阅读字体，同一时间最多只有一个阅读字体处于加载状态。

#### Scenario: 卸载当前字体后加载新字体
- **WHEN** 当前已加载 24px 阅读字体，用户请求切换到 32px
- **THEN** SHALL 先释放 24px 字体的全部 PSRAM 内存，再加载 32px 字体

#### Scenario: 列出可用字体
- **WHEN** 调用 `ui_font_list_available(sizes_out, max_count)`
- **THEN** SHALL 扫描 `/fonts` 目录下所有 .pfnt 文件，解析 header 中的 font_size_px，写入 sizes_out 数组，返回实际数量

### Requirement: ui_font_get 透明路由
`ui_font_get(int size_px)` SHALL 根据请求的字号自动选择字体来源：若请求的字号匹配 UI 嵌入字体（20px 或 28px），返回 ROM 指针；否则返回当前已加载的阅读字体指针。

#### Scenario: 请求 UI 字号
- **WHEN** 调用 `ui_font_get(20)` 或 `ui_font_get(28)`
- **THEN** SHALL 返回 ROM 中嵌入的 UI 字体指针

#### Scenario: 请求阅读字号且已加载
- **WHEN** 已加载 24px 阅读字体，调用 `ui_font_get(24)`
- **THEN** SHALL 返回 PSRAM 中的阅读字体指针

#### Scenario: 请求阅读字号但未加载
- **WHEN** 未加载任何阅读字体，调用 `ui_font_get(24)`
- **THEN** SHALL 自动从 LittleFS 加载最近匹配的 .pfnt 文件并返回；若无可用文件则 fallback 到最近的 UI 嵌入字体

#### Scenario: 调用方代码无需修改
- **WHEN** 现有代码调用 `ui_font_get(size)` 并使用返回的 `EpdFont*` 进行渲染
- **THEN** 渲染结果 SHALL 正确，无需修改调用方代码
