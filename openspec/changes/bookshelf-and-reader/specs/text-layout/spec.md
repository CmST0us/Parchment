## ADDED Requirements

### Requirement: UTF-8 文本解析
`text_layout` SHALL 正确解析 UTF-8 编码的文本，提取 Unicode 码点序列。

#### Scenario: ASCII 文本
- **WHEN** 输入纯 ASCII 文本 "Hello"
- **THEN** 解析出 5 个码点：U+0048, U+0065, U+006C, U+006C, U+006F

#### Scenario: 中文文本
- **WHEN** 输入中文 UTF-8 文本 "你好"
- **THEN** 解析出 2 个码点：U+4F60, U+597D

#### Scenario: 中英混排
- **WHEN** 输入 "Hello你好World"
- **THEN** 正确解析所有 10 个码点

### Requirement: 自动换行
`text_layout` SHALL 在给定宽度内自动换行，遵循中英文换行规则。

#### Scenario: 中文逐字换行
- **WHEN** 当前行剩余宽度不足以放下一个中文字符
- **THEN** 在该字符前换行，该字符成为下一行首字符

#### Scenario: 英文按词换行
- **WHEN** 英文单词无法完整放入当前行剩余宽度
- **THEN** 整个单词移到下一行（在前一个空格或连字符处断行）

#### Scenario: 超长英文单词
- **WHEN** 单个英文单词宽度超过整行宽度
- **THEN** 在行尾强制断字

#### Scenario: 换行符处理
- **WHEN** 文本包含 `\n` 字符
- **THEN** 在该位置强制换行

### Requirement: 排版参数配置
`text_layout` SHALL 支持配置字号、行距和页面边距。

#### Scenario: 默认排版参数
- **WHEN** 未显式设置排版参数
- **THEN** 使用默认值：字号 24px，行距 1.5 倍字号，上下左右边距各 30px

#### Scenario: 自定义参数
- **WHEN** 设置字号为 20px、行距为 1.8 倍、边距为 40px
- **THEN** 后续排版使用新参数

### Requirement: 文本渲染到 framebuffer
`text_layout` SHALL 将排版后的文本行渲染到 ui_core framebuffer。

#### Scenario: 单页渲染
- **WHEN** 提供文本内容、起始位置和 framebuffer 指针
- **THEN** 按排版规则将文本渲染到 framebuffer 对应区域，使用 4bpp 灰度

#### Scenario: 渲染区域限制
- **WHEN** 文本超出指定渲染区域（由边距定义的内容区）
- **THEN** 停止渲染，返回已渲染的最后一个字符在原文中的偏移量

### Requirement: 计算页面文本容量
`text_layout` SHALL 能计算给定排版参数下，一页能容纳多少文本。

#### Scenario: 页面容量计算
- **WHEN** 提供一段文本和排版参数
- **THEN** 返回该页能显示到原文中的字节偏移量（即下一页的起始偏移）
