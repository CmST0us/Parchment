## MODIFIED Requirements

### Requirement: 预编译字体资源
系统 SHALL 提供两类预编译字体资源：

1. **UI 嵌入字体**: Noto Sans CJK SC 的 20px 和 28px，以 epdiy `EpdFont` 格式存储在 `components/ui_core/fonts/` 目录下的 C header 文件中（`ui_font_20.h`、`ui_font_28.h`）。字符集为 ASCII 可打印字符 + GB2312 一级常用字（~3,850 字符）。

2. **阅读字体**: Noto Sans CJK SC 的 24px 和 32px（默认出厂配置），以 .pfnt 二进制格式存储在 `fonts_data/` 目录，构建时自动打包到 LittleFS 分区。字符集为 GB18030（~27,564 字符）。

#### Scenario: UI 字体文件存在
- **WHEN** 构建项目
- **THEN** SHALL 存在以下字体头文件：`ui_font_20.h`、`ui_font_28.h`

#### Scenario: 阅读字体文件存在
- **WHEN** 查看 `fonts_data/` 目录
- **THEN** SHALL 存在 `noto_cjk_24.pfnt`、`noto_cjk_32.pfnt`

#### Scenario: UI 字体数据结构
- **WHEN** 包含 `ui_font_20.h`
- **THEN** SHALL 导出 `const EpdFont ui_font_20` 全局变量

### Requirement: 字号选择 API
系统 SHALL 提供 `ui_font.h` 头文件，声明 UI 嵌入字体的 `extern` 引用，并提供初始化、字号选择和字体列表查询函数。

#### Scenario: 初始化
- **WHEN** 调用 `ui_font_init()`
- **THEN** SHALL 挂载 LittleFS 并扫描可用阅读字体

#### Scenario: 获取 UI 字号
- **WHEN** 调用 `ui_font_get(20)`
- **THEN** SHALL 返回指向 ROM 中 `ui_font_20` 的指针

#### Scenario: 获取阅读字号
- **WHEN** 调用 `ui_font_get(24)` 且 LittleFS 中存在 `noto_cjk_24.pfnt`
- **THEN** SHALL 返回指向 PSRAM 中动态加载字体的指针

#### Scenario: 超出范围
- **WHEN** 调用 `ui_font_get(10)` 或 `ui_font_get(100)`
- **THEN** SHALL 返回最近匹配的可用字体

#### Scenario: 列出可用阅读字体
- **WHEN** 调用 `ui_font_list_available(sizes, max)`
- **THEN** SHALL 返回 LittleFS 中所有 .pfnt 文件的字号列表

### Requirement: 字符集覆盖
UI 嵌入字体 SHALL 覆盖 ASCII 可打印字符（0x20-0x7E）和 GB2312 一级常用字（3,755 个汉字），以及常用中日文标点。阅读字体 SHALL 覆盖 GB18030 强制集 BMP 范围内全部字符。

#### Scenario: UI 字体渲染常用汉字
- **WHEN** 使用 UI 字体渲染 GB2312 一级常用字中的任意字符
- **THEN** `epd_get_glyph(font, codepoint)` SHALL 返回非 NULL

#### Scenario: 阅读字体渲染生僻字
- **WHEN** 使用阅读字体渲染 GB18030 BMP 范围内的任意 CJK 字符
- **THEN** `epd_get_glyph(font, codepoint)` SHALL 返回非 NULL

#### Scenario: 阅读字体渲染日文
- **WHEN** 使用阅读字体渲染平假名或片假名字符
- **THEN** `epd_get_glyph(font, codepoint)` SHALL 返回非 NULL

### Requirement: 字体生成工具
项目 SHALL 在 `tools/fontconvert.py` 提供字体转换脚本，支持输出 C header 和 .pfnt 二进制两种格式，支持 `custom`/`gb2312-1`/`gbk`/`gb18030` 字符集选项。

#### Scenario: 生成 UI 字体 header
- **WHEN** 运行 `python tools/fontconvert.py ui_font_20 20 NotoSansCJKsc.otf --compress --charset gb2312-1`
- **THEN** SHALL 输出合法的 C header

#### Scenario: 生成阅读字体 pfnt
- **WHEN** 运行 `python tools/fontconvert.py noto_cjk_24 24 NotoSansCJKsc.otf --compress --output-format pfnt --charset gb18030`
- **THEN** SHALL 输出合法的 .pfnt 二进制文件

## REMOVED Requirements

### Requirement: 旧字体文件存在（原"字体文件存在"场景）
**Reason**: 旧的 6 个字号字体文件（noto_sans_cjk_20/24/28/32/36/40.h）被新的 UI 字体文件（ui_font_20.h、ui_font_28.h）和 .pfnt 阅读字体替代。
**Migration**: 使用 `ui_font_get(size)` 获取字体，API 不变。20px 和 28px 直接匹配新 UI 字体；其他字号从 LittleFS 加载阅读字体。
