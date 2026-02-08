/**
 * @file text_paginator.c
 * @brief 文本分页器实现。
 */

#include "text_paginator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "text_paginator";

/** 分页扫描时的读取块大小。 */
#define SCAN_BUF_SIZE 8192

/** 分页缓存目录。 */
#define CACHE_DIR "/sdcard/books/.parchment"

/* ========================================================================== */
/*  Hash 辅助                                                                  */
/* ========================================================================== */

/**
 * @brief 简单的 djb2 字符串 hash。
 */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief 计算排版参数 hash。
 */
static uint32_t hash_layout_params(const text_layout_params_t *p) {
    uint32_t h = 5381;
    h = h * 33 + p->font_size;
    h = h * 33 + p->line_height;
    h = h * 33 + p->margin_top;
    h = h * 33 + p->margin_bottom;
    h = h * 33 + p->margin_left;
    h = h * 33 + p->margin_right;
    return h;
}

/* ========================================================================== */
/*  缓存路径生成                                                                */
/* ========================================================================== */

/**
 * @brief 从文件路径生成缓存文件路径。
 *
 * /sdcard/books/test.txt → /sdcard/books/.parchment/test.txt.idx
 */
static void cache_path_from_file(const char *file_path, char *out, size_t out_size) {
    const char *fname = strrchr(file_path, '/');
    fname = fname ? fname + 1 : file_path;
    snprintf(out, out_size, "%s/%s.idx", CACHE_DIR, fname);
}

/* ========================================================================== */
/*  缓存读写                                                                   */
/* ========================================================================== */

/**
 * @brief 写入缓存文件。
 */
static esp_err_t write_cache(const char *file_path, uint32_t font_hash,
                              const text_layout_params_t *params,
                              const page_index_t *index) {
    /* 确保缓存目录存在 */
    mkdir(CACHE_DIR, 0755);

    char path[256];
    cache_path_from_file(file_path, path, sizeof(path));

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGW(TAG, "Cannot write cache: %s", path);
        return ESP_FAIL;
    }

    paginator_cache_header_t hdr = {
        .magic       = PAGINATOR_MAGIC,
        .version     = PAGINATOR_VERSION,
        .font_hash   = font_hash,
        .font_size   = (uint16_t)params->font_size,
        .layout_hash = hash_layout_params(params),
        .total_pages = index->total_pages,
    };

    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(index->offsets, sizeof(uint32_t), index->total_pages, f);
    fclose(f);

    ESP_LOGI(TAG, "Cache written: %s (%"PRIu32" pages)", path, index->total_pages);
    return ESP_OK;
}

/* ========================================================================== */
/*  公开 API                                                                   */
/* ========================================================================== */

esp_err_t paginator_build(const char *file_path, font_handle_t font,
                           const text_layout_params_t *params,
                           page_index_t *index) {
    if (!file_path || !font || !params || !index) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(file_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open: %s", file_path);
        return ESP_ERR_NOT_FOUND;
    }

    /* 获取文件大小 */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        index->total_pages = 0;
        index->offsets = NULL;
        return ESP_OK;
    }

    /* 预分配 offsets（估算最大页数：文件大小 / 每页最少 100 字节） */
    uint32_t max_pages = (file_size / 100) + 16;
    uint32_t *offsets = heap_caps_malloc(max_pages * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    if (!offsets) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    char *buf = heap_caps_malloc(SCAN_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!buf) {
        free(offsets);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    uint32_t page_count = 0;
    long current_offset = 0;

    while (current_offset < file_size) {
        /* 记录本页起始偏移 */
        if (page_count >= max_pages) {
            max_pages *= 2;
            uint32_t *new_offsets = heap_caps_realloc(offsets, max_pages * sizeof(uint32_t),
                                                       MALLOC_CAP_SPIRAM);
            if (!new_offsets) {
                free(offsets);
                free(buf);
                fclose(f);
                return ESP_ERR_NO_MEM;
            }
            offsets = new_offsets;
        }
        offsets[page_count] = (uint32_t)current_offset;
        page_count++;

        /* 读取一块数据用于排版计算 */
        fseek(f, current_offset, SEEK_SET);
        size_t remaining = file_size - current_offset;
        size_t read_len = remaining < SCAN_BUF_SIZE ? remaining : SCAN_BUF_SIZE;
        size_t actual = fread(buf, 1, read_len, f);
        if (actual == 0) break;

        /* 计算本页能容纳多少文本 */
        size_t page_bytes = text_layout_calc_page_end(font, buf, actual, params);
        if (page_bytes == 0) {
            /* 防止死循环：至少推进 1 字节 */
            page_bytes = 1;
        }

        current_offset += page_bytes;
    }

    free(buf);
    fclose(f);

    /* 缩减 offsets 到实际大小 */
    if (page_count > 0 && page_count < max_pages) {
        uint32_t *trimmed = heap_caps_realloc(offsets, page_count * sizeof(uint32_t),
                                               MALLOC_CAP_SPIRAM);
        if (trimmed) offsets = trimmed;
    }

    index->total_pages = page_count;
    index->offsets = offsets;

    ESP_LOGI(TAG, "Paginated: %s → %"PRIu32" pages", file_path, page_count);

    /* 写入缓存（best effort） */
    write_cache(file_path, 0, params, index);

    return ESP_OK;
}

esp_err_t paginator_load_cache(const char *file_path, const char *font_path,
                                const text_layout_params_t *params,
                                page_index_t *index) {
    if (!file_path || !params || !index) return ESP_ERR_INVALID_ARG;

    char path[256];
    cache_path_from_file(file_path, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;

    paginator_cache_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return ESP_ERR_NOT_FOUND;
    }

    /* 校验 */
    uint32_t font_h = font_path ? hash_string(font_path) : 0;
    uint32_t layout_h = hash_layout_params(params);

    if (hdr.magic != PAGINATOR_MAGIC ||
        hdr.version != PAGINATOR_VERSION ||
        hdr.font_hash != font_h ||
        hdr.font_size != (uint16_t)params->font_size ||
        hdr.layout_hash != layout_h ||
        hdr.total_pages == 0) {
        fclose(f);
        return ESP_ERR_NOT_FOUND;
    }

    uint32_t *offsets = heap_caps_malloc(hdr.total_pages * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    if (!offsets) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    if (fread(offsets, sizeof(uint32_t), hdr.total_pages, f) != hdr.total_pages) {
        free(offsets);
        fclose(f);
        return ESP_ERR_NOT_FOUND;
    }

    fclose(f);

    index->total_pages = hdr.total_pages;
    index->offsets = offsets;

    ESP_LOGI(TAG, "Cache loaded: %s (%"PRIu32" pages)", path, hdr.total_pages);
    return ESP_OK;
}

size_t paginator_read_page(const char *file_path, const page_index_t *index,
                            uint32_t page_num, char *buf, size_t buf_size) {
    if (!file_path || !index || !buf || buf_size == 0) return 0;
    if (page_num >= index->total_pages) return 0;

    FILE *f = fopen(file_path, "rb");
    if (!f) return 0;

    uint32_t start = index->offsets[page_num];
    uint32_t end_offset;

    if (page_num + 1 < index->total_pages) {
        end_offset = index->offsets[page_num + 1];
    } else {
        fseek(f, 0, SEEK_END);
        end_offset = (uint32_t)ftell(f);
    }

    size_t page_len = end_offset - start;
    if (page_len > buf_size - 1) {
        page_len = buf_size - 1;
    }

    fseek(f, start, SEEK_SET);
    size_t actual = fread(buf, 1, page_len, f);
    fclose(f);

    buf[actual] = '\0';
    return actual;
}

void paginator_free(page_index_t *index) {
    if (!index) return;
    if (index->offsets) {
        free(index->offsets);
        index->offsets = NULL;
    }
    index->total_pages = 0;
}
