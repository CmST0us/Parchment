/**
 * @file ui_core.h
 * @brief Parchment 轻量 UI 框架统一入口。
 *
 * 包含所有子模块头文件，提供 UI 框架初始化和主循环接口。
 */

#ifndef UI_CORE_H
#define UI_CORE_H

#include "ui_event.h"
#include "ui_page.h"
#include "ui_canvas.h"
#include "ui_touch.h"
#include "ui_render.h"

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 UI 框架。
 *
 * 按顺序初始化事件系统、页面栈和渲染状态。
 *
 * @return ESP_OK 成功。
 */
esp_err_t ui_core_init(void);

/**
 * @brief 启动 UI 主循环任务。
 *
 * 创建 FreeRTOS 任务运行事件循环：等待事件 → 分发给当前页面 → 按需渲染刷屏。
 *
 * @return ESP_OK 成功。
 */
esp_err_t ui_core_run(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_CORE_H */
