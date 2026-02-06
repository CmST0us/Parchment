/**
 * @file ui_touch.h
 * @brief 触摸手势识别。
 *
 * 中断驱动的触摸采集任务，将 GT911 原始触摸数据转换为语义手势事件
 * （点击、滑动、长按），通过事件队列发送给 UI 主循环。
 */

#ifndef UI_TOUCH_H
#define UI_TOUCH_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动触摸采集任务。
 *
 * 配置 GT911 INT 引脚下降沿中断，创建触摸任务。
 * 无触摸时任务阻塞在信号量上（低功耗）；按下期间 20ms 轮询追踪。
 *
 * @param int_gpio GT911 INT 引脚编号。
 * @return ESP_OK 成功。
 */
esp_err_t ui_touch_start(int int_gpio);

/**
 * @brief 停止触摸采集任务。
 *
 * 禁用 INT 中断并销毁触摸任务。
 */
void ui_touch_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_TOUCH_H */
