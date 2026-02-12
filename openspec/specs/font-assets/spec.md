## Requirements

### Requirement: 预编译字体资源
系统 SHALL 提供两类预编译字体资源，均以 .pfnt 二进制格式存储在 `fonts_data/` 目录，构建时自动打包到 LittleFS 分区：

1. **UI 字体**: LXGWWenKaiMono 16px 和 24px，文件名 `ui_font_16.pfnt`、`ui_font_24.pfnt`。字符集为 ASCII 可打印字符 + GB2312 一级常用字（~3,755 个汉字）。Boot 时常驻加载到 PSRAM。

2. **阅读字体**: LXGWWenKai 24px 和 32px，文件名 `reading_24.pfnt`、`reading_32.pfnt`。字符集为 GB2312 一级+二级（~6,763 个汉字）+ 日语（假名 + JIS X 0208 汉字/符号）。按需加载到 PSRAM，同一时间最多一个。

#### Scenario: UI 字体文件存在
- **WHEN** 查看 `fonts_data/` 目录
- **THEN** SHALL 存在 `ui_font_16.pfnt`、`ui_font_24.pfnt`

#### Scenario: 阅读字体文件存在
- **WHEN** 查看 `fonts_data/` 目录
- **THEN** SHALL 存在 `reading_24.pfnt`、`reading_32.pfnt`

#### Scenario: 文件名前缀区分类型
- **WHEN** 文件名以 `ui_font_` 开头
- **THEN** SHALL 被识别为 UI 字体，boot 时常驻加载

### Requirement: 字号选择 API
系统 SHALL 提供 `ui_font.h` 头文件，声明初始化、字号选择和字体列表查询函数。所有字体均从 LittleFS .pfnt 文件加载，对调用方透明。

#### Scenario: 初始化
- **WHEN** 调用 `ui_font_init()`
- **THEN** SHALL 挂载 LittleFS，扫描所有 .pfnt 文件，并将 UI 字体（`ui_font_*`）常驻加载到 PSRAM

#### Scenario: 获取 UI 字号
- **WHEN** 调用 `ui_font_get(16)` 或 `ui_font_get(24)`
- **THEN** SHALL 返回 boot 时常驻加载的 UI 字体指针（PSRAM）

#### Scenario: 获取阅读字号
- **WHEN** 调用 `ui_font_get(32)` 且 LittleFS 中存在 `reading_32.pfnt`
- **THEN** SHALL 返回指向 PSRAM 中动态加载字体的指针

#### Scenario: 超出范围
- **WHEN** 调用 `ui_font_get(10)` 或 `ui_font_get(100)`
- **THEN** SHALL 返回最近匹配的可用字体

#### Scenario: 列出可用阅读字体
- **WHEN** 调用 `ui_font_list_available(sizes, max)`
- **THEN** SHALL 返回 LittleFS 中所有阅读字体（非 `ui_font_*` 前缀）的字号列表

### Requirement: 字符集覆盖
UI 字体 SHALL 覆盖 ASCII 可打印字符（0x20-0x7E）和 GB2312 一级常用字（~3,755 个汉字）及常用中日文标点。阅读字体 SHALL 覆盖 GB2312 一级+二级汉字（~6,763 个）及日语字符（平假名、片假名、JIS X 0208 汉字/符号）。

#### Scenario: UI 字体渲染常用汉字
- **WHEN** 使用 UI 字体渲染 GB2312 一级常用字中的任意字符
- **THEN** `epd_get_glyph(font, codepoint)` SHALL 返回非 NULL

#### Scenario: 阅读字体渲染二级汉字
- **WHEN** 使用阅读字体渲染 GB2312 二级汉字中的任意字符
- **THEN** `epd_get_glyph(font, codepoint)` SHALL 返回非 NULL

#### Scenario: 阅读字体渲染日文
- **WHEN** 使用阅读字体渲染平假名、片假名或 JIS X 0208 汉字
- **THEN** `epd_get_glyph(font, codepoint)` SHALL 返回非 NULL

### Requirement: 字体生成工具
项目 SHALL 在 `tools/fontconvert.py` 提供字体转换脚本，支持输出 C header 和 .pfnt 二进制两种格式，支持 `custom`/`gb2312-1`/`gb2312`/`gb2312+ja`/`gbk`/`gb18030` 字符集选项。`tools/generate_fonts.sh` 接受两个参数（UI 字体文件、阅读字体文件），统一输出 .pfnt 到 `fonts_data/`。

#### Scenario: 生成 UI 字体 pfnt
- **WHEN** 运行 `generate_fonts.sh <ui-font> <reading-font>`
- **THEN** SHALL 在 `fonts_data/` 生成 `ui_font_16.pfnt`、`ui_font_24.pfnt`

#### Scenario: 生成阅读字体 pfnt
- **WHEN** 运行 `generate_fonts.sh <ui-font> <reading-font>`
- **THEN** SHALL 在 `fonts_data/` 生成 `reading_24.pfnt`、`reading_32.pfnt`
