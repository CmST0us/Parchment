## ADDED Requirements

### Requirement: fontconvert.py pfnt 输出模式
`tools/fontconvert.py` SHALL 支持 `--output-format pfnt` 参数，输出 .pfnt 二进制文件到 stdout。默认值为 `header`（保持现有行为）。

#### Scenario: 生成 pfnt 文件
- **WHEN** 运行 `python tools/fontconvert.py noto_cjk_24 24 NotoSansCJKsc.otf --compress --output-format pfnt --charset gb18030 > noto_cjk_24.pfnt`
- **THEN** SHALL 输出合法的 .pfnt 二进制文件，包含 GB18030 字符集的 24px 字体数据

#### Scenario: 默认输出格式不变
- **WHEN** 运行不带 `--output-format` 参数的命令
- **THEN** SHALL 输出 C header 格式（与现有行为一致）

### Requirement: fontconvert.py charset 参数
`tools/fontconvert.py` SHALL 支持 `--charset` 参数，可选值为 `custom`（默认，使用 `--string` 参数）、`gb2312-1`（GB2312 一级常用字 + ASCII）、`gbk`、`gb18030`。

#### Scenario: gb2312-1 字符集
- **WHEN** 使用 `--charset gb2312-1`
- **THEN** SHALL 包含 ASCII 可打印字符（0x20-0x7E）和 GB2312 一级常用字（~3,755 个汉字）以及常用中日文标点

#### Scenario: gb18030 字符集
- **WHEN** 使用 `--charset gb18030`
- **THEN** SHALL 包含 GB18030 强制集中 BMP 范围内的全部 CJK 字符（~27,564 个）、ASCII、日文假名、CJK 标点

#### Scenario: custom 字符集（向后兼容）
- **WHEN** 使用 `--charset custom --string "abc你好"`
- **THEN** SHALL 仅包含指定字符串中的字符（现有行为）

### Requirement: generate_fonts.sh 双轨生成
`tools/generate_fonts.sh` SHALL 重写为支持两种生成模式：UI 嵌入字体（.h）和阅读字体（.pfnt）。

#### Scenario: 生成 UI 字体
- **WHEN** 运行 `./tools/generate_fonts.sh <otf-path>`
- **THEN** SHALL 在 `components/ui_core/fonts/` 生成 `ui_font_20.h` 和 `ui_font_28.h`，字符集为 ASCII + GB2312 一级常用字

#### Scenario: 生成阅读字体
- **WHEN** 运行 `./tools/generate_fonts.sh <otf-path>`
- **THEN** SHALL 在 `fonts_data/` 目录生成 `noto_cjk_24.pfnt` 和 `noto_cjk_32.pfnt`，字符集为 GB18030

#### Scenario: 输出目录自动创建
- **WHEN** `fonts_data/` 或 `components/ui_core/fonts/` 目录不存在
- **THEN** SHALL 自动创建

### Requirement: CMake LittleFS 镜像自动打包
构建系统 SHALL 在 `idf.py build` 过程中自动将 `fonts_data/` 目录打包为 LittleFS 分区镜像。

#### Scenario: build 生成镜像
- **WHEN** 执行 `idf.py build`
- **THEN** 构建输出 SHALL 包含 fonts 分区的 LittleFS 镜像文件

#### Scenario: flash 烧录镜像
- **WHEN** 执行 `idf.py flash`
- **THEN** LittleFS 镜像 SHALL 被写入 Flash 的 fonts 分区（0x410000）

#### Scenario: fonts_data 目录为空
- **WHEN** `fonts_data/` 目录为空或不存在
- **THEN** 构建 SHALL 成功，生成空的 LittleFS 镜像（不含字体文件）
