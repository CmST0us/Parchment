/**
 * @file text_paginator.h
 * @brief 文本分页器。
 *
 * 预扫描 TXT 文件生成每页起始偏移的分页表，
 * 支持 .idx 缓存文件持久化以避免重复扫描。
 */

#ifndef TEXT_PAGINATOR_H
#define TEXT_PAGINATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "text_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 分页缓存文件 magic 标识。 */
#define PAGINATOR_MAGIC 0x50494458  /* "PIDX" */

/** 分页缓存文件版本。 */
#define PAGINATOR_VERSION 1

/** 分页索引。 */
typedef struct {
    uint32_t  total_pages;  /**< 总页数。 */
    uint32_t *offsets;      /**< 每页起始字节偏移数组，长度为 total_pages。 */
} page_index_t;

/** 分页缓存文件 header。 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /**< PAGINATOR_MAGIC */
    uint16_t version;           /**< PAGINATOR_VERSION */
    uint32_t font_hash;         /**< 字体文件路径 hash。 */
    uint16_t font_size;         /**< 字号。 */
    uint32_t layout_hash;       /**< 排版参数 hash。 */
    uint32_t total_pages;       /**< 总页数。 */
} paginator_cache_header_t;

/**
 * @brief 构建分页索引。
 *
 * 扫描整个 TXT 文件，按当前字体和排版参数计算每页的起始偏移。
 * 完成后自动写入 .idx 缓存文件。
 *
 * @param file_path   TXT 文件路径。
 * @param font        字体句柄。
 * @param params      排版参数。
 * @param[out] index  输出分页索引（调用方负责调用 paginator_free 释放）。
 * @return ESP_OK 成功。
 */
esp_err_t paginator_build(const char *file_path, font_handle_t font,
                           const text_layout_params_t *params,
                           page_index_t *index);

/**
 * @brief 尝试从缓存加载分页索引。
 *
 * 如果缓存有效（magic、版本、字体 hash、排版参数匹配），直接加载。
 *
 * @param file_path   TXT 文件路径。
 * @param font_path   当前字体文件路径（用于计算 hash）。
 * @param params      排版参数。
 * @param[out] index  输出分页索引。
 * @return ESP_OK 缓存有效并已加载，ESP_ERR_NOT_FOUND 缓存无效或不存在。
 */
esp_err_t paginator_load_cache(const char *file_path, const char *font_path,
                                const text_layout_params_t *params,
                                page_index_t *index);

/**
 * @brief 读取指定页的文本内容。
 *
 * @param file_path TXT 文件路径。
 * @param index     分页索引。
 * @param page_num  页码（0-based）。
 * @param[out] buf  输出缓冲区。
 * @param buf_size  缓冲区大小。
 * @return 读取的字节数，0 表示失败。
 */
size_t paginator_read_page(const char *file_path, const page_index_t *index,
                            uint32_t page_num, char *buf, size_t buf_size);

/**
 * @brief 释放分页索引。
 *
 * @param index 分页索引。
 */
void paginator_free(page_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_PAGINATOR_H */
