/**
 * @file ui_font_pfnt.h
 * @brief Parchment Font (.pfnt) 二进制格式定义及加载/卸载 API。
 *
 * .pfnt 文件自包含一个字体的一个字号的全部数据：header、unicode intervals、
 * glyph table 和 zlib 压缩的 4bpp bitmap。加载时将全部数据读入 PSRAM 并构建
 * 标准 EpdFont 结构体。
 */

#ifndef UI_FONT_PFNT_H
#define UI_FONT_PFNT_H

#include <stdint.h>
#include "epdiy.h"

#ifdef __cplusplus
extern "C" {
#endif

/** .pfnt 文件 magic: "PFNT" */
#define PFNT_MAGIC 0x544E4650  /* 'P','F','N','T' little-endian */

/** 当前 .pfnt 格式版本。 */
#define PFNT_VERSION 1

/** flags 位域定义。 */
#define PFNT_FLAG_COMPRESSED (1 << 0)

/**
 * @brief .pfnt 文件头结构（32 字节，与文件中的布局一一对应）。
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;           /**< 文件标识: PFNT_MAGIC */
    uint8_t  version;         /**< 格式版本 */
    uint8_t  flags;           /**< bit0: compressed */
    uint16_t font_size_px;    /**< 字号（像素） */
    uint16_t advance_y;       /**< 行高 */
    int16_t  ascender;        /**< 基线上方最大高度 */
    int16_t  descender;       /**< 基线下方最大深度 */
    uint32_t interval_count;  /**< Unicode interval 数量 */
    uint32_t glyph_count;     /**< Glyph 总数 */
    uint8_t  reserved[10];    /**< 保留字段，全零 */
} pfnt_header_t;

_Static_assert(sizeof(pfnt_header_t) == 32, "pfnt_header_t must be 32 bytes");

/**
 * @brief .pfnt 文件中的 glyph 条目（20 字节）。
 *
 * 与 EpdGlyph 字段对齐，但使用固定宽度类型以保证文件格式稳定。
 */
typedef struct __attribute__((packed)) {
    uint16_t width;
    uint16_t height;
    uint16_t advance_x;
    int16_t  left;
    int16_t  top;
    uint32_t compressed_size;
    uint32_t data_offset;
    uint16_t reserved;        /**< 保留，全零 */
} pfnt_glyph_t;

_Static_assert(sizeof(pfnt_glyph_t) == 20, "pfnt_glyph_t must be 20 bytes");

/**
 * @brief .pfnt 文件中的 unicode interval 条目（12 字节）。
 */
typedef struct __attribute__((packed)) {
    uint32_t first;
    uint32_t last;
    uint32_t glyph_offset;
} pfnt_interval_t;

_Static_assert(sizeof(pfnt_interval_t) == 12, "pfnt_interval_t must be 12 bytes");

/**
 * @brief 从 .pfnt 文件加载字体到 PSRAM。
 *
 * 读取整个文件内容，校验 magic 和版本，将 intervals、glyphs、bitmap 数据
 * 分配到 PSRAM，并构建 EpdFont 结构体。
 *
 * @param path .pfnt 文件路径（如 "/fonts/noto_cjk_24.pfnt"）。
 * @return 成功返回指向 PSRAM 中 EpdFont 的指针（需用 pfnt_unload 释放），
 *         失败返回 NULL。
 */
EpdFont *pfnt_load(const char *path);

/**
 * @brief 卸载通过 pfnt_load 加载的字体，释放全部 PSRAM 内存。
 *
 * @param font pfnt_load 返回的指针，NULL 安全。
 */
void pfnt_unload(EpdFont *font);

/**
 * @brief 仅读取 .pfnt 文件头，获取字号信息。
 *
 * 用于扫描可用字体时快速获取 font_size_px 而不加载整个文件。
 *
 * @param path .pfnt 文件路径。
 * @param[out] header 输出文件头。
 * @return 成功返回 0，失败返回 -1。
 */
int pfnt_read_header(const char *path, pfnt_header_t *header);

#ifdef __cplusplus
}
#endif

#endif /* UI_FONT_PFNT_H */
