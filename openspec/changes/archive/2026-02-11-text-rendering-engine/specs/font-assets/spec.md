## ADDED Requirements

### Requirement: 预编译字体资源
系统 SHALL 提供 Noto Sans CJK SC 字体的预编译资源，以 epdiy `EpdFont` 格式存储在 `components/ui_core/fonts/` 目录下的 C header 文件中。每个字号一个 `.h` 文件。

#### Scenario: 字体文件存在
- **WHEN** 构建项目
- **THEN** SHALL 存在以下字体头文件：`noto_sans_cjk_20.h`、`noto_sans_cjk_24.h`、`noto_sans_cjk_28.h`、`noto_sans_cjk_32.h`、`noto_sans_cjk_36.h`、`noto_sans_cjk_40.h`

#### Scenario: 字体数据结构
- **WHEN** 包含任一字体头文件
- **THEN** SHALL 导出一个 `const EpdFont` 类型的全局变量，变量名与文件名对应（如 `noto_sans_cjk_20`）

### Requirement: 字号选择 API
系统 SHALL 提供 `ui_font.h` 头文件，声明所有可用字体的 `extern` 引用，并提供按像素大小获取最近匹配字号的函数。

#### Scenario: 精确匹配
- **WHEN** 调用 `ui_font_get(24)`
- **THEN** SHALL 返回指向 `noto_sans_cjk_24` 的指针

#### Scenario: 非精确匹配
- **WHEN** 调用 `ui_font_get(22)`
- **THEN** SHALL 返回最接近的可用字号（向下取整到 20）

#### Scenario: 超出范围
- **WHEN** 调用 `ui_font_get(10)` 或 `ui_font_get(100)`
- **THEN** SHALL 返回最小（20）或最大（40）可用字号

### Requirement: 字符集覆盖
预编译字体 SHALL 包含渲染测试文本（中日英混排歌词）所需的全部字符。字符集通过 `fontconvert.py --string` 参数指定，包含 ASCII 字母、CJK 汉字、平假名、片假名、CJK 标点及全角标点。

#### Scenario: 所有测试文本字符可渲染
- **WHEN** 遍历测试歌词文本的每个 Unicode code point
- **THEN** `epd_get_glyph(font, codepoint)` 对每个字符 SHALL 返回非 NULL

### Requirement: 字体生成工具
项目 SHALL 在 `tools/fontconvert.py` 提供修改版的字体转换脚本，DPI 设为 72（使 size 参数直接对应像素高度）。

#### Scenario: 生成 20px 字体
- **WHEN** 运行 `python tools/fontconvert.py noto_sans_cjk_20 20 NotoSansCJKsc-Regular.otf --compress --string "..."`
- **THEN** SHALL 输出合法的 C header，其中 `EpdFont` 的 `advance_y` 值 SHALL 接近 20 像素
