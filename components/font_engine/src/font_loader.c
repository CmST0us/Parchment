/**
 * @file font_loader.c
 * @brief .bin 字体文件加载器 + HashMap 索引 + glyph 查找
 */

#include "font_engine/font_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "font_loader";

// ──────────────────────────────────────────────────────────────────────
// HashMap 实现
// ──────────────────────────────────────────────────────────────────────

static uint32_t hash_unicode(uint32_t unicode, uint32_t bucket_count)
{
    // FNV-1a inspired simple hash
    uint32_t h = unicode;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return h % bucket_count;
}

static bool hashmap_init(font_hashmap_t *map, uint32_t expected_count)
{
    // 使用 ~1.5x 的桶数减少冲突
    map->bucket_count = expected_count * 3 / 2;
    if (map->bucket_count < 64) map->bucket_count = 64;
    map->entry_count = 0;

    size_t alloc_size = map->bucket_count * sizeof(font_hash_entry_t *);
    map->buckets = (font_hash_entry_t **)heap_caps_calloc(1, alloc_size, MALLOC_CAP_SPIRAM);
    if (!map->buckets) {
        ESP_LOGE(TAG, "Failed to allocate hashmap buckets (%u bytes)", (unsigned)alloc_size);
        return false;
    }
    return true;
}

static bool hashmap_insert(font_hashmap_t *map, uint32_t unicode, const font_glyph_entry_t *glyph)
{
    uint32_t idx = hash_unicode(unicode, map->bucket_count);

    font_hash_entry_t *entry = (font_hash_entry_t *)heap_caps_malloc(
        sizeof(font_hash_entry_t), MALLOC_CAP_SPIRAM);
    if (!entry) return false;

    entry->unicode = unicode;
    entry->glyph = *glyph;
    entry->next = map->buckets[idx];
    map->buckets[idx] = entry;
    map->entry_count++;
    return true;
}

static const font_glyph_entry_t *hashmap_find(const font_hashmap_t *map, uint32_t unicode)
{
    uint32_t idx = hash_unicode(unicode, map->bucket_count);
    font_hash_entry_t *entry = map->buckets[idx];
    while (entry) {
        if (entry->unicode == unicode) {
            return &entry->glyph;
        }
        entry = entry->next;
    }
    return NULL;
}

static void hashmap_free(font_hashmap_t *map)
{
    if (!map->buckets) return;
    for (uint32_t i = 0; i < map->bucket_count; i++) {
        font_hash_entry_t *entry = map->buckets[i];
        while (entry) {
            font_hash_entry_t *next = entry->next;
            heap_caps_free(entry);
            entry = next;
        }
    }
    heap_caps_free(map->buckets);
    map->buckets = NULL;
    map->entry_count = 0;
}

// ──────────────────────────────────────────────────────────────────────
// 文件加载
// ──────────────────────────────────────────────────────────────────────

/** 从 3 字节 LE 读取 uint32 (低 24 位) */
static uint32_t read_offset24(const uint8_t bytes[3])
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
}

bool font_engine_load(font_engine_t *engine, const char *path)
{
    if (!engine || !path) return false;

    memset(engine, 0, sizeof(font_engine_t));

    // 初始化页面缓存 slots
    for (int i = 0; i < FONT_PAGE_CACHE_WINDOW; i++) {
        engine->page_cache.slots[i].page_id = -1;
    }
    engine->page_cache.center_page = -1;

    // 打开文件
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open font file: %s", path);
        return false;
    }

    // 读取文件头
    if (fread(&engine->header, 1, FONT_HEADER_SIZE, f) != FONT_HEADER_SIZE) {
        ESP_LOGE(TAG, "Failed to read font header");
        fclose(f);
        return false;
    }

    // 校验 magic
    if (engine->header.magic != FONT_MAGIC) {
        ESP_LOGE(TAG, "Invalid font magic: 0x%08X (expected 0x%08X)",
                 (unsigned)engine->header.magic, (unsigned)FONT_MAGIC);
        fclose(f);
        return false;
    }

    // 校验版本
    if (engine->header.version != FONT_VERSION) {
        ESP_LOGE(TAG, "Unsupported font version: %d", engine->header.version);
        fclose(f);
        return false;
    }

    ESP_LOGI(TAG, "Loading font: %s, height=%d, glyphs=%u",
             engine->header.family_name, engine->header.font_height,
             (unsigned)engine->header.glyph_count);

    // 构建 HashMap 索引
    if (!hashmap_init(&engine->index, engine->header.glyph_count)) {
        fclose(f);
        return false;
    }

    // 逐个读取 glyph 条目并插入 HashMap
    for (uint32_t i = 0; i < engine->header.glyph_count; i++) {
        font_glyph_entry_t entry;
        if (fread(&entry, 1, FONT_GLYPH_ENTRY_SIZE, f) != FONT_GLYPH_ENTRY_SIZE) {
            ESP_LOGE(TAG, "Failed to read glyph entry %u", (unsigned)i);
            hashmap_free(&engine->index);
            fclose(f);
            return false;
        }
        if (!hashmap_insert(&engine->index, entry.unicode, &entry)) {
            ESP_LOGE(TAG, "Failed to insert glyph U+%04X into hashmap",
                     (unsigned)entry.unicode);
            hashmap_free(&engine->index);
            fclose(f);
            return false;
        }
    }

    // 保存文件句柄和路径
    engine->file = f;
    strncpy(engine->file_path, path, sizeof(engine->file_path) - 1);
    engine->loaded = true;

    ESP_LOGI(TAG, "Font loaded: %u glyphs indexed", (unsigned)engine->index.entry_count);
    return true;
}

void font_engine_unload(font_engine_t *engine)
{
    if (!engine) return;

    // 释放 HashMap
    hashmap_free(&engine->index);

    // 释放常用字缓存
    if (engine->common_cache.glyphs) {
        for (uint32_t i = 0; i < engine->common_cache.count; i++) {
            if (engine->common_cache.glyphs[i].bitmap) {
                heap_caps_free(engine->common_cache.glyphs[i].bitmap);
            }
        }
        heap_caps_free(engine->common_cache.glyphs);
    }
    if (engine->common_cache.lookup) {
        for (uint32_t i = 0; i < engine->common_cache.bucket_count; i++) {
            font_hash_entry_t *e = engine->common_cache.lookup[i];
            while (e) {
                font_hash_entry_t *next = e->next;
                heap_caps_free(e);
                e = next;
            }
        }
        heap_caps_free(engine->common_cache.lookup);
    }

    // 释放页面缓存
    for (int i = 0; i < FONT_PAGE_CACHE_WINDOW; i++) {
        font_page_slot_t *slot = &engine->page_cache.slots[i];
        if (slot->glyphs) {
            for (uint32_t j = 0; j < slot->count; j++) {
                if (slot->glyphs[j].bitmap) {
                    heap_caps_free(slot->glyphs[j].bitmap);
                }
            }
            heap_caps_free(slot->glyphs);
        }
    }

    // 释放回收池
    if (engine->recycle_pool.buckets) {
        for (uint32_t i = 0; i < engine->recycle_pool.bucket_count; i++) {
            font_recycle_node_t *node = engine->recycle_pool.buckets[i];
            while (node) {
                font_recycle_node_t *next = node->hash_next;
                if (node->glyph.bitmap) {
                    heap_caps_free(node->glyph.bitmap);
                }
                heap_caps_free(node);
                node = next;
            }
        }
        heap_caps_free(engine->recycle_pool.buckets);
    }

    // 关闭文件
    if (engine->file) {
        fclose(engine->file);
    }

    memset(engine, 0, sizeof(font_engine_t));
}

// ──────────────────────────────────────────────────────────────────────
// Glyph 读取和解码
// ──────────────────────────────────────────────────────────────────────

/** 从文件中读取 RLE 数据并解码为 4bpp packed bitmap */
static bool decode_glyph_from_file(font_engine_t *engine,
                                   const font_glyph_entry_t *entry,
                                   font_decoded_glyph_t *out)
{
    if (entry->width == 0 || entry->height == 0) {
        // 空 glyph（如空格）
        out->unicode = entry->unicode;
        out->width = entry->width;
        out->height = entry->height;
        out->advance_x = entry->advance_x;
        out->x_offset = entry->x_offset;
        out->y_offset = entry->y_offset;
        out->bitmap = NULL;
        out->bitmap_bytes = 0;
        return true;
    }

    uint32_t offset = read_offset24(entry->bitmap_offset);

    // 读取 RLE 数据
    uint8_t *rle_buf = (uint8_t *)malloc(entry->bitmap_size);
    if (!rle_buf) return false;

    fseek(engine->file, offset, SEEK_SET);
    if (fread(rle_buf, 1, entry->bitmap_size, engine->file) != entry->bitmap_size) {
        free(rle_buf);
        return false;
    }

    // 解码为 4bpp packed
    uint32_t row_bytes = (entry->width + 1) / 2;
    uint32_t decoded_size = row_bytes * entry->height;
    uint8_t *decoded = (uint8_t *)heap_caps_malloc(decoded_size, MALLOC_CAP_SPIRAM);
    if (!decoded) {
        free(rle_buf);
        return false;
    }

    int pixels = font_rle_decode(rle_buf, entry->bitmap_size,
                                 entry->width, entry->height,
                                 decoded, decoded_size);
    free(rle_buf);

    if (pixels != entry->width * entry->height) {
        ESP_LOGW(TAG, "RLE decode mismatch for U+%04X: got %d, expected %d",
                 (unsigned)entry->unicode, pixels, entry->width * entry->height);
    }

    out->unicode = entry->unicode;
    out->width = entry->width;
    out->height = entry->height;
    out->advance_x = entry->advance_x;
    out->x_offset = entry->x_offset;
    out->y_offset = entry->y_offset;
    out->bitmap = decoded;
    out->bitmap_bytes = decoded_size;
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// 缓存查找
// ──────────────────────────────────────────────────────────────────────

/** 在常用字缓存中查找 */
static const font_decoded_glyph_t *find_in_common_cache(const font_common_cache_t *cache,
                                                         uint32_t unicode)
{
    if (!cache->lookup || cache->count == 0) return NULL;

    uint32_t idx = hash_unicode(unicode, cache->bucket_count);
    font_hash_entry_t *e = cache->lookup[idx];
    while (e) {
        if (e->unicode == unicode) {
            // glyph.unicode 存的是 index
            uint32_t glyph_idx = e->glyph.unicode;
            if (glyph_idx < cache->count) {
                return &cache->glyphs[glyph_idx];
            }
        }
        e = e->next;
    }
    return NULL;
}

/** 在页面缓存中查找 */
static const font_decoded_glyph_t *find_in_page_cache(const font_page_cache_t *cache,
                                                       uint32_t unicode)
{
    for (int s = 0; s < FONT_PAGE_CACHE_WINDOW; s++) {
        const font_page_slot_t *slot = &cache->slots[s];
        if (slot->page_id < 0) continue;
        for (uint32_t i = 0; i < slot->count; i++) {
            if (slot->glyphs[i].unicode == unicode) {
                return &slot->glyphs[i];
            }
        }
    }
    return NULL;
}

/** 在回收池中查找 */
static font_decoded_glyph_t *find_in_recycle_pool(font_recycle_pool_t *pool, uint32_t unicode)
{
    if (!pool->buckets || pool->count == 0) return NULL;

    uint32_t idx = hash_unicode(unicode, pool->bucket_count);
    font_recycle_node_t *node = pool->buckets[idx];
    while (node) {
        if (node->glyph.unicode == unicode) {
            // 更新 LRU: 移到头部
            node->lru_tick = ++pool->tick;
            if (node != pool->lru_head) {
                // 从链表中摘除
                if (node->lru_prev) node->lru_prev->lru_next = node->lru_next;
                if (node->lru_next) node->lru_next->lru_prev = node->lru_prev;
                if (node == pool->lru_tail) pool->lru_tail = node->lru_prev;
                // 插入头部
                node->lru_prev = NULL;
                node->lru_next = pool->lru_head;
                if (pool->lru_head) pool->lru_head->lru_prev = node;
                pool->lru_head = node;
            }
            return &node->glyph;
        }
        node = node->hash_next;
    }
    return NULL;
}

const font_decoded_glyph_t *font_engine_get_glyph(font_engine_t *engine, uint32_t unicode)
{
    if (!engine || !engine->loaded) return NULL;

    // 1. 页面缓存
    const font_decoded_glyph_t *result = find_in_page_cache(&engine->page_cache, unicode);
    if (result) return result;

    // 2. 常用字缓存
    result = find_in_common_cache(&engine->common_cache, unicode);
    if (result) return result;

    // 3. 回收池
    font_decoded_glyph_t *recycled = find_in_recycle_pool(&engine->recycle_pool, unicode);
    if (recycled) return recycled;

    // 4. 文件读取 + 解码
    const font_glyph_entry_t *entry = hashmap_find(&engine->index, unicode);
    if (!entry) return NULL;

    // 解码 glyph — 暂时用静态缓冲区（真正的集成在页面缓存中做）
    static font_decoded_glyph_t temp_glyph;
    if (temp_glyph.bitmap) {
        heap_caps_free(temp_glyph.bitmap);
        temp_glyph.bitmap = NULL;
    }
    if (!decode_glyph_from_file(engine, entry, &temp_glyph)) {
        return NULL;
    }
    return &temp_glyph;
}

// ──────────────────────────────────────────────────────────────────────
// 缩放 glyph 获取
// ──────────────────────────────────────────────────────────────────────

bool font_engine_get_scaled_glyph(font_engine_t *engine, uint32_t unicode,
                                  uint8_t target_size, font_scaled_glyph_t *out)
{
    if (!engine || !engine->loaded || !out) return false;

    const font_decoded_glyph_t *base = font_engine_get_glyph(engine, unicode);
    if (!base) return false;

    float scale = (float)target_size / engine->header.font_height;
    if (scale > 1.0f) scale = 1.0f;
    if (scale < 0.5f) scale = 0.5f;

    out->unicode = unicode;
    out->advance_x = (uint8_t)roundf(base->advance_x * scale);
    out->x_offset = (int8_t)roundf(base->x_offset * scale);
    out->y_offset = (int8_t)roundf(base->y_offset * scale);

    if (base->width == 0 || base->height == 0 || !base->bitmap) {
        out->width = 0;
        out->height = 0;
        out->bitmap = NULL;
        out->bitmap_bytes = 0;
        return true;
    }

    if (scale >= 0.999f) {
        // 原始字号，只做 4bpp → 8bpp 转换
        out->width = base->width;
        out->height = base->height;
        uint32_t out_size = out->width * out->height;
        out->bitmap = (uint8_t *)heap_caps_malloc(out_size, MALLOC_CAP_SPIRAM);
        if (!out->bitmap) return false;
        out->bitmap_bytes = out_size;

        // 4bpp packed → 8bpp
        uint32_t row_bytes = (base->width + 1) / 2;
        for (int y = 0; y < base->height; y++) {
            for (int x = 0; x < base->width; x++) {
                uint32_t byte_idx = y * row_bytes + x / 2;
                uint8_t gray4;
                if (x % 2 == 0) {
                    gray4 = (base->bitmap[byte_idx] >> 4) & 0x0F;
                } else {
                    gray4 = base->bitmap[byte_idx] & 0x0F;
                }
                out->bitmap[y * out->width + x] = gray4 * 17;
            }
        }
    } else {
        // 面积加权缩放
        out->width = (uint8_t)roundf(base->width * scale);
        out->height = (uint8_t)roundf(base->height * scale);
        if (out->width == 0) out->width = 1;
        if (out->height == 0) out->height = 1;

        uint32_t out_size = out->width * out->height;
        out->bitmap = (uint8_t *)heap_caps_malloc(out_size, MALLOC_CAP_SPIRAM);
        if (!out->bitmap) return false;
        out->bitmap_bytes = out_size;

        font_scale_area_weighted(base->bitmap, base->width, base->height,
                                 out->bitmap, out->width, out->height);
    }

    return true;
}

// ──────────────────────────────────────────────────────────────────────
// 页面缓存管理
// ──────────────────────────────────────────────────────────────────────

void font_engine_set_page(font_engine_t *engine, int32_t page_id)
{
    if (!engine || !engine->loaded) return;

    engine->page_cache.center_page = page_id;

    // 计算新窗口范围
    int32_t new_start = page_id - FONT_PAGE_CACHE_WINDOW / 2;
    int32_t new_end = page_id + FONT_PAGE_CACHE_WINDOW / 2;

    // 释放窗口外的 slot
    for (int i = 0; i < FONT_PAGE_CACHE_WINDOW; i++) {
        font_page_slot_t *slot = &engine->page_cache.slots[i];
        if (slot->page_id >= 0 && (slot->page_id < new_start || slot->page_id > new_end)) {
            // 转入回收池（如果回收池已初始化）
            // TODO: 实现回收池转入逻辑
            // 目前只释放
            for (uint32_t j = 0; j < slot->count; j++) {
                if (slot->glyphs[j].bitmap) {
                    heap_caps_free(slot->glyphs[j].bitmap);
                }
            }
            slot->count = 0;
            slot->page_id = -1;
        }
    }
}

void font_engine_page_cache_add(font_engine_t *engine, int32_t page_id, uint32_t unicode)
{
    if (!engine || !engine->loaded) return;

    // 查找或分配 slot
    font_page_slot_t *target = NULL;
    for (int i = 0; i < FONT_PAGE_CACHE_WINDOW; i++) {
        if (engine->page_cache.slots[i].page_id == page_id) {
            target = &engine->page_cache.slots[i];
            break;
        }
    }
    if (!target) {
        // 找空 slot
        for (int i = 0; i < FONT_PAGE_CACHE_WINDOW; i++) {
            if (engine->page_cache.slots[i].page_id < 0) {
                target = &engine->page_cache.slots[i];
                target->page_id = page_id;
                break;
            }
        }
    }
    if (!target) return;  // 所有 slot 都被占用

    // 检查是否已缓存
    for (uint32_t i = 0; i < target->count; i++) {
        if (target->glyphs[i].unicode == unicode) return;
    }

    // 解码并添加
    const font_glyph_entry_t *entry = hashmap_find(&engine->index, unicode);
    if (!entry) return;

    // 扩展 glyph 数组
    if (target->count >= target->capacity) {
        uint32_t new_cap = target->capacity ? target->capacity * 2 : 64;
        font_decoded_glyph_t *new_arr = (font_decoded_glyph_t *)heap_caps_realloc(
            target->glyphs, new_cap * sizeof(font_decoded_glyph_t), MALLOC_CAP_SPIRAM);
        if (!new_arr) return;
        target->glyphs = new_arr;
        target->capacity = new_cap;
    }

    font_decoded_glyph_t decoded;
    if (decode_glyph_from_file(engine, entry, &decoded)) {
        target->glyphs[target->count++] = decoded;
    }
}
