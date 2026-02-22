## ADDED Requirements

### Requirement: TextSource component 结构
`text_source` SHALL 是一个独立的 ESP-IDF C++17 component，位于 `components/text_source/`。公开头文件 `include/text_source/TextSource.h`，命名空间 `ink::`。REQUIRES `text_encoding` 和 `sd_storage`。不依赖 `ink_ui`。

#### Scenario: component 注册
- **WHEN** `idf.py build` 编译项目
- **THEN** `text_source` component 正确编译，导出 `ink::TextSource` 类

### Requirement: TextSource 生命周期
`TextSource` SHALL 支持以下生命周期：
- 默认构造为 `Closed` 状态
- `open(filePath, cacheDirPath)` 打开文件，快速返回（< 100ms），只加载首块
- `close()` 释放所有资源（PSRAM 缓冲区、文件句柄、停止后台 task）
- 析构函数 SHALL 自动调用 `close()`
- 不可拷贝，不可移动

#### Scenario: 打开 UTF-8 文件
- **WHEN** 调用 `open("/sdcard/book/test.txt", "/sdcard/.cache/abc123")` 且文件为 UTF-8
- **THEN** 状态从 `Closed` 变为 `Available`，首块文本可通过 `read()` 访问

#### Scenario: 打开不存在的文件
- **WHEN** 调用 `open("/sdcard/book/nofile.txt", "/sdcard/.cache/abc123")`
- **THEN** 返回 `false`，状态为 `Error`

#### Scenario: 关闭后资源释放
- **WHEN** 调用 `close()` 后
- **THEN** PSRAM 缓冲区已释放，文件句柄已关闭，后台 task 已停止，状态为 `Closed`

### Requirement: TextSource 状态机
TextSource SHALL 维护以下状态（`TextSourceState` 枚举）：
- `Closed` — 未打开
- `Preparing` — 正在检测编码、加载首块
- `Available` — 首块已就绪，可顺序阅读
- `Converting` — GBK 后台转换进行中（首块已可读）
- `Ready` — 全部文本可访问（UTF-8 文件直接进入此状态）
- `Error` — 打开失败

状态转换：
- `Closed` → `open()` → `Preparing` → `Available`（UTF-8）或 `Converting`（GBK）
- `Converting` → 后台转换完成 → `Ready`
- `Available` → 即刻 → `Ready`（UTF-8 文件）
- 任意状态 → `close()` → `Closed`

#### Scenario: UTF-8 文件状态流转
- **WHEN** 打开一个 UTF-8 文件
- **THEN** 状态经历 `Closed → Preparing → Ready`，不经过 `Converting`

#### Scenario: GBK 文件状态流转
- **WHEN** 打开一个 GBK 文件
- **THEN** 状态经历 `Closed → Preparing → Converting → Ready`

#### Scenario: 查询状态
- **WHEN** 调用 `state()` 方法
- **THEN** 返回当前的 `TextSourceState` 枚举值

### Requirement: TextSource 文本读取
TextSource SHALL 提供 `TextSpan read(uint32_t offset)` 方法：
- 返回 `TextSpan{data, length}`，其中 `data` 指向内部 PSRAM 缓冲区的 UTF-8 文本
- `length` 为从该 offset 起可用的连续字节数
- 指针在下一次 `read()` 调用或 `close()` 前有效
- offset 超出当前可用范围时返回 `{nullptr, 0}`

内部实现 SHALL 使用 512KB 滑动窗口（TextWindow），存储在 PSRAM（`MALLOC_CAP_SPIRAM`）中。当 offset 不在当前窗口范围内时，重新加载窗口使 offset 位于窗口中心附近。

窗口加载时 SHALL 将起始位置对齐到 UTF-8 字符边界（跳过 continuation bytes `10xxxxxx`）。

#### Scenario: 读取窗口内的文本
- **WHEN** 窗口覆盖 offset 0-524287，调用 `read(1000)`
- **THEN** 返回 `{data, 523287}` 指向 offset 1000 处的文本，无需磁盘 I/O

#### Scenario: 读取窗口外的文本触发重新加载
- **WHEN** 窗口覆盖 offset 0-524287，调用 `read(600000)`
- **THEN** 窗口重新加载为以 600000 为中心的 512KB 区间，返回有效 TextSpan

#### Scenario: 读取超出可用范围
- **WHEN** GBK 转换只完成了 200KB，调用 `read(300000)`
- **THEN** 返回 `{nullptr, 0}`

### Requirement: TextSource 大小和进度查询
TextSource SHALL 提供以下查询方法：
- `totalSize()` — UTF-8 文本总大小（字节）。GBK 转换未完成时返回 0。
- `availableSize()` — 当前可访问的 UTF-8 字节范围上限。`Ready` 状态下等于 `totalSize()`。
- `progress()` — 后台准备进度 `[0.0, 1.0]`。UTF-8 文件 open 后立即 1.0。GBK 文件为转换字节进度。
- `detectedEncoding()` — 检测到的原始编码（`text_encoding_t`）
- `originalFileSize()` — 原始文件大小（字节）

#### Scenario: UTF-8 文件大小查询
- **WHEN** 打开一个 2MB UTF-8 文件后调用 `totalSize()`
- **THEN** 返回 2097152（文件大小）

#### Scenario: GBK 转换中查询进度
- **WHEN** GBK 转换进度 50%，调用 `progress()`
- **THEN** 返回 0.5

#### Scenario: GBK 转换完成后查询总大小
- **WHEN** GBK 转换完成后调用 `totalSize()`
- **THEN** 返回 text.utf8 缓存文件的大小

### Requirement: TextSource GBK 后台转换
当检测到 GBK 编码时，TextSource SHALL：
1. 内存中转换首块（128KB）以便立即显示
2. 创建缓存目录（如不存在）
3. 启动 FreeRTOS task 流式转换整个文件到 `<cacheDirPath>/text.utf8`
4. 每次转换 64KB 块，写入输出文件
5. 转换完成后写入完成标记（文件末尾追加 4 字节 magic `"DONE"`）
6. 切换内部读取源从内存首块到缓存文件
7. 状态从 `Converting` 变为 `Ready`

后台 task SHALL 使用约 8KB 栈空间，优先级低于主循环（`tskIDLE_PRIORITY + 2`）。

#### Scenario: GBK 文件首次打开
- **WHEN** 打开一个 3MB GBK 文件，无缓存
- **THEN** 首块在 ~60ms 内可读，后台 task 开始转换，`/sdcard/.cache/<hash>/text.utf8` 逐步写入

#### Scenario: GBK 文件再次打开（有缓存）
- **WHEN** 打开一个 GBK 文件，`text.utf8` 缓存已存在且完成标记有效
- **THEN** 直接使用缓存文件，状态跳过 `Converting` 直接进入 `Ready`

#### Scenario: 不完整缓存文件处理
- **WHEN** 打开文件时发现 `text.utf8` 存在但缺少完成标记
- **THEN** 删除不完整文件，重新启动后台转换

### Requirement: TextSource 线程安全
TextSource SHALL 使用内部 mutex 保护以下共享状态：
- `state_` 状态变量
- `availableSize_` 可用大小
- 文件句柄切换（从内存首块到缓存文件）

`read()` 方法 SHALL 是线程安全的，可从主线程和后台 task 同时调用。

#### Scenario: 主线程读取同时后台转换
- **WHEN** 主线程调用 `read(0)` 同时后台 task 正在更新 `availableSize_`
- **THEN** 不产生数据竞争，两者均正确完成
