/**
 * @file ui_icon.h
 * @brief 图标资源定义与绘制 API。
 *
 * 提供预定义的 32×32 4bpp 图标和统一的绘制接口。
 * 图标数据来自 Tabler Icons (MIT License)。
 */

#ifndef UI_ICON_H
#define UI_ICON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 图标数据结构。 */
typedef struct {
    const uint8_t *data;  /**< 4bpp alpha 位图数据。 */
    int w;                /**< 图标宽度（像素）。 */
    int h;                /**< 图标高度（像素）。 */
} ui_icon_t;

/** 预定义图标常量 (32×32)。 */
extern const ui_icon_t UI_ICON_ARROW_LEFT;     /**< ← 返回箭头。 */
extern const ui_icon_t UI_ICON_ARROW_RIGHT;    /**< → 前进箭头。 */
extern const ui_icon_t UI_ICON_MENU;           /**< ≡ 菜单。 */
extern const ui_icon_t UI_ICON_SETTINGS;       /**< 齿轮设置。 */
extern const ui_icon_t UI_ICON_BOOKMARK;       /**< 书签。 */
extern const ui_icon_t UI_ICON_LIST;           /**< 列表/目录。 */
extern const ui_icon_t UI_ICON_SORT_DESC;      /**< 降序排序。 */
extern const ui_icon_t UI_ICON_TYPOGRAPHY;     /**< Aa 字体设置。 */
extern const ui_icon_t UI_ICON_CHEVRON_RIGHT;  /**< > 右箭头。 */
extern const ui_icon_t UI_ICON_BOOK;           /**< 书本。 */

/**
 * @brief 以指定前景色绘制图标。
 *
 * @param fb    framebuffer 指针。
 * @param x     左上角 X 坐标（逻辑坐标）。
 * @param y     左上角 Y 坐标（逻辑坐标）。
 * @param icon  图标指针。
 * @param color 前景色（灰度值，高 4 位有效）。
 */
void ui_icon_draw(uint8_t *fb, int x, int y, const ui_icon_t *icon, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif /* UI_ICON_H */
