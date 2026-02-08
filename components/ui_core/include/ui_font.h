/**
 * @file ui_font.h
 * @brief 字体渲染 API。
 *
 * 从 SD 卡加载预渲染的 .bin 格式 bitmap 字体，提供字符/字符串渲染和文本测量功能。
 * 字体文件格式兼容 M5ReadPaper V2 (1-bit packed bitmap)。
 */

#ifndef UI_FONT_H
#define UI_FONT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从 SD 卡加载 .bin 字体文件。
 *
 * 解析 header 和 glyph table，在 PSRAM 中构建 hash map 索引。
 * 若已有字体加载，会先卸载再加载新字体。
 *
 * @param path 字体文件路径（如 "/sdcard/fonts/noto_serif_24.bin"）。
 * @return ESP_OK 成功，ESP_ERR_NOT_FOUND 文件不存在，ESP_ERR_INVALID_ARG 格式无效。
 */
esp_err_t ui_font_load(const char *path);

/**
 * @brief 卸载当前字体，释放所有资源。
 *
 * 释放 PSRAM 中的 glyph 索引内存并关闭文件句柄。
 * 未加载字体时调用安全返回。
 */
void ui_font_unload(void);

/**
 * @brief 渲染单个字符到 framebuffer。
 *
 * @param fb        framebuffer 指针。
 * @param x         左上角 X 坐标（baseline 左侧）。
 * @param y         左上角 Y 坐标（字符顶部）。
 * @param codepoint Unicode 码位。
 * @param color     前景色灰度值。
 * @return 该字符的 advance width（步进宽度），未加载字体返回 0。
 */
int ui_font_draw_char(uint8_t *fb, int x, int y, uint32_t codepoint, uint8_t color);

/**
 * @brief 渲染 UTF-8 文本字符串，支持自动换行。
 *
 * CJK 字符逐字可断行，ASCII 单词尽量不拆分。
 *
 * @param fb          framebuffer 指针。
 * @param x           起始 X 坐标。
 * @param y           起始 Y 坐标。
 * @param max_w       最大行宽（像素）。
 * @param line_height 行高（像素）。
 * @param utf8_text   UTF-8 编码的文本。
 * @param color       前景色灰度值。
 * @return 渲染所占用的总高度（像素）。
 */
int ui_font_draw_text(uint8_t *fb, int x, int y, int max_w, int line_height,
                      const char *utf8_text, uint8_t color);

/**
 * @brief 测量文本渲染后的像素宽度（不实际渲染）。
 *
 * @param utf8_text UTF-8 编码的文本。
 * @param max_chars 最大测量字符数，0 表示不限制。
 * @return 文本的像素宽度。
 */
int ui_font_measure_text(const char *utf8_text, int max_chars);

/**
 * @brief 获取当前加载字体的高度。
 *
 * @return 字体高度（像素），未加载返回 0。
 */
int ui_font_get_height(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_FONT_H */
