## Context

Parchment 阅读器需要数据持久化层来支撑书库界面和阅读功能。当前已有 `sd_storage` 组件提供 SD 卡 FAT 文件系统挂载（`/sdcard`），以及 ESP-IDF 内置的 NVS (Non-Volatile Storage) flash KV 存储。需要在此基础上构建两个上层组件：书籍管理和设置存储。

硬件约束：8MB PSRAM、16MB Flash、SD 卡容量可变（通常 4-32GB）。

## Goals / Non-Goals

**Goals:**
- 提供 `book_store` API：扫描 SD 卡 TXT 文件、返回书籍列表、支持排序
- 提供 `settings_store` API：读写用户阅读偏好、读写每本书阅读进度
- 两个组件均为独立 ESP-IDF component，可被 UI 层直接调用
- API 线程安全（可从多个 task 调用）

**Non-Goals:**
- 不支持 EPUB 解析（后续 Change 11）
- 不支持封面图提取
- 不支持文件系统监控（热插拔检测）
- 不实现缓存层或数据库

## Decisions

### 1. book_store：纯目录扫描，不缓存

**选择**：每次调用 `book_store_scan()` 时直接遍历 SD 卡目录，将结果存入调用方提供的数组。

**替代方案**：在 NVS 中缓存书籍列表 → 增加复杂度，SD 卡可能被外部修改导致不一致。

**理由**：TXT 文件扫描很快（只读目录条目，不读文件内容）。SD 卡可能在关机期间被用户修改，每次启动重新扫描最可靠。

### 2. book_store：扫描 `/sdcard` 根目录，不递归

**选择**：只扫描根目录的 `.txt` 文件。

**理由**：简化实现，避免深层目录遍历的栈开销。用户使用模式是将 TXT 文件放在 SD 卡根目录。后续可扩展为递归扫描。

### 3. settings_store：NVS namespace 分区

**选择**：使用两个 NVS namespace：
- `"settings"` — 全局阅读偏好（字号、行距、页边距等）
- `"progress"` — 阅读进度，key 为文件路径的哈希（MD5 前 15 字节 hex 截断为 NVS key 长度限制）

**替代方案**：单一 namespace → key 冲突风险；用文件名作 key → 可能超过 NVS key 长度限制 (15 bytes)。

**理由**：NVS key 最大 15 字节，文件路径通常很长。对路径做 MD5 hash 后取前 15 hex 字符作为 key，冲突概率极低。两个 namespace 逻辑分离。

### 4. 阅读进度存储格式

**选择**：每本书的进度用单个 NVS blob 存储，包含 `{uint32_t byte_offset, uint32_t total_bytes, uint32_t current_page, uint32_t total_pages}`。

**理由**：TXT 文件用字节偏移定位最直接；页码依赖排版参数（字号、行距），所以同时存储两者。blob 格式紧凑且原子写入。

### 5. 组件目录结构

```
components/book_store/
  CMakeLists.txt
  include/book_store.h
  book_store.c

components/settings_store/
  CMakeLists.txt
  include/settings_store.h
  settings_store.c
```

每个组件声明对应的 ESP-IDF 依赖（`REQUIRES`）。

## Risks / Trade-offs

- **NVS key 哈希冲突** → 概率极低（MD5 15 hex = 60 bits），可接受。如遇冲突，表现为两本书共享进度，影响不大。
- **SD 卡未挂载时 book_store 调用** → `book_store_scan()` 返回 0 本书 + `ESP_ERR_INVALID_STATE`，调用方需处理。
- **NVS flash 写入寿命** → 阅读进度写入频率低（翻页时写入），远低于 NVS flash 的 10 万次擦写限制。
- **并发访问** → 使用 mutex 保护共享状态。NVS API 本身线程安全，但 `book_store` 的扫描结果列表需要保护。
