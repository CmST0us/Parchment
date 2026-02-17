## MODIFIED Requirements

### Requirement: ReaderViewController 文件加载
ReaderViewController SHALL 在 `viewDidLoad()` 中从 SD 卡读取 TXT 文件全部内容到内存（PSRAM），**检测文件编码并转换为 UTF-8**，并从 `settings_store` 加载该书的阅读进度（byte_offset）。

加载流程 SHALL 按以下顺序执行：
1. 以二进制模式读取文件全部内容到 PSRAM 缓冲区
2. 调用 `text_encoding_detect()` 检测编码
3. 若为 `TEXT_ENCODING_GBK`：分配新 PSRAM 缓冲区（容量为原始大小的 1.5 倍 + 1），调用 `text_encoding_gbk_to_utf8()` 转码，释放原缓冲区，替换为新缓冲区
4. 若为 `TEXT_ENCODING_UTF8_BOM`：跳过前 3 字节（memmove），更新缓冲区大小
5. 若为 `TEXT_ENCODING_UTF8`：不做额外处理
6. 空终止缓冲区

#### Scenario: 文件加载成功（UTF-8）
- **WHEN** viewDidLoad 执行，文件 /sdcard/book/三体.txt 存在且为 UTF-8 编码
- **THEN** 文件内容加载到 PSRAM buffer，无需转码，阅读进度恢复到上次位置

#### Scenario: 文件加载成功（GBK 自动转码）
- **WHEN** viewDidLoad 执行，文件为 GBK 编码的中文 TXT
- **THEN** 检测到 GBK 编码，自动转码为 UTF-8 存入 PSRAM buffer，后续分页和渲染正常显示中文

#### Scenario: 文件加载成功（UTF-8 BOM 剥离）
- **WHEN** viewDidLoad 执行，文件以 `EF BB BF` 开头
- **THEN** BOM 被剥离，textBuffer_ 从实际文本内容开始，textSize_ 减少 3

#### Scenario: 文件加载失败
- **WHEN** viewDidLoad 执行，文件不存在或读取错误
- **THEN** 显示错误提示 "Failed to load file"，不崩溃

#### Scenario: GBK 转码内存分配失败
- **WHEN** GBK 文件较大，PSRAM 无法分配转码缓冲区
- **THEN** 释放原缓冲区，loadFile 返回 false，显示错误提示
