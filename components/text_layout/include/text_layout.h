/**
 * @file text_layout.h
 * @brief 文本排版引擎。
 *
 * 实现 UTF-8 文本排版、中英文自动换行和渲染到 framebuffer。
 * 依赖 font_manager 获取字形数据，依赖 ui_canvas 绘制像素。
 */

#ifndef TEXT_LAYOUT_H
#define TEXT_LAYOUT_H

#include <stdint.h>
#include <stddef.h>
#include "font_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 排版参数。 */
typedef struct {
    int font_size;      /**< 字号（像素高度），默认 24。 */
    int line_height;    /**< 行高（像素），默认 font_size * 1.5。0 表示自动计算。 */
    int margin_top;     /**< 上边距（像素），默认 30。 */
    int margin_bottom;  /**< 下边距（像素），默认 30。 */
    int margin_left;    /**< 左边距（像素），默认 30。 */
    int margin_right;   /**< 右边距（像素），默认 30。 */
} text_layout_params_t;

/**
 * @brief 获取默认排版参数。
 *
 * @return 默认参数（字号 24px，行距 1.5 倍，边距 30px）。
 */
text_layout_params_t text_layout_default_params(void);

/**
 * @brief 渲染一页文本到 framebuffer。
 *
 * 从 text 开始排版，渲染到 framebuffer 的内容区域内（由边距定义），
 * 在超出页面高度时停止。
 *
 * @param fb     framebuffer 指针。
 * @param font   字体句柄。
 * @param text   UTF-8 文本数据。
 * @param len    文本长度（字节）。
 * @param params 排版参数。
 * @return 已渲染的文本字节数（即下一页的起始偏移）。
 */
size_t text_layout_render_page(uint8_t *fb, font_handle_t font,
                                const char *text, size_t len,
                                const text_layout_params_t *params);

/**
 * @brief 计算一页能容纳的文本字节数。
 *
 * 执行排版计算但不渲染，返回该页结束位置在文本中的字节偏移。
 *
 * @param font   字体句柄。
 * @param text   UTF-8 文本数据。
 * @param len    文本长度（字节）。
 * @param params 排版参数。
 * @return 该页能容纳的文本字节数。
 */
size_t text_layout_calc_page_end(font_handle_t font,
                                  const char *text, size_t len,
                                  const text_layout_params_t *params);

/**
 * @brief 渲染单行文本到 framebuffer（不换行）。
 *
 * 适用于书架页面等简短文字渲染场景。
 * 超出 max_width 时截断。
 *
 * @param fb        framebuffer 指针。
 * @param font      字体句柄。
 * @param text      UTF-8 文本。
 * @param len       文本长度（字节）。
 * @param x         起始 X 坐标。
 * @param y         基线 Y 坐标。
 * @param max_width 最大宽度（像素），0 表示不限。
 * @param font_size 字号（像素高度）。
 * @return 渲染的总宽度（像素）。
 */
int text_layout_render_line(uint8_t *fb, font_handle_t font,
                             const char *text, size_t len,
                             int x, int y, int max_width, int font_size);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_LAYOUT_H */
