/**
 * @file ui_event.h
 * @brief 统一事件系统。
 *
 * 定义事件类型和数据结构，提供基于 FreeRTOS 队列的事件发送和接收接口。
 */

#ifndef UI_EVENT_H
#define UI_EVENT_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 事件类型枚举。 */
typedef enum {
    UI_EVENT_TOUCH_TAP,          /**< 点击 */
    UI_EVENT_TOUCH_SWIPE_LEFT,   /**< 左滑 */
    UI_EVENT_TOUCH_SWIPE_RIGHT,  /**< 右滑 */
    UI_EVENT_TOUCH_SWIPE_UP,     /**< 上滑 */
    UI_EVENT_TOUCH_SWIPE_DOWN,   /**< 下滑 */
    UI_EVENT_TOUCH_LONG_PRESS,   /**< 长按 */
    UI_EVENT_TIMER,              /**< 定时器 */
    UI_EVENT_CUSTOM,             /**< 自定义 */
} ui_event_type_t;

/** 事件数据结构。 */
typedef struct {
    ui_event_type_t type;  /**< 事件类型 */
    int16_t x;             /**< 触摸 X 坐标（竖屏逻辑坐标） */
    int16_t y;             /**< 触摸 Y 坐标（竖屏逻辑坐标） */
    union {
        int32_t param;     /**< 整型附加参数 */
        void *ptr;         /**< 指针附加参数 */
    } data;
} ui_event_t;

/**
 * @brief 初始化事件系统。
 *
 * 创建容量为 16 的 FreeRTOS 事件队列。重复调用安全。
 *
 * @return ESP_OK 成功。
 */
esp_err_t ui_event_init(void);

/**
 * @brief 发送事件到队列。
 *
 * 非阻塞发送，队列满时返回 ESP_ERR_TIMEOUT。
 *
 * @param event 事件指针。
 * @return ESP_OK 成功，ESP_ERR_TIMEOUT 队列满。
 */
esp_err_t ui_event_send(const ui_event_t *event);

/**
 * @brief 从队列接收事件。
 *
 * 阻塞等待直到有事件或超时。
 *
 * @param[out] event 输出事件数据。
 * @param timeout_ms 超时时间（毫秒），0 表示不等待。
 * @return ESP_OK 收到事件，ESP_ERR_TIMEOUT 超时。
 */
esp_err_t ui_event_receive(ui_event_t *event, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_EVENT_H */
