/**
 * @file ui_text.h
 * @brief 文本排版引擎 API。
 *
 * 提供自动换行和分页渲染功能，支持 CJK 逐字断行、英文单词断行和标点禁则。
 */

#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "epdiy.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 文本渲染结果。 */
typedef struct {
    int bytes_consumed;   /**< 本次渲染消耗的 UTF-8 字节数。 */
    int lines_rendered;   /**< 实际渲染的行数。 */
    int last_y;           /**< 最后一行的 y 坐标。 */
    bool reached_end;     /**< 是否已到达文本末尾。 */
} ui_text_result_t;

/**
 * @brief 自动换行渲染文本。
 *
 * 在指定行宽内自动断行并渲染到 framebuffer。
 *
 * @param fb         framebuffer 指针。
 * @param x          内容区左上角 X 坐标。
 * @param y          首行基线 Y 坐标。
 * @param max_width  行宽限制（像素）。
 * @param line_height 行高（像素）。
 * @param font       字体指针。
 * @param text       UTF-8 编码文本。
 * @param fg_color   前景色。
 * @return 渲染结果。
 */
ui_text_result_t ui_canvas_draw_text_wrapped(
    uint8_t *fb, int x, int y, int max_width, int line_height,
    const EpdFont *font, const char *text, uint8_t fg_color);

/**
 * @brief 分页渲染文本。
 *
 * 从 start_offset 开始渲染文本，直到填满 max_height 或到达文本末尾。
 * 调用方可用返回的 bytes_consumed 计算下一页的起始偏移。
 *
 * @param fb           framebuffer 指针。
 * @param x            内容区左上角 X 坐标。
 * @param y            首行基线 Y 坐标。
 * @param max_width    行宽限制（像素）。
 * @param max_height   页面高度限制（像素，从 y 起算）。
 * @param line_height  行高（像素）。
 * @param font         字体指针。
 * @param text         完整 UTF-8 编码文本。
 * @param start_offset 本页起始字节偏移。
 * @param fg_color     前景色。
 * @return 渲染结果。
 */
ui_text_result_t ui_canvas_draw_text_page(
    uint8_t *fb, int x, int y, int max_width, int max_height,
    int line_height, const EpdFont *font,
    const char *text, int start_offset, uint8_t fg_color);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEXT_H */
