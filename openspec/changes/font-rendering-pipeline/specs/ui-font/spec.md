## ADDED Requirements

### Requirement: 字体加载
系统 SHALL 提供 `ui_font_load(const char *path)` 函数，从 SD 卡加载 .bin 格式的 binary font 文件。加载过程 SHALL 解析 134 字节 header 和 glyph table，并在 PSRAM 中构建 glyph 索引（hash map）。

#### Scenario: 正常加载字体
- **WHEN** 调用 `ui_font_load("/sdcard/fonts/noto_serif_24.bin")`，且文件存在且格式正确
- **THEN** 函数 SHALL 返回 `ESP_OK`
- **AND** 字体的 glyph 索引 SHALL 被构建在 PSRAM 中

#### Scenario: 文件不存在
- **WHEN** 调用 `ui_font_load()` 且指定路径不存在
- **THEN** 函数 SHALL 返回 `ESP_ERR_NOT_FOUND`
- **AND** SHALL 记录错误日志

#### Scenario: 文件格式无效
- **WHEN** .bin 文件 header 中 version 不为 2，或 char_count 为 0，或 font_height 不在 [20, 50] 范围内
- **THEN** 函数 SHALL 返回 `ESP_ERR_INVALID_ARG`
- **AND** SHALL 记录错误日志

#### Scenario: 重复加载
- **WHEN** 已有字体加载的状态下再次调用 `ui_font_load()`
- **THEN** SHALL 先卸载当前字体再加载新字体

### Requirement: 字体卸载
系统 SHALL 提供 `ui_font_unload(void)` 函数，释放当前加载的字体资源（glyph 索引内存、关闭文件句柄）。

#### Scenario: 正常卸载
- **WHEN** 调用 `ui_font_unload()` 且有字体已加载
- **THEN** glyph 索引内存 SHALL 被释放
- **AND** .bin 文件句柄 SHALL 被关闭

#### Scenario: 未加载时卸载
- **WHEN** 调用 `ui_font_unload()` 且没有字体加载
- **THEN** SHALL 安全返回，不产生错误

### Requirement: 渲染单个字符
系统 SHALL 提供 `ui_font_draw_char(uint8_t *fb, int x, int y, uint32_t codepoint, uint8_t color)` 函数，在 framebuffer 上渲染单个字符。函数 SHALL 返回该字符的 advance width（步进宽度）。

#### Scenario: 渲染 ASCII 字符
- **WHEN** 调用 `ui_font_draw_char(fb, 100, 200, 'A', 0x00)`
- **THEN** 字符 'A' 的 bitmap SHALL 被渲染到 framebuffer 的 (100+x_offset, 200+y_offset) 位置
- **AND** 函数 SHALL 返回字符 'A' 的 advance width

#### Scenario: 渲染中文字符
- **WHEN** 调用 `ui_font_draw_char(fb, 100, 200, 0x4F60, 0x00)`（"你"的 Unicode）
- **THEN** 该汉字的 bitmap SHALL 被渲染到 framebuffer
- **AND** 函数 SHALL 返回该汉字的 advance width

#### Scenario: 字体中不存在的字符
- **WHEN** 请求渲染的 codepoint 在当前字体中不存在
- **THEN** SHALL 渲染一个替代标记（矩形框）
- **AND** SHALL 返回字体高度的一半作为 advance width

#### Scenario: 未加载字体时渲染
- **WHEN** 未加载任何字体时调用 `ui_font_draw_char()`
- **THEN** SHALL 返回 0 且不进行渲染

### Requirement: 渲染文本字符串
系统 SHALL 提供 `ui_font_draw_text(uint8_t *fb, int x, int y, int max_w, int line_height, const char *utf8_text, uint8_t color)` 函数，渲染 UTF-8 编码的文本字符串，支持自动换行。函数 SHALL 返回渲染所占用的总高度（像素）。

#### Scenario: 单行英文文本
- **WHEN** 调用 `ui_font_draw_text(fb, 24, 100, 492, 36, "Hello World", 0x00)` 且文本宽度未超过 max_w
- **THEN** 文本 SHALL 在 (24, 100) 位置单行渲染
- **AND** 返回值 SHALL 等于一个 line_height

#### Scenario: 自动换行
- **WHEN** 文本宽度超过 max_w
- **THEN** 系统 SHALL 在合适的位置换行
- **AND** 新行 SHALL 从 (x, y + line_height) 开始渲染

#### Scenario: CJK 字符换行
- **WHEN** 当前行剩余宽度不足以放下下一个 CJK 字符
- **THEN** SHALL 在该字符前换行（CJK 字符 SHALL 逐字可断行）

#### Scenario: ASCII 单词换行
- **WHEN** 当前行剩余宽度不足以放下当前 ASCII 单词
- **THEN** SHALL 将整个单词移到下一行（不拆分单词）
- **AND** 如果单个单词宽度超过 max_w，SHALL 强制在行尾断开

#### Scenario: 中英文混排
- **WHEN** 文本包含中文和英文混合内容
- **THEN** SHALL 正确渲染所有字符，CJK 字符和 ASCII 字符的间距由各自的 advance width 决定

### Requirement: 文本宽度测量
系统 SHALL 提供 `ui_font_measure_text(const char *utf8_text, int max_chars)` 函数，计算文本渲染后的像素宽度，不执行实际渲染。

#### Scenario: 测量短文本
- **WHEN** 调用 `ui_font_measure_text("Hello", 0)`（max_chars=0 表示不限制）
- **THEN** SHALL 返回 "Hello" 所有字符 advance width 之和

#### Scenario: 限制字符数
- **WHEN** 调用 `ui_font_measure_text("Hello World", 5)`
- **THEN** SHALL 仅测量前 5 个字符的宽度

### Requirement: 获取字体信息
系统 SHALL 提供 `ui_font_get_height(void)` 函数，返回当前加载字体的高度（像素）。

#### Scenario: 获取字体高度
- **WHEN** 已加载一个 font_height=24 的字体
- **THEN** `ui_font_get_height()` SHALL 返回 24

#### Scenario: 未加载字体
- **WHEN** 未加载字体时调用 `ui_font_get_height()`
- **THEN** SHALL 返回 0

### Requirement: UTF-8 解码
字体模块 SHALL 内部实现 UTF-8 解码，支持 Unicode BMP 范围 (U+0000~U+FFFF)。

#### Scenario: ASCII 字符
- **WHEN** 输入单字节 UTF-8 序列 (0x00~0x7F)
- **THEN** SHALL 正确解码为对应 Unicode codepoint

#### Scenario: 中文字符
- **WHEN** 输入三字节 UTF-8 序列（如 "你" = 0xE4 0xBD 0xA0）
- **THEN** SHALL 正确解码为 U+4F60

#### Scenario: 无效 UTF-8 序列
- **WHEN** 遇到无效的 UTF-8 字节序列
- **THEN** SHALL 跳过无效字节并继续解码后续内容

### Requirement: Glyph 索引结构
系统 SHALL 将 glyph 元数据存储在 PSRAM 分配的 hash map 中，以 Unicode codepoint 为 key 实现 O(1) 查找。

#### Scenario: 查找已有字符
- **WHEN** 查找一个存在于字体中的 Unicode codepoint
- **THEN** SHALL 返回该字符的元数据（advance_w, bitmap_w, bitmap_h, x_offset, y_offset, data_offset, data_size）

#### Scenario: 查找不存在的字符
- **WHEN** 查找一个不存在于字体中的 Unicode codepoint
- **THEN** SHALL 返回 NULL

#### Scenario: 内存分配
- **WHEN** 构建 glyph 索引
- **THEN** 索引数据 SHALL 分配在 PSRAM（使用 `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`）
