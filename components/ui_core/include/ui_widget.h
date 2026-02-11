/**
 * @file ui_widget.h
 * @brief 通用 UI Widget 组件层。
 *
 * 提供 header、button、progress、list、separator 五种可复用组件的
 * 绘制和命中检测 API。所有 widget 采用纯函数式接口，不持有内部状态。
 */

#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_icon.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Header ─────────────────────────────────────────────────────── */

/** Header 固定高度（像素）。 */
#define UI_HEADER_HEIGHT 48

/** Header 命中检测返回值。 */
#define UI_HEADER_HIT_NONE   0  /**< 未命中或空白区域。 */
#define UI_HEADER_HIT_LEFT   1  /**< 命中左侧图标区域。 */
#define UI_HEADER_HIT_RIGHT  2  /**< 命中右侧图标区域。 */

/** Header 描述结构体。 */
typedef struct {
    const char *title;            /**< 标题文字（UTF-8）。 */
    const ui_icon_t *icon_left;   /**< 左侧图标，可为 NULL。 */
    const ui_icon_t *icon_right;  /**< 右侧图标，可为 NULL。 */
} ui_header_t;

/**
 * @brief 绘制 Header 标题栏。
 *
 * 固定绘制在 (0,0)，尺寸 540×48，BLACK 底色。
 * 标题文字 24px WHITE 色居中，左右图标 WHITE 色。
 *
 * @param fb     framebuffer 指针。
 * @param header Header 描述结构体指针。
 */
void ui_widget_draw_header(uint8_t *fb, const ui_header_t *header);

/**
 * @brief Header 命中检测。
 *
 * @param header Header 描述结构体指针。
 * @param x      触摸 X 坐标。
 * @param y      触摸 Y 坐标。
 * @return UI_HEADER_HIT_LEFT / UI_HEADER_HIT_RIGHT / UI_HEADER_HIT_NONE。
 */
int ui_widget_header_hit_test(const ui_header_t *header, int x, int y);

/* ── Button ─────────────────────────────────────────────────────── */

/** 按钮样式枚举。 */
typedef enum {
    UI_BTN_PRIMARY,    /**< BLACK 底 WHITE 字。 */
    UI_BTN_SECONDARY,  /**< WHITE 底 BLACK 字 BLACK 边框 (2px)。 */
    UI_BTN_ICON,       /**< 透明底，仅绘制图标。 */
    UI_BTN_SELECTED,   /**< LIGHT 底 BLACK 字。 */
} ui_button_style_t;

/** Button 描述结构体。 */
typedef struct {
    int x, y, w, h;             /**< 按钮位置和尺寸。 */
    const char *label;          /**< 文字标签，可为 NULL。 */
    const ui_icon_t *icon;      /**< 图标，可为 NULL。 */
    ui_button_style_t style;    /**< 按钮样式。 */
} ui_button_t;

/**
 * @brief 绘制按钮。
 *
 * 根据 style 绘制不同样式，支持纯文字、纯图标、图标+文字组合。
 *
 * @param fb  framebuffer 指针。
 * @param btn Button 描述结构体指针。
 */
void ui_widget_draw_button(uint8_t *fb, const ui_button_t *btn);

/**
 * @brief Button 命中检测。
 *
 * @param btn Button 描述结构体指针。
 * @param x   触摸 X 坐标。
 * @param y   触摸 Y 坐标。
 * @return true 命中按钮区域，false 未命中。
 */
bool ui_widget_button_hit_test(const ui_button_t *btn, int x, int y);

/* ── Progress ───────────────────────────────────────────────────── */

/** Progress 描述结构体。 */
typedef struct {
    int x, y, w, h;    /**< 进度条位置和尺寸。 */
    int value;          /**< 进度值 (0-100)。 */
    bool show_label;    /**< 是否在右侧显示百分比文字。 */
} ui_progress_t;

/**
 * @brief 绘制进度条。
 *
 * 已完成区域 BLACK，未完成区域 LIGHT。
 * show_label 为 true 时在右侧 8px 处显示百分比文字。
 *
 * @param fb   framebuffer 指针。
 * @param prog Progress 描述结构体指针。
 */
void ui_widget_draw_progress(uint8_t *fb, const ui_progress_t *prog);

/**
 * @brief Progress 触摸映射。
 *
 * 在 y 方向 ±20px 扩展触摸区内，将 x 坐标映射到 0-100 值。
 *
 * @param prog Progress 描述结构体指针。
 * @param x    触摸 X 坐标。
 * @param y    触摸 Y 坐标。
 * @return 0-100 的进度值，未命中返回 -1。
 */
int ui_widget_progress_touch(const ui_progress_t *prog, int x, int y);

/* ── List ───────────────────────────────────────────────────────── */

/** 列表项绘制回调函数类型。 */
typedef void (*ui_list_draw_item_fn)(uint8_t *fb, int index,
                                     int x, int y, int w, int h);

/** List 描述结构体。 */
typedef struct {
    int x, y, w, h;                /**< 列表可视区域。 */
    int item_height;               /**< 每项高度（像素）。 */
    int item_count;                /**< 总项数。 */
    int scroll_offset;             /**< 当前滚动偏移（像素，≥0）。 */
    ui_list_draw_item_fn draw_item; /**< 绘制单项的回调函数。 */
} ui_list_t;

/**
 * @brief 绘制列表。
 *
 * 仅对可视区域内的项调用 draw_item 回调，实现虚拟化绘制。
 *
 * @param fb   framebuffer 指针。
 * @param list List 描述结构体指针。
 */
void ui_widget_draw_list(uint8_t *fb, const ui_list_t *list);

/**
 * @brief List 命中检测。
 *
 * @param list List 描述结构体指针。
 * @param x    触摸 X 坐标。
 * @param y    触摸 Y 坐标。
 * @return 命中的 item index，未命中返回 -1。
 */
int ui_widget_list_hit_test(const ui_list_t *list, int x, int y);

/**
 * @brief List 滚动。
 *
 * 更新 scroll_offset，结果 clamp 到 [0, max_scroll]。
 *
 * @param list    List 描述结构体指针（scroll_offset 会被修改）。
 * @param delta_y 滚动增量（正值向下，负值向上）。
 */
void ui_widget_list_scroll(ui_list_t *list, int delta_y);

/* ── Separator ──────────────────────────────────────────────────── */

/**
 * @brief 绘制分隔线。
 *
 * 在 (x, y) 绘制 1px 高、w 像素宽的 MEDIUM 色水平线。
 *
 * @param fb framebuffer 指针。
 * @param x  起始 X 坐标。
 * @param y  Y 坐标。
 * @param w  线宽（像素）。
 */
void ui_widget_draw_separator(uint8_t *fb, int x, int y, int w);

#ifdef __cplusplus
}
#endif

#endif /* UI_WIDGET_H */
