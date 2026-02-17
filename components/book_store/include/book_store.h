/**
 * @file book_store.h
 * @brief 书籍管理 — SD 卡 TXT 文件扫描与列表维护。
 *
 * 扫描 /sdcard/books 目录下的 .txt 文件，提供书籍列表查询和排序功能。
 */

#ifndef BOOK_STORE_H
#define BOOK_STORE_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 最大可扫描的书籍数量。 */
#define BOOK_STORE_MAX_BOOKS 64

/** 书籍文件名最大长度（含 NUL）。 */
#define BOOK_NAME_MAX_LEN 128

/** 书籍完整路径最大长度（含 NUL）。 */
#define BOOK_PATH_MAX_LEN 280

/** 书籍排序方式。 */
typedef enum {
    BOOK_SORT_NAME,  /**< 按文件名字母升序 */
    BOOK_SORT_SIZE,  /**< 按文件大小降序 */
} book_sort_t;

/** 单本书籍信息。 */
typedef struct {
    char name[BOOK_NAME_MAX_LEN];   /**< 文件名（不含路径） */
    char path[BOOK_PATH_MAX_LEN];   /**< 完整路径（如 /sdcard/book.txt） */
    uint32_t size_bytes;            /**< 文件大小（字节） */
} book_info_t;

/**
 * @brief 扫描 SD 卡根目录的 TXT 文件。
 *
 * 清空已有列表，重新遍历 /sdcard/books 目录筛选 .txt 文件。
 * 最多保留 BOOK_STORE_MAX_BOOKS 本。
 *
 * @return ESP_OK 成功（含 0 本书的情况），
 *         ESP_ERR_INVALID_STATE SD 卡未挂载。
 */
esp_err_t book_store_scan(void);

/**
 * @brief 获取当前书籍列表中的数量。
 *
 * @return 书籍数量（0 ~ BOOK_STORE_MAX_BOOKS）。
 */
size_t book_store_get_count(void);

/**
 * @brief 按索引获取书籍信息。
 *
 * 返回的指针在下次 book_store_scan() 调用前保持有效。
 *
 * @param index 书籍索引（0 起）。
 * @return 书籍信息指针，索引越界时返回 NULL。
 */
const book_info_t *book_store_get_book(size_t index);

/**
 * @brief 对书籍列表排序。
 *
 * @param sort 排序方式。
 */
void book_store_sort(book_sort_t sort);

#ifdef __cplusplus
}
#endif

#endif /* BOOK_STORE_H */
