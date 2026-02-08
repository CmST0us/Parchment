/**
 * @file ui_render.h
 * @brief 渲染管线：脏区域追踪和按需刷新。
 *
 * 管理脏区域（bounding box 合并），根据区域大小选择局部或全屏刷新。
 */

#ifndef UI_RENDER_H
#define UI_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化渲染状态。
 */
void ui_render_init(void);

/**
 * @brief 标记脏区域。
 *
 * 多次调用将合并为一个包围矩形（bounding box）。
 *
 * @param x 左上角 X 坐标（逻辑坐标）。
 * @param y 左上角 Y 坐标（逻辑坐标）。
 * @param w 宽度。
 * @param h 高度。
 */
void ui_render_mark_dirty(int x, int y, int w, int h);

/**
 * @brief 将整个屏幕标记为脏区域，强制全屏刷新。
 */
void ui_render_mark_full_dirty(void);

/**
 * @brief 请求在下次渲染前执行全屏清除（消除残影）。
 *
 * 会在主循环中 on_render 之前调用 epd_driver_clear()，
 * 多次闪烁清除屏幕残影，然后再绘制新内容。
 */
void ui_render_request_clear(void);

/**
 * @brief 查询是否有待刷新区域。
 *
 * @return true 有脏区域，false 无。
 */
bool ui_render_is_dirty(void);

/**
 * @brief 若有待处理的全屏清除请求，立即执行。
 *
 * 由主循环在 on_render 之前调用，确保清屏后再绘制内容。
 */
void ui_render_clear_if_pending(void);

/**
 * @brief 执行屏幕刷新。
 *
 * 脏区域面积超过屏幕 60% 时使用全屏刷新，否则局部刷新。
 * 刷新后清除脏区域状态。
 */
void ui_render_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_RENDER_H */
