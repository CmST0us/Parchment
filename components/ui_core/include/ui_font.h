/**
 * @file ui_font.h
 * @brief 预编译字体资源声明和字号选择 API。
 *
 * 提供 Noto Sans CJK SC 字体的 6 个预编译字号（20/24/28/32/36/40px），
 * 以及按像素大小选择最近匹配字号的辅助函数。
 */

#ifndef UI_FONT_H
#define UI_FONT_H

#include "epdiy.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 可用字号列表。 */
#define UI_FONT_SIZE_MIN 20
#define UI_FONT_SIZE_MAX 40

/** 预编译字体声明。 */
extern const EpdFont noto_sans_cjk_20;
extern const EpdFont noto_sans_cjk_24;
extern const EpdFont noto_sans_cjk_28;
extern const EpdFont noto_sans_cjk_32;
extern const EpdFont noto_sans_cjk_36;
extern const EpdFont noto_sans_cjk_40;

/**
 * @brief 按像素大小获取最近匹配的字体。
 *
 * 向下取整到最近的可用字号。超出范围时钳位到 20 或 40。
 *
 * @param size_px 期望的像素大小。
 * @return 匹配的 EpdFont 指针。
 */
const EpdFont *ui_font_get(int size_px);

#ifdef __cplusplus
}
#endif

#endif /* UI_FONT_H */
