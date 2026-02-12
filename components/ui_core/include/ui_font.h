/**
 * @file ui_font.h
 * @brief 字体资源管理 API。
 *
 * 所有字体统一从 LittleFS .pfnt 文件加载到 PSRAM。
 * UI 字体（20/28px）在 boot 时常驻加载，阅读字体按需加载。
 * 对调用方透明——ui_font_get() 自动路由。
 */

#ifndef UI_FONT_H
#define UI_FONT_H

#include "epdiy.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化字体子系统。
 *
 * 挂载 LittleFS fonts 分区到 /fonts，扫描所有 .pfnt 字体文件，
 * 并将 UI 字体（ui_font_*.pfnt）常驻加载到 PSRAM。
 * 必须在 ui_font_get() 之前调用。
 */
void ui_font_init(void);

/**
 * @brief 按像素大小获取最近匹配的字体。
 *
 * - 20px / 28px：返回 boot 时常驻加载的 UI 字体（PSRAM）。
 * - 其他字号：从 LittleFS 加载最近匹配的阅读字体到 PSRAM。
 *   同一时间只有一个阅读字体处于加载状态；切换字号时自动卸载旧字体。
 * - 若无匹配的阅读字体，fallback 到最近的 UI 字体。
 *
 * @param size_px 期望的像素大小。
 * @return 匹配的 EpdFont 指针（PSRAM），无字体可用时返回 NULL。
 */
const EpdFont *ui_font_get(int size_px);

/**
 * @brief 列出 LittleFS 中可用的阅读字体字号。
 *
 * @param[out] sizes_out 输出字号数组。
 * @param max_count 数组最大容量。
 * @return 实际可用字号数量。
 */
int ui_font_list_available(int *sizes_out, int max_count);

#ifdef __cplusplus
}
#endif

#endif /* UI_FONT_H */
