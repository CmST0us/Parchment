/**
 * @file font_manager.h
 * @brief 字体管理器。
 *
 * 基于 stb_truetype 实现 TTF/TTC 字体加载和字形渲染，
 * 支持从 LittleFS 和 SD 卡双源扫描字体，PSRAM LRU 字形缓存。
 */

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 字体扫描结果的最大条目数。 */
#define FONT_MAX_SCAN_ENTRIES 16

/** 字体路径最大长度。 */
#define FONT_PATH_MAX 256

/** 字体句柄（不透明类型）。 */
typedef struct font_handle *font_handle_t;

/** 字形位图信息。 */
typedef struct {
    uint8_t *bitmap;    /**< 4bpp 灰度位图数据（高 4 位有效），调用方不可释放。 */
    int      width;     /**< 位图宽度（像素）。 */
    int      height;    /**< 位图高度（像素）。 */
    int      offset_x;  /**< 字形左侧偏移（像素）。 */
    int      offset_y;  /**< 字形顶部偏移（相对基线，向上为正）。 */
    int      advance_x; /**< 水平步进宽度（像素）。 */
} font_glyph_t;

/** 字体扫描结果列表。 */
typedef struct {
    int  count;                              /**< 扫描到的字体数量。 */
    char paths[FONT_MAX_SCAN_ENTRIES][FONT_PATH_MAX]; /**< 字体文件路径列表。 */
} font_scan_result_t;

/**
 * @brief 初始化字体管理器。
 *
 * 初始化字形缓存并加载默认字体（/littlefs/wqy-microhei.ttc）。
 *
 * @return ESP_OK 成功。
 */
esp_err_t font_manager_init(void);

/**
 * @brief 销毁字体管理器。
 *
 * 释放所有已加载字体和缓存。
 */
void font_manager_deinit(void);

/**
 * @brief 从文件加载字体。
 *
 * 将整个 TTF/TTC 文件读入 PSRAM 并初始化 stbtt_fontinfo。
 * TTC 文件默认加载第一个字面（index 0）。
 *
 * @param path 字体文件路径。
 * @return 字体句柄，失败返回 NULL。
 */
font_handle_t font_manager_load(const char *path);

/**
 * @brief 释放字体。
 *
 * @param font 字体句柄。
 */
void font_manager_unload(font_handle_t font);

/**
 * @brief 获取默认字体。
 *
 * @return 默认字体句柄，未加载时返回 NULL。
 */
font_handle_t font_manager_get_default(void);

/**
 * @brief 扫描可用字体。
 *
 * 扫描 LittleFS (/littlefs/) 和 SD 卡 (/sdcard/fonts/) 目录下
 * 所有 .ttf 和 .ttc 文件。
 *
 * @param[out] result 扫描结果。
 */
void font_manager_scan(font_scan_result_t *result);

/**
 * @brief 获取字形位图。
 *
 * 渲染指定 Unicode 码点的字形为 4bpp 灰度位图。
 * 优先从缓存获取，缓存未命中时通过 stb_truetype 光栅化。
 *
 * @param font      字体句柄。
 * @param codepoint Unicode 码点。
 * @param size_px   字号（像素高度）。
 * @param[out] glyph 字形信息输出。
 * @return ESP_OK 成功，ESP_ERR_NOT_FOUND 字形不存在。
 */
esp_err_t font_manager_get_glyph(font_handle_t font, uint32_t codepoint,
                                  int size_px, font_glyph_t *glyph);

/**
 * @brief 获取字体行距信息。
 *
 * @param font    字体句柄。
 * @param size_px 字号（像素高度）。
 * @param[out] ascent  基线上方高度。
 * @param[out] descent 基线下方深度（正值）。
 * @param[out] line_gap 行间距。
 */
void font_manager_get_metrics(font_handle_t font, int size_px,
                               int *ascent, int *descent, int *line_gap);

#ifdef __cplusplus
}
#endif

#endif /* FONT_MANAGER_H */
