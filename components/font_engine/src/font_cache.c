/**
 * @file font_cache.c
 * @brief 字体缓存系统 — 常用字缓存初始化 + 回收池管理
 */

#include "font_engine/font_engine.h"
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "font_cache";

// ──────────────────────────────────────────────────────────────────────
// 常用字符列表
// ──────────────────────────────────────────────────────────────────────

/** 常用 CJK 标点和符号 */
static const uint32_t COMMON_CJK_PUNCTUATION[] = {
    0x3001, // 、
    0x3002, // 。
    0xFF0C, // ，
    0xFF1B, // ；
    0xFF1A, // ：
    0xFF01, // ！
    0xFF1F, // ？
    0xFF08, // （
    0xFF09, // ）
    0x3010, // 【
    0x3011, // 】
    0x300A, // 《
    0x300B, // 》
    0x201C, // "
    0x201D, // "
    0x2018, // '
    0x2019, // '
    0x2014, // —
    0x2026, // …
    0x00B7, // ·
};

static uint32_t hash_u32(uint32_t key, uint32_t buckets)
{
    uint32_t h = key;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return h % buckets;
}

// ──────────────────────────────────────────────────────────────────────
// 常用字缓存
// ──────────────────────────────────────────────────────────────────────

/** 前向声明：从字体引擎查找并解码 glyph */
extern bool font_engine_load(font_engine_t *engine, const char *path);

/**
 * 初始化常用字缓存。
 * 应在 font_engine_load() 成功后调用。
 */
bool font_cache_init_common(font_engine_t *engine)
{
    if (!engine || !engine->loaded) return false;

    // 收集所有需要预加载的码点
    uint32_t codepoints[256];
    uint32_t count = 0;

    // ASCII printable (0x20-0x7E)
    for (uint32_t cp = 0x20; cp <= 0x7E && count < 256; cp++) {
        codepoints[count++] = cp;
    }

    // 常用 CJK 标点
    uint32_t punct_count = sizeof(COMMON_CJK_PUNCTUATION) / sizeof(COMMON_CJK_PUNCTUATION[0]);
    for (uint32_t i = 0; i < punct_count && count < 256; i++) {
        codepoints[count++] = COMMON_CJK_PUNCTUATION[i];
    }

    ESP_LOGI(TAG, "Initializing common cache with %u characters", (unsigned)count);

    // 分配 glyph 数组
    engine->common_cache.glyphs = (font_decoded_glyph_t *)heap_caps_calloc(
        count, sizeof(font_decoded_glyph_t), MALLOC_CAP_SPIRAM);
    if (!engine->common_cache.glyphs) {
        ESP_LOGE(TAG, "Failed to allocate common cache glyphs");
        return false;
    }

    // 创建查找 HashMap
    engine->common_cache.bucket_count = count * 2;
    engine->common_cache.lookup = (font_hash_entry_t **)heap_caps_calloc(
        engine->common_cache.bucket_count, sizeof(font_hash_entry_t *), MALLOC_CAP_SPIRAM);
    if (!engine->common_cache.lookup) {
        heap_caps_free(engine->common_cache.glyphs);
        engine->common_cache.glyphs = NULL;
        return false;
    }

    // 逐个加载
    uint32_t loaded = 0;
    for (uint32_t i = 0; i < count; i++) {
        // 使用 get_glyph 从文件加载（此时缓存为空，会走文件路径）
        const font_decoded_glyph_t *g = font_engine_get_glyph(engine, codepoints[i]);
        if (!g) continue;

        // 复制到常用字缓存
        font_decoded_glyph_t *cached = &engine->common_cache.glyphs[loaded];
        *cached = *g;

        // 需要独立拥有 bitmap 内存
        if (g->bitmap && g->bitmap_bytes > 0) {
            cached->bitmap = (uint8_t *)heap_caps_malloc(g->bitmap_bytes, MALLOC_CAP_SPIRAM);
            if (cached->bitmap) {
                memcpy(cached->bitmap, g->bitmap, g->bitmap_bytes);
            }
        }

        // 插入查找表
        uint32_t idx = hash_u32(codepoints[i], engine->common_cache.bucket_count);
        font_hash_entry_t *entry = (font_hash_entry_t *)heap_caps_malloc(
            sizeof(font_hash_entry_t), MALLOC_CAP_SPIRAM);
        if (entry) {
            entry->unicode = codepoints[i];
            // 复用 glyph.unicode 字段存储 index
            entry->glyph.unicode = loaded;
            entry->next = engine->common_cache.lookup[idx];
            engine->common_cache.lookup[idx] = entry;
        }

        loaded++;
    }

    engine->common_cache.count = loaded;
    ESP_LOGI(TAG, "Common cache loaded: %u glyphs", (unsigned)loaded);
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// 回收池
// ──────────────────────────────────────────────────────────────────────

bool font_cache_init_recycle_pool(font_engine_t *engine)
{
    if (!engine) return false;

    font_recycle_pool_t *pool = &engine->recycle_pool;
    pool->bucket_count = FONT_RECYCLE_POOL_MAX;
    pool->buckets = (font_recycle_node_t **)heap_caps_calloc(
        pool->bucket_count, sizeof(font_recycle_node_t *), MALLOC_CAP_SPIRAM);
    if (!pool->buckets) {
        ESP_LOGE(TAG, "Failed to allocate recycle pool");
        return false;
    }
    pool->lru_head = NULL;
    pool->lru_tail = NULL;
    pool->count = 0;
    pool->tick = 0;

    return true;
}

/** 将 glyph 加入回收池 */
void font_cache_recycle_add(font_engine_t *engine, const font_decoded_glyph_t *glyph)
{
    if (!engine || !glyph) return;
    font_recycle_pool_t *pool = &engine->recycle_pool;
    if (!pool->buckets) return;

    // 如果池满，淘汰 LRU 尾部
    if (pool->count >= FONT_RECYCLE_POOL_MAX && pool->lru_tail) {
        font_recycle_node_t *victim = pool->lru_tail;

        // 从 LRU 链表中移除
        if (victim->lru_prev) {
            victim->lru_prev->lru_next = NULL;
        }
        pool->lru_tail = victim->lru_prev;
        if (pool->lru_head == victim) {
            pool->lru_head = NULL;
        }

        // 从 hash 链中移除
        uint32_t idx = hash_u32(victim->glyph.unicode, pool->bucket_count);
        font_recycle_node_t **pp = &pool->buckets[idx];
        while (*pp) {
            if (*pp == victim) {
                *pp = victim->hash_next;
                break;
            }
            pp = &(*pp)->hash_next;
        }

        // 释放
        if (victim->glyph.bitmap) {
            heap_caps_free(victim->glyph.bitmap);
        }
        heap_caps_free(victim);
        pool->count--;
    }

    // 创建新节点
    font_recycle_node_t *node = (font_recycle_node_t *)heap_caps_calloc(
        1, sizeof(font_recycle_node_t), MALLOC_CAP_SPIRAM);
    if (!node) return;

    node->glyph = *glyph;
    // bitmap 所有权转移（调用者不再拥有）
    node->lru_tick = ++pool->tick;

    // 插入 hash
    uint32_t idx = hash_u32(glyph->unicode, pool->bucket_count);
    node->hash_next = pool->buckets[idx];
    pool->buckets[idx] = node;

    // 插入 LRU 头部
    node->lru_prev = NULL;
    node->lru_next = pool->lru_head;
    if (pool->lru_head) {
        pool->lru_head->lru_prev = node;
    }
    pool->lru_head = node;
    if (!pool->lru_tail) {
        pool->lru_tail = node;
    }
    pool->count++;
}
