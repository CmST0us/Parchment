/**
 * @file font_engine.h
 * @brief Parchment 字体引擎公共 API
 *
 * 自定义 .bin 字体格式 (4bpp 灰度 + RLE 压缩)
 * 多级 PSRAM 缓存 + 面积加权缩放
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ──────────────────────────────────────────────────────────────────────
// .bin 文件格式常量
// ──────────────────────────────────────────────────────────────────────

#define FONT_MAGIC          0x54464E50  // "PFNT" little-endian
#define FONT_VERSION        1
#define FONT_HEADER_SIZE    128
#define FONT_GLYPH_ENTRY_SIZE 16
#define FONT_FAMILY_NAME_LEN 64

// ──────────────────────────────────────────────────────────────────────
// 数据结构
// ──────────────────────────────────────────────────────────────────────

/** .bin 文件头 (128 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t magic;                         // "PFNT"
    uint8_t  version;                       // 格式版本 (1)
    uint8_t  font_height;                   // 基础字号 (32)
    uint32_t glyph_count;                   // 字形总数
    char     family_name[FONT_FAMILY_NAME_LEN]; // 字体族名 (UTF-8)
    uint16_t ascender;                      // 基线以上高度
    uint16_t descender;                     // 基线以下深度
    uint8_t  reserved[50];                  // 保留
} font_header_t;

/** Glyph 条目 (16 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t unicode;                       // Unicode 码点
    uint8_t  width;                         // 位图像素宽
    uint8_t  height;                        // 位图像素高
    uint8_t  advance_x;                     // 前进宽度
    int8_t   x_offset;                      // 水平偏移
    int8_t   y_offset;                      // 垂直偏移
    uint8_t  bitmap_offset[3];              // 位图数据文件偏移 (低24位, LE)
    uint32_t bitmap_size;                   // RLE 编码后大小
} font_glyph_entry_t;

/** HashMap 桶条目 */
typedef struct font_hash_entry {
    uint32_t            unicode;            // key
    font_glyph_entry_t  glyph;             // value
    struct font_hash_entry *next;           // 链式冲突解决
} font_hash_entry_t;

/** HashMap */
typedef struct {
    font_hash_entry_t **buckets;
    uint32_t            bucket_count;
    uint32_t            entry_count;
} font_hashmap_t;

/** 已解码 glyph bitmap (缓存中存储的格式) */
typedef struct {
    uint32_t unicode;
    uint8_t  width;
    uint8_t  height;
    uint8_t  advance_x;
    int8_t   x_offset;
    int8_t   y_offset;
    uint8_t *bitmap;        // 已解码 4bpp 数据 (packed, 每字节2像素)
    uint16_t bitmap_bytes;  // bitmap 缓冲区大小
} font_decoded_glyph_t;

/** 缩放后 glyph (8bpp 输出) */
typedef struct {
    uint32_t unicode;
    uint8_t  width;
    uint8_t  height;
    uint8_t  advance_x;
    int8_t   x_offset;
    int8_t   y_offset;
    uint8_t *bitmap;        // 8bpp 灰度数据 (每像素1字节, 0x00=黑, 0xFF=白)
    uint16_t bitmap_bytes;  // bitmap 缓冲区大小
} font_scaled_glyph_t;

// ──────────────────────────────────────────────────────────────────────
// LRU 回收池
// ──────────────────────────────────────────────────────────────────────

#define FONT_RECYCLE_POOL_MAX  1500

typedef struct font_recycle_node {
    font_decoded_glyph_t        glyph;
    uint32_t                    lru_tick;
    struct font_recycle_node   *hash_next;    // HashMap chain
    struct font_recycle_node   *lru_prev;     // LRU doubly-linked
    struct font_recycle_node   *lru_next;
} font_recycle_node_t;

typedef struct {
    font_recycle_node_t **buckets;
    uint32_t              bucket_count;
    font_recycle_node_t  *lru_head;   // 最近使用
    font_recycle_node_t  *lru_tail;   // 最久未使用
    uint32_t              count;
    uint32_t              tick;
} font_recycle_pool_t;

// ──────────────────────────────────────────────────────────────────────
// 页面缓存
// ──────────────────────────────────────────────────────────────────────

#define FONT_PAGE_CACHE_WINDOW  5

/** 单个页面的 glyph 缓存 */
typedef struct {
    int32_t               page_id;     // 页面号, -1 表示未使用
    font_decoded_glyph_t *glyphs;      // 动态数组
    uint32_t              count;
    uint32_t              capacity;
} font_page_slot_t;

typedef struct {
    font_page_slot_t slots[FONT_PAGE_CACHE_WINDOW];
    int32_t          center_page;
} font_page_cache_t;

// ──────────────────────────────────────────────────────────────────────
// 常用字缓存
// ──────────────────────────────────────────────────────────────────────

typedef struct {
    font_decoded_glyph_t *glyphs;
    uint32_t              count;
    font_hash_entry_t   **lookup;      // Unicode → index 快速查找
    uint32_t              bucket_count;
} font_common_cache_t;

// ──────────────────────────────────────────────────────────────────────
// 字体引擎主结构
// ──────────────────────────────────────────────────────────────────────

typedef struct font_engine_t {
    // .bin 文件信息
    font_header_t        header;
    char                 file_path[128];
    FILE                *file;          // 保持文件打开以便按需读取

    // HashMap 索引 (Unicode → GlyphEntry)
    font_hashmap_t       index;

    // 三级缓存
    font_common_cache_t  common_cache;
    font_page_cache_t    page_cache;
    font_recycle_pool_t  recycle_pool;

    bool                 loaded;
} font_engine_t;

// ──────────────────────────────────────────────────────────────────────
// 字体引擎 API
// ──────────────────────────────────────────────────────────────────────

/**
 * 加载 .bin 字体文件并构建索引和缓存。
 * @param engine    字体引擎实例
 * @param path      .bin 文件路径 (LittleFS 或 SD)
 * @return true 成功
 */
bool font_engine_load(font_engine_t *engine, const char *path);

/**
 * 卸载字体，释放所有缓存和索引内存。
 */
void font_engine_unload(font_engine_t *engine);

/**
 * 查找 glyph (走缓存链路)。
 * 返回的指针在下次翻页或缓存淘汰前有效。
 *
 * @param engine    字体引擎
 * @param unicode   Unicode 码点
 * @return 已解码 glyph，或 NULL (未找到)
 */
const font_decoded_glyph_t *font_engine_get_glyph(font_engine_t *engine, uint32_t unicode);

/**
 * 获取缩放后的 glyph (8bpp 输出)。
 * 如果 target_size == font_height，直接转换为 8bpp 返回。
 *
 * @param engine      字体引擎
 * @param unicode     Unicode 码点
 * @param target_size 目标字号 (16-32)
 * @param out         输出缩放后的 glyph 信息
 * @return true 成功
 */
bool font_engine_get_scaled_glyph(font_engine_t *engine, uint32_t unicode,
                                  uint8_t target_size, font_scaled_glyph_t *out);

/**
 * 通知字体引擎当前阅读页面变化，触发页面缓存滑窗。
 */
void font_engine_set_page(font_engine_t *engine, int32_t page_id);

/**
 * 将 glyph 添加到当前页面缓存。
 * 通常在排版阶段调用，记录每页用到的 glyph。
 */
void font_engine_page_cache_add(font_engine_t *engine, int32_t page_id, uint32_t unicode);

// ──────────────────────────────────────────────────────────────────────
// 缓存初始化 API
// ──────────────────────────────────────────────────────────────────────

/**
 * 初始化常用字缓存（ASCII + CJK 标点），应在 font_engine_load 后调用。
 */
bool font_cache_init_common(font_engine_t *engine);

/**
 * 初始化回收池（LRU, 1500 上限）。
 */
bool font_cache_init_recycle_pool(font_engine_t *engine);

/**
 * 将 glyph 加入回收池（bitmap 所有权转移）。
 */
void font_cache_recycle_add(font_engine_t *engine, const font_decoded_glyph_t *glyph);

// ──────────────────────────────────────────────────────────────────────
// RLE 解码 API (内部使用，但公开以便测试)
// ──────────────────────────────────────────────────────────────────────

/**
 * RLE 解码 → 4bpp packed bitmap。
 * @param rle_data   RLE 编码数据
 * @param rle_size   RLE 数据大小
 * @param width      glyph 宽度
 * @param height     glyph 高度
 * @param out_buf    输出缓冲区 (调用者分配, 大小 = ceil(width/2) * height)
 * @param out_size   输出缓冲区大小
 * @return 解码的像素数 (应等于 width * height)
 */
int font_rle_decode(const uint8_t *rle_data, uint32_t rle_size,
                    uint8_t width, uint8_t height,
                    uint8_t *out_buf, uint32_t out_size);

// ──────────────────────────────────────────────────────────────────────
// 缩放 API (内部使用，但公开以便测试)
// ──────────────────────────────────────────────────────────────────────

/**
 * 面积加权缩放：4bpp packed → 8bpp。
 * @param src       源 4bpp packed bitmap (每字节2像素)
 * @param src_w     源宽度
 * @param src_h     源高度
 * @param dst       目标 8bpp buffer (调用者分配, 大小 = dst_w * dst_h)
 * @param dst_w     目标宽度
 * @param dst_h     目标高度
 */
void font_scale_area_weighted(const uint8_t *src, uint8_t src_w, uint8_t src_h,
                              uint8_t *dst, uint8_t dst_w, uint8_t dst_h);

#ifdef __cplusplus
}
#endif
