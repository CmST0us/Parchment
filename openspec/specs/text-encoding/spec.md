## ADDED Requirements

### Requirement: 编码检测
`text_encoding_detect()` SHALL 接收字节缓冲区及其长度，返回检测到的编码类型 `text_encoding_t`。检测逻辑 SHALL 按以下优先级执行：
1. 若前 3 字节为 `EF BB BF`，返回 `TEXT_ENCODING_UTF8_BOM`
2. 若全部字节构成合法 UTF-8 序列（含纯 ASCII），返回 `TEXT_ENCODING_UTF8`
3. 否则返回 `TEXT_ENCODING_GBK`

#### Scenario: 检测 UTF-8 BOM 文件
- **WHEN** 缓冲区前 3 字节为 `EF BB BF`，后续内容为合法 UTF-8
- **THEN** 返回 `TEXT_ENCODING_UTF8_BOM`

#### Scenario: 检测纯 UTF-8 文件
- **WHEN** 缓冲区为合法 UTF-8 编码（无 BOM），包含中文字符
- **THEN** 返回 `TEXT_ENCODING_UTF8`

#### Scenario: 检测纯 ASCII 文件
- **WHEN** 缓冲区全部为 0x00-0x7F 范围内的 ASCII 字节
- **THEN** 返回 `TEXT_ENCODING_UTF8`（ASCII 是 UTF-8 子集）

#### Scenario: 检测 GBK 文件
- **WHEN** 缓冲区包含非 ASCII 高字节且不构成合法 UTF-8 序列
- **THEN** 返回 `TEXT_ENCODING_GBK`

#### Scenario: 空缓冲区
- **WHEN** 缓冲区长度为 0
- **THEN** 返回 `TEXT_ENCODING_UTF8`

### Requirement: UTF-8 有效性校验
编码检测中的 UTF-8 校验 SHALL 严格检查多字节序列：
- 2 字节序列：首字节 `C2-DF`，后跟 1 个 continuation byte (`80-BF`)
- 3 字节序列：首字节 `E0-EF`，后跟 2 个 continuation bytes
- 4 字节序列：首字节 `F0-F4`，后跟 3 个 continuation bytes
- `C0-C1` 和 `F5-FF` 范围的首字节 SHALL 判定为非法 UTF-8

#### Scenario: overlong 序列被拒绝
- **WHEN** 缓冲区包含 `C0 80`（overlong 编码的 NUL）
- **THEN** 该序列不被识别为合法 UTF-8，编码判定为 GBK

#### Scenario: 合法 3 字节中文字符
- **WHEN** 缓冲区包含 `E4 BD A0`（"你" 的 UTF-8 编码）
- **THEN** 该序列被识别为合法 UTF-8

### Requirement: GBK 到 UTF-8 转码
`text_encoding_gbk_to_utf8()` SHALL 将 GBK 编码的缓冲区转换为 UTF-8。转码规则：
- ASCII 字节（0x00-0x7F）直接输出
- GBK 双字节序列（首字节 0x81-0xFE，次字节 0x40-0xFE 跳过 0x7F）通过映射表转为 Unicode codepoint，再编码为 UTF-8
- 无法映射的字节序列 SHALL 输出 UTF-8 替换字符 U+FFFD (`EF BF BD`)
- 函数 SHALL 返回 `ESP_OK` 表示成功，`ESP_FAIL` 表示参数错误

#### Scenario: GBK 中文转码
- **WHEN** 输入 GBK 编码的 `C4 E3 BA C3`（"你好"）
- **THEN** 输出 UTF-8 编码 `E4 BD A0 E5 A5 BD`，返回 `ESP_OK`

#### Scenario: GBK 混合 ASCII 转码
- **WHEN** 输入 `48 65 6C 6C 6F C4 E3`（"Hello你"）
- **THEN** ASCII 部分直接输出，GBK 部分转为 UTF-8

#### Scenario: 无效 GBK 序列
- **WHEN** 输入包含无法映射的双字节序列
- **THEN** 该序列被替换为 U+FFFD，不中断转码，返回 `ESP_OK`

#### Scenario: 目标缓冲区不足
- **WHEN** dst_len 传入的容量不足以容纳转码结果
- **THEN** 转码截断到可容纳的最后一个完整 UTF-8 字符，dst_len 写入实际输出长度，返回 `ESP_OK`

### Requirement: GBK 映射表
组件 SHALL 包含编译期生成的 GBK→Unicode 映射表，覆盖 GBK 全部双字节区域（首字节 0x81-0xFE，次字节 0x40-0xFE 跳过 0x7F）。映射表 SHALL 声明为 `const` 存储于 flash，不占用 RAM。

#### Scenario: 映射表覆盖 GB2312 一级汉字
- **WHEN** 查询 GBK 码 `B0 A1`（"啊"）
- **THEN** 映射表返回 Unicode codepoint U+554A

#### Scenario: 映射表覆盖 GBK 扩展汉字
- **WHEN** 查询 GBK 扩展区域的双字节码
- **THEN** 映射表返回对应的 Unicode codepoint

### Requirement: 编码类型枚举
组件 SHALL 定义 `text_encoding_t` 枚举，包含以下值：
- `TEXT_ENCODING_UTF8`: 合法 UTF-8（含纯 ASCII）
- `TEXT_ENCODING_UTF8_BOM`: 带 BOM 的 UTF-8
- `TEXT_ENCODING_GBK`: GBK/GB2312 编码
- `TEXT_ENCODING_UNKNOWN`: 无法识别的编码

#### Scenario: 枚举值可用于 switch
- **WHEN** 调用 `text_encoding_detect()` 获取返回值
- **THEN** 返回值为 `text_encoding_t` 枚举成员，可用于 switch-case 分支
