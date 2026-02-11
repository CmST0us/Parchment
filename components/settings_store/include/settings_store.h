/**
 * @file settings_store.h
 * @brief 设置与进度存储 — 基于 NVS 的阅读偏好和阅读进度持久化。
 *
 * 使用两个 NVS namespace：
 * - "settings": 全局阅读偏好（字号、行距等）
 * - "progress": 每本书的阅读进度（以文件路径 MD5 hash 为 key）
 */

#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 阅读偏好默认值。 */
#define SETTINGS_DEFAULT_FONT_SIZE         20
#define SETTINGS_DEFAULT_LINE_SPACING      16  /* 实际 1.6, 存储 ×10 */
#define SETTINGS_DEFAULT_PARAGRAPH_SPACING 8
#define SETTINGS_DEFAULT_MARGIN            24
#define SETTINGS_DEFAULT_FULL_REFRESH      5

/** 阅读偏好结构体。 */
typedef struct {
    uint8_t font_size;           /**< 字号（14-32, 默认 20） */
    uint8_t line_spacing;        /**< 行距 ×10（10-25, 默认 16 即 1.6） */
    uint8_t paragraph_spacing;   /**< 段间距像素（0-24, 默认 8） */
    uint8_t margin;              /**< 页边距像素（16/24/36, 默认 24） */
    uint8_t full_refresh_pages;  /**< 全刷间隔页数（1-20, 默认 5） */
} reading_prefs_t;

/** 阅读进度结构体。 */
typedef struct {
    uint32_t byte_offset;   /**< 当前阅读字节偏移 */
    uint32_t total_bytes;   /**< 文件总字节数 */
    uint32_t current_page;  /**< 当前页码 */
    uint32_t total_pages;   /**< 总页数 */
} reading_progress_t;

/**
 * @brief 初始化 NVS flash 存储。
 *
 * 若 NVS 分区损坏，自动擦除并重新初始化。
 *
 * @return ESP_OK 成功。
 */
esp_err_t settings_store_init(void);

/**
 * @brief 保存阅读偏好到 NVS。
 *
 * @param prefs 偏好设置。
 * @return ESP_OK 成功。
 */
esp_err_t settings_store_save_prefs(const reading_prefs_t *prefs);

/**
 * @brief 从 NVS 加载阅读偏好。
 *
 * 不存在的字段使用默认值填充。
 *
 * @param prefs 输出偏好设置。
 * @return ESP_OK 成功。
 */
esp_err_t settings_store_load_prefs(reading_prefs_t *prefs);

/**
 * @brief 保存某本书的阅读进度到 NVS。
 *
 * @param file_path 书籍文件完整路径。
 * @param progress  阅读进度。
 * @return ESP_OK 成功。
 */
esp_err_t settings_store_save_progress(const char *file_path,
                                       const reading_progress_t *progress);

/**
 * @brief 加载某本书的阅读进度。
 *
 * @param file_path 书籍文件完整路径。
 * @param progress  输出阅读进度。
 * @return ESP_OK 成功，ESP_ERR_NOT_FOUND 无已保存进度（进度置零）。
 */
esp_err_t settings_store_load_progress(const char *file_path,
                                       reading_progress_t *progress);

/**
 * @brief 保存书库排序偏好。
 *
 * @param order 排序方式（存储为 uint8_t）。
 * @return ESP_OK 成功。
 */
esp_err_t settings_store_save_sort_order(uint8_t order);

/**
 * @brief 加载书库排序偏好。
 *
 * @param order 输出排序方式。
 * @return ESP_OK 成功，无已保存值时 order 设为 0（BOOK_SORT_NAME）。
 */
esp_err_t settings_store_load_sort_order(uint8_t *order);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_STORE_H */
