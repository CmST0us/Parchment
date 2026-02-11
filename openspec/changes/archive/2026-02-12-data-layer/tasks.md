## 1. book_store 组件骨架

- [x] 1.1 创建 `components/book_store/` 目录结构：`CMakeLists.txt`、`include/book_store.h`、`book_store.c`
- [x] 1.2 在 `book_store.h` 中定义 `book_info_t` 结构体（name, path, size_bytes）、排序枚举 `book_sort_t`、`BOOK_STORE_MAX_BOOKS` 常量及所有公开 API 声明

## 2. book_store 核心实现

- [x] 2.1 实现 `book_store_scan()`：遍历 `/sdcard` 根目录，筛选 `.txt` 文件，填充内部书籍数组
- [x] 2.2 实现 `book_store_get_count()` 和 `book_store_get_book(index)` 访问接口
- [x] 2.3 实现 `book_store_sort()`：支持按文件名升序和按文件大小降序排序

## 3. settings_store 组件骨架

- [x] 3.1 创建 `components/settings_store/` 目录结构：`CMakeLists.txt`、`include/settings_store.h`、`settings_store.c`
- [x] 3.2 在 `settings_store.h` 中定义 `reading_prefs_t` 和 `reading_progress_t` 结构体及所有公开 API 声明

## 4. settings_store 核心实现

- [x] 4.1 实现 `settings_store_init()`：NVS flash 初始化（含分区损坏时擦除重建）
- [x] 4.2 实现 `settings_store_save_prefs()` / `settings_store_load_prefs()`：阅读偏好的 NVS 读写
- [x] 4.3 实现 `settings_store_save_progress()` / `settings_store_load_progress()`：阅读进度的 NVS blob 读写（MD5 hash key）
- [x] 4.4 实现 `settings_store_save_sort_order()` / `settings_store_load_sort_order()`：排序偏好读写

## 5. 集成

- [x] 5.1 在 `main/main.c` 初始化流程中调用 `settings_store_init()`
- [x] 5.2 编译验证：确保两个新组件与现有代码无冲突，`idf.py build` 通过
