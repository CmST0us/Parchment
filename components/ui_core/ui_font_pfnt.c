/**
 * @file ui_font_pfnt.c
 * @brief .pfnt 二进制字体文件加载/卸载实现。
 *
 * 从 LittleFS 读取 .pfnt 文件，在 PSRAM 中构建 EpdFont 结构体。
 */

#include "ui_font_pfnt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "pfnt";

int pfnt_read_header(const char *path, pfnt_header_t *header) {
    if (!path || !header) {
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    size_t n = fread(header, 1, sizeof(pfnt_header_t), f);
    fclose(f);
    if (n != sizeof(pfnt_header_t)) {
        return -1;
    }
    if (header->magic != PFNT_MAGIC) {
        return -1;
    }
    return 0;
}

EpdFont *pfnt_load(const char *path) {
    if (!path) {
        ESP_LOGE(TAG, "pfnt_load: path is NULL");
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return NULL;
    }

    /* 读取文件头。 */
    pfnt_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        ESP_LOGE(TAG, "Failed to read header from %s", path);
        fclose(f);
        return NULL;
    }

    /* 校验 magic 和版本。 */
    if (hdr.magic != PFNT_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic in %s: 0x%08lx", path,
                 (unsigned long)hdr.magic);
        fclose(f);
        return NULL;
    }
    if (hdr.version > PFNT_VERSION) {
        ESP_LOGE(TAG, "Unsupported version %d in %s", hdr.version, path);
        fclose(f);
        return NULL;
    }

    /* 计算各段大小。 */
    size_t intervals_size = hdr.interval_count * sizeof(pfnt_interval_t);
    size_t glyphs_size = hdr.glyph_count * sizeof(pfnt_glyph_t);

    /* 获取 bitmap 数据大小：文件剩余部分。 */
    long pos_after_meta = sizeof(hdr) + intervals_size + glyphs_size;
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    long bitmap_size = file_size - pos_after_meta;
    if (bitmap_size < 0) {
        ESP_LOGE(TAG, "Corrupt file %s: bitmap_size=%ld", path, bitmap_size);
        fclose(f);
        return NULL;
    }

    /* 在 PSRAM 中分配内存。 */
    EpdFont *font = heap_caps_calloc(1, sizeof(EpdFont), MALLOC_CAP_SPIRAM);
    EpdUnicodeInterval *intervals = heap_caps_malloc(
        intervals_size, MALLOC_CAP_SPIRAM);
    EpdGlyph *glyphs = heap_caps_malloc(
        hdr.glyph_count * sizeof(EpdGlyph), MALLOC_CAP_SPIRAM);
    uint8_t *bitmap = heap_caps_malloc(bitmap_size, MALLOC_CAP_SPIRAM);

    if (!font || !intervals || !glyphs || !bitmap) {
        ESP_LOGE(TAG, "PSRAM alloc failed for %s (need %ld bytes)",
                 path, (long)(sizeof(EpdFont) + intervals_size +
                              hdr.glyph_count * sizeof(EpdGlyph) +
                              bitmap_size));
        heap_caps_free(font);
        heap_caps_free(intervals);
        heap_caps_free(glyphs);
        heap_caps_free(bitmap);
        fclose(f);
        return NULL;
    }

    /* 读取 intervals。 */
    fseek(f, sizeof(hdr), SEEK_SET);
    pfnt_interval_t *raw_intervals = heap_caps_malloc(
        intervals_size, MALLOC_CAP_SPIRAM);
    if (!raw_intervals ||
        fread(raw_intervals, 1, intervals_size, f) != intervals_size) {
        ESP_LOGE(TAG, "Failed to read intervals from %s", path);
        goto fail;
    }
    for (uint32_t i = 0; i < hdr.interval_count; i++) {
        intervals[i].first = raw_intervals[i].first;
        intervals[i].last = raw_intervals[i].last;
        intervals[i].offset = raw_intervals[i].glyph_offset;
    }
    heap_caps_free(raw_intervals);
    raw_intervals = NULL;

    /* 读取 glyph table。 */
    pfnt_glyph_t *raw_glyphs = heap_caps_malloc(
        glyphs_size, MALLOC_CAP_SPIRAM);
    if (!raw_glyphs ||
        fread(raw_glyphs, 1, glyphs_size, f) != glyphs_size) {
        ESP_LOGE(TAG, "Failed to read glyph table from %s", path);
        heap_caps_free(raw_glyphs);
        goto fail;
    }
    for (uint32_t i = 0; i < hdr.glyph_count; i++) {
        glyphs[i].width = raw_glyphs[i].width;
        glyphs[i].height = raw_glyphs[i].height;
        glyphs[i].advance_x = raw_glyphs[i].advance_x;
        glyphs[i].left = raw_glyphs[i].left;
        glyphs[i].top = raw_glyphs[i].top;
        glyphs[i].compressed_size = raw_glyphs[i].compressed_size;
        glyphs[i].data_offset = raw_glyphs[i].data_offset;
    }
    heap_caps_free(raw_glyphs);

    /* 读取 bitmap 数据。 */
    if (fread(bitmap, 1, bitmap_size, f) != (size_t)bitmap_size) {
        ESP_LOGE(TAG, "Failed to read bitmap data from %s", path);
        goto fail;
    }
    fclose(f);

    /* 组装 EpdFont。 */
    font->bitmap = bitmap;
    font->glyph = glyphs;
    font->intervals = intervals;
    font->interval_count = hdr.interval_count;
    font->compressed = (hdr.flags & PFNT_FLAG_COMPRESSED) ? true : false;
    font->advance_y = hdr.advance_y;
    font->ascender = hdr.ascender;
    font->descender = hdr.descender;

    ESP_LOGI(TAG, "Loaded %s: %lupx, %lu glyphs, %ld bytes bitmap",
             path, (unsigned long)hdr.font_size_px,
             (unsigned long)hdr.glyph_count, bitmap_size);
    return font;

fail:
    heap_caps_free(font);
    heap_caps_free(intervals);
    heap_caps_free(glyphs);
    heap_caps_free(bitmap);
    fclose(f);
    return NULL;
}

void pfnt_unload(EpdFont *font) {
    if (!font) {
        return;
    }
    /* bitmap, glyph, intervals 均为 PSRAM 分配，需逐一释放。 */
    heap_caps_free((void *)font->bitmap);
    heap_caps_free((void *)font->glyph);
    heap_caps_free((void *)font->intervals);
    heap_caps_free(font);
}
