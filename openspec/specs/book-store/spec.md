# book-store Specification

## Purpose

SD 卡 TXT 文件扫描、书籍列表构建与排序。提供 `book_store` API 供 UI 层查询可用书籍。

## Requirements

### Requirement: 书籍扫描

系统 SHALL 扫描 SD 卡根目录下的 `.txt` 文件，构建书籍列表。

#### Scenario: SD 卡已挂载且有 TXT 文件

- **WHEN** 调用 `book_store_scan()` 且 SD 卡已挂载
- **THEN** SHALL 返回 `ESP_OK`
- **AND** 书籍列表 SHALL 包含 `/sdcard` 根目录下所有 `.txt` 扩展名的文件
- **AND** SHALL 跳过以 `.` 开头的隐藏文件
- **AND** 每个书籍条目 SHALL 包含：文件名、完整路径、文件大小（字节）

#### Scenario: SD 卡已挂载但无 TXT 文件

- **WHEN** 调用 `book_store_scan()` 且 SD 卡根目录下无 `.txt` 文件
- **THEN** SHALL 返回 `ESP_OK`
- **AND** 书籍数量 SHALL 为 0

#### Scenario: SD 卡未挂载

- **WHEN** 调用 `book_store_scan()` 且 SD 卡未挂载
- **THEN** SHALL 返回 `ESP_ERR_INVALID_STATE`
- **AND** 书籍数量 SHALL 为 0

---

### Requirement: 书籍列表访问

系统 SHALL 提供 API 获取扫描结果中的书籍数量和单本书籍信息。

#### Scenario: 获取书籍数量

- **WHEN** 扫描完成后调用 `book_store_get_count()`
- **THEN** SHALL 返回当前列表中的书籍总数

#### Scenario: 按索引获取书籍信息

- **WHEN** 调用 `book_store_get_book(index)` 且 index 在有效范围内
- **THEN** SHALL 返回对应书籍的信息指针
- **AND** 指针在下次 `book_store_scan()` 调用前保持有效

#### Scenario: 索引越界

- **WHEN** 调用 `book_store_get_book(index)` 且 index 超出范围
- **THEN** SHALL 返回 `NULL`

---

### Requirement: 书籍列表排序

系统 SHALL 支持对书籍列表按不同方式排序。

#### Scenario: 按文件名排序

- **WHEN** 调用 `book_store_sort(BOOK_SORT_NAME)`
- **THEN** 书籍列表 SHALL 按文件名字母顺序升序排列

#### Scenario: 按文件大小排序

- **WHEN** 调用 `book_store_sort(BOOK_SORT_SIZE)`
- **THEN** 书籍列表 SHALL 按文件大小降序排列

---

### Requirement: 书籍列表容量限制

系统 SHALL 限制最大可扫描的书籍数量，防止内存溢出。

#### Scenario: 文件数量超过上限

- **WHEN** SD 卡根目录 TXT 文件数量超过 `BOOK_STORE_MAX_BOOKS`（默认 64）
- **THEN** SHALL 只保留前 `BOOK_STORE_MAX_BOOKS` 个文件
- **AND** `book_store_get_count()` SHALL 返回 `BOOK_STORE_MAX_BOOKS`
