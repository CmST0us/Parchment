## Requirements

### Requirement: fontconvert.py pfnt 输出模式
`tools/fontconvert.py` SHALL 支持 `--output-format pfnt` 参数，输出 .pfnt 二进制文件到 stdout。默认值为 `header`（保持现有行为）。

#### Scenario: 生成 pfnt 文件
- **WHEN** 运行 `python tools/fontconvert.py reading_24 24 LXGWWenKai.ttf --compress --output-format pfnt --charset gb2312+ja > reading_24.pfnt`
- **THEN** SHALL 输出合法的 .pfnt 二进制文件

#### Scenario: 默认输出格式不变
- **WHEN** 运行不带 `--output-format` 参数的命令
- **THEN** SHALL 输出 C header 格式（与现有行为一致）

### Requirement: fontconvert.py charset 参数
`tools/fontconvert.py` SHALL 支持 `--charset` 参数，可选值为 `custom`（默认）、`gb2312-1`（一级）、`gb2312`（一级+二级）、`gb2312+ja`（GB2312 + 日语）、`gbk`、`gb18030`。

#### Scenario: gb2312-1 字符集
- **WHEN** 使用 `--charset gb2312-1`
- **THEN** SHALL 包含 ASCII 可打印字符和 GB2312 一级常用字（~3,755 个汉字）及常用标点

#### Scenario: gb2312 字符集
- **WHEN** 使用 `--charset gb2312`
- **THEN** SHALL 包含 GB2312 一级+二级汉字（~6,763 个）

#### Scenario: gb2312+ja 字符集
- **WHEN** 使用 `--charset gb2312+ja`
- **THEN** SHALL 包含 GB2312 一级+二级汉字 + 日语（平假名、片假名、半角片假名、JIS X 0208 汉字/符号、CJK 标点、全角拉丁）

#### Scenario: custom 字符集（向后兼容）
- **WHEN** 使用 `--charset custom --string "abc你好"`
- **THEN** SHALL 仅包含指定字符串中的字符

### Requirement: generate_fonts.sh 统一生成
`tools/generate_fonts.sh` SHALL 接受两个参数（UI 字体文件、阅读字体文件），统一输出 .pfnt 格式到 `fonts_data/`。

#### Scenario: 生成全部字体
- **WHEN** 运行 `./tools/generate_fonts.sh <ui-font.ttf> <reading-font.ttf>`
- **THEN** SHALL 在 `fonts_data/` 生成 `ui_font_16.pfnt`、`ui_font_24.pfnt`、`reading_24.pfnt`、`reading_32.pfnt`

#### Scenario: 输出目录自动创建
- **WHEN** `fonts_data/` 目录不存在
- **THEN** SHALL 自动创建

### Requirement: CMake LittleFS 镜像自动打包
构建系统 SHALL 在 `idf.py build` 过程中自动将 `fonts_data/` 目录打包为 LittleFS 分区镜像。

#### Scenario: build 生成镜像
- **WHEN** 执行 `idf.py build`
- **THEN** 构建输出 SHALL 包含 fonts 分区的 LittleFS 镜像文件（`fonts.bin`）

#### Scenario: flash 烧录镜像
- **WHEN** 执行 `idf.py flash`
- **THEN** LittleFS 镜像 SHALL 被写入 Flash 的 fonts 分区
