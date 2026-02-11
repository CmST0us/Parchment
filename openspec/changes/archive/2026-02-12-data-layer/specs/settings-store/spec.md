## ADDED Requirements

### Requirement: NVS 初始化

系统 SHALL 初始化 NVS flash 存储，供设置和进度数据使用。

#### Scenario: 首次初始化

- **WHEN** 调用 `settings_store_init()`
- **THEN** SHALL 初始化 NVS flash（`nvs_flash_init()`）
- **AND** SHALL 返回 `ESP_OK`

#### Scenario: NVS 分区损坏

- **WHEN** NVS 分区校验失败
- **THEN** SHALL 擦除 NVS 分区并重新初始化
- **AND** SHALL 返回 `ESP_OK`（数据丢失但功能恢复）

---

### Requirement: 阅读偏好存储

系统 SHALL 持久化存储用户阅读偏好设置。

#### Scenario: 保存阅读偏好

- **WHEN** 调用 `settings_store_save_prefs()` 并传入偏好结构体
- **THEN** SHALL 将以下字段写入 NVS `"settings"` namespace：
  - 字号 (`font_size`: uint8, 范围 14-32, 默认 20)
  - 行距 (`line_spacing`: uint8, 存储为 ×10 整数, 范围 10-25, 默认 16)
  - 段间距 (`paragraph_spacing`: uint8, 范围 0-24, 默认 8)
  - 页边距 (`margin`: uint8, 枚举值 16/24/36, 默认 24)
  - 全刷间隔 (`full_refresh_pages`: uint8, 范围 1-20, 默认 5)
- **AND** SHALL 返回 `ESP_OK`

#### Scenario: 加载阅读偏好

- **WHEN** 调用 `settings_store_load_prefs()`
- **THEN** SHALL 从 NVS 读取所有偏好字段
- **AND** 对于不存在的 key SHALL 使用默认值填充
- **AND** SHALL 返回 `ESP_OK`

---

### Requirement: 阅读进度存储

系统 SHALL 持久化存储每本书的阅读进度。

#### Scenario: 保存阅读进度

- **WHEN** 调用 `settings_store_save_progress(file_path, progress)` 并传入书籍路径和进度结构体
- **THEN** SHALL 以文件路径的 MD5 hash 前 15 字符为 key
- **AND** SHALL 将进度数据（byte_offset, total_bytes, current_page, total_pages）作为 blob 写入 NVS `"progress"` namespace
- **AND** SHALL 返回 `ESP_OK`

#### Scenario: 加载阅读进度

- **WHEN** 调用 `settings_store_load_progress(file_path, progress)`
- **THEN** SHALL 从 NVS 读取对应 key 的 blob 数据
- **AND** SHALL 填充进度结构体
- **AND** SHALL 返回 `ESP_OK`

#### Scenario: 加载不存在的进度

- **WHEN** 调用 `settings_store_load_progress()` 且该书籍无已保存进度
- **THEN** SHALL 将进度结构体所有字段置为 0
- **AND** SHALL 返回 `ESP_ERR_NOT_FOUND`

---

### Requirement: 排序偏好存储

系统 SHALL 持久化存储书库排序方式。

#### Scenario: 保存排序偏好

- **WHEN** 调用 `settings_store_save_sort_order(order)`
- **THEN** SHALL 将排序方式写入 NVS `"settings"` namespace
- **AND** SHALL 返回 `ESP_OK`

#### Scenario: 加载排序偏好

- **WHEN** 调用 `settings_store_load_sort_order()`
- **THEN** SHALL 返回已保存的排序方式
- **AND** 若无已保存值 SHALL 返回默认值 `BOOK_SORT_NAME`
