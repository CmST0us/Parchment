/**
 * @file ui_touch.c
 * @brief 中断驱动的触摸采集与手势识别实现。
 *
 * GT911 INT 引脚下降沿中断唤醒触摸任务，任务读取触摸数据并通过
 * 简单状态机识别手势（点击、滑动、长按），发送事件到 UI 事件队列。
 */

#include "ui_touch.h"
#include "ui_event.h"
#include "ui_canvas.h"
#include "gt911.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ui_touch";

/* -------------------------------------------------------------------------- */
/*  手势识别参数                                                                */
/* -------------------------------------------------------------------------- */
#define GESTURE_TAP_MAX_DIST     20   /**< 点击最大移动距离 (px) */
#define GESTURE_TAP_MAX_TIME_MS  500  /**< 点击最大持续时间 (ms) */
#define GESTURE_SWIPE_MIN_DIST   40   /**< 滑动最小移动距离 (px) */
#define GESTURE_LONG_PRESS_MS    800  /**< 长按触发时间 (ms) */
#define GESTURE_LONG_PRESS_DIST  20   /**< 长按最大移动距离 (px) */

#define TOUCH_POLL_INTERVAL_MS   20   /**< 按下期间轮询间隔 (ms) */
#define TOUCH_TASK_STACK_SIZE    4096
#define TOUCH_TASK_PRIORITY      6

/* -------------------------------------------------------------------------- */
/*  内部状态                                                                    */
/* -------------------------------------------------------------------------- */

/** 手势识别状态。 */
typedef enum {
    GESTURE_IDLE,       /**< 等待触摸 */
    GESTURE_TRACKING,   /**< 手指按下，追踪中 */
    GESTURE_LONG_FIRED, /**< 长按已触发，等待释放 */
} gesture_state_t;

/** 触摸追踪数据。 */
typedef struct {
    int16_t start_x;      /**< 按下位置 X (逻辑坐标) */
    int16_t start_y;      /**< 按下位置 Y (逻辑坐标) */
    int16_t current_x;    /**< 当前位置 X */
    int16_t current_y;    /**< 当前位置 Y */
    int64_t start_time_us; /**< 按下时间 (微秒) */
} touch_track_t;

static SemaphoreHandle_t s_touch_sem = NULL;
static TaskHandle_t s_touch_task = NULL;
static volatile bool s_running = false;
static gesture_state_t s_gesture_state = GESTURE_IDLE;
static touch_track_t s_track;
static int s_int_gpio = -1;  /**< INT 引脚编号 */

/* -------------------------------------------------------------------------- */
/*  坐标映射                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief GT911 坐标直通（无转换）。
 *
 * GT911 报告的坐标直接作为逻辑屏幕坐标，裁剪到有效范围。
 */
static void touch_to_screen(uint16_t tx, uint16_t ty,
                            int16_t *sx, int16_t *sy) {
    *sx = (int16_t)tx;
    *sy = (int16_t)ty;

    if (*sx < 0) *sx = 0;
    if (*sx >= UI_SCREEN_WIDTH) *sx = UI_SCREEN_WIDTH - 1;
    if (*sy < 0) *sy = 0;
    if (*sy >= UI_SCREEN_HEIGHT) *sy = UI_SCREEN_HEIGHT - 1;
}

/* -------------------------------------------------------------------------- */
/*  中断处理                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief GT911 INT 引脚中断服务程序。
 */
static void IRAM_ATTR touch_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_touch_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/* -------------------------------------------------------------------------- */
/*  辅助函数                                                                    */
/* -------------------------------------------------------------------------- */

/** 计算两点距离的平方（避免 sqrt）。 */
static inline int32_t distance_sq(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    return dx * dx + dy * dy;
}

/** 发送手势事件。 */
static void send_gesture_event(ui_event_type_t type, int16_t x, int16_t y) {
    ui_event_t evt = {
        .type = type,
        .x = x,
        .y = y,
        .data.param = 0,
    };
    ui_event_send(&evt);
    ESP_LOGD(TAG, "Gesture event: type=%d at (%d, %d)", type, x, y);
}

/* -------------------------------------------------------------------------- */
/*  手势状态机                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief 处理手指按下事件。
 */
static void gesture_on_press(int16_t x, int16_t y) {
    s_gesture_state = GESTURE_TRACKING;
    s_track.start_x = x;
    s_track.start_y = y;
    s_track.current_x = x;
    s_track.current_y = y;
    s_track.start_time_us = esp_timer_get_time();
}

/**
 * @brief 处理手指移动事件。
 */
static void gesture_on_move(int16_t x, int16_t y) {
    s_track.current_x = x;
    s_track.current_y = y;

    if (s_gesture_state == GESTURE_TRACKING) {
        /* 检查长按 */
        int64_t elapsed_us = esp_timer_get_time() - s_track.start_time_us;
        int32_t dist_sq = distance_sq(s_track.start_x, s_track.start_y, x, y);

        if (elapsed_us >= (int64_t)GESTURE_LONG_PRESS_MS * 1000 &&
            dist_sq < GESTURE_LONG_PRESS_DIST * GESTURE_LONG_PRESS_DIST) {
            send_gesture_event(UI_EVENT_TOUCH_LONG_PRESS,
                               s_track.start_x, s_track.start_y);
            s_gesture_state = GESTURE_LONG_FIRED;
        }
    }
}

/**
 * @brief 处理手指释放事件。
 */
static void gesture_on_release(void) {
    if (s_gesture_state == GESTURE_LONG_FIRED) {
        /* 长按已触发，释放后不再产生其他手势 */
        s_gesture_state = GESTURE_IDLE;
        return;
    }

    if (s_gesture_state != GESTURE_TRACKING) {
        s_gesture_state = GESTURE_IDLE;
        return;
    }

    int64_t elapsed_us = esp_timer_get_time() - s_track.start_time_us;
    int32_t elapsed_ms = (int32_t)(elapsed_us / 1000);
    int32_t dx = s_track.current_x - s_track.start_x;
    int32_t dy = s_track.current_y - s_track.start_y;
    int32_t dist_sq = dx * dx + dy * dy;

    if (dist_sq >= GESTURE_SWIPE_MIN_DIST * GESTURE_SWIPE_MIN_DIST) {
        /* 滑动：根据主方向判断 */
        int32_t abs_dx = dx < 0 ? -dx : dx;
        int32_t abs_dy = dy < 0 ? -dy : dy;

        if (abs_dx >= abs_dy) {
            /* 水平滑动 */
            send_gesture_event(dx < 0 ? UI_EVENT_TOUCH_SWIPE_LEFT : UI_EVENT_TOUCH_SWIPE_RIGHT,
                               s_track.start_x, s_track.start_y);
        } else {
            /* 垂直滑动 */
            send_gesture_event(dy < 0 ? UI_EVENT_TOUCH_SWIPE_UP : UI_EVENT_TOUCH_SWIPE_DOWN,
                               s_track.start_x, s_track.start_y);
        }
    } else if (dist_sq < GESTURE_TAP_MAX_DIST * GESTURE_TAP_MAX_DIST &&
               elapsed_ms < GESTURE_TAP_MAX_TIME_MS) {
        /* 点击 */
        send_gesture_event(UI_EVENT_TOUCH_TAP, s_track.start_x, s_track.start_y);
    }
    /* 其他情况（移动不够滑动，时间太长不算点击）不产生事件 */

    s_gesture_state = GESTURE_IDLE;
}

/* -------------------------------------------------------------------------- */
/*  触摸任务                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief 触摸采集任务主函数。
 */
static void touch_task(void *arg) {
    gt911_touch_point_t point;
    bool was_pressed = false;

    while (s_running) {
        if (s_gesture_state == GESTURE_IDLE) {
            /* 空闲：等待 INT 中断信号量 */
            xSemaphoreTake(s_touch_sem, pdMS_TO_TICKS(1000));
        } else {
            /* 追踪中：20ms 轮询 */
            vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_INTERVAL_MS));
        }

        if (!s_running) {
            break;
        }

        /* 读取触摸数据 */
        if (gt911_read(&point) != ESP_OK) {
            continue;
        }

        if (point.pressed) {
            int16_t lx, ly;
            touch_to_screen(point.x, point.y, &lx, &ly);

            if (!was_pressed) {
                gesture_on_press(lx, ly);
            } else {
                gesture_on_move(lx, ly);
            }
            was_pressed = true;
        } else {
            if (was_pressed) {
                gesture_on_release();
                was_pressed = false;
            }
        }
    }

    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*  公共 API                                                                    */
/* -------------------------------------------------------------------------- */

esp_err_t ui_touch_start(int int_gpio) {
    if (s_running) {
        ESP_LOGW(TAG, "Touch task already running");
        return ESP_OK;
    }

    s_int_gpio = int_gpio;

    /* 创建信号量 */
    if (s_touch_sem == NULL) {
        s_touch_sem = xSemaphoreCreateBinary();
        if (s_touch_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create touch semaphore");
            return ESP_ERR_NO_MEM;
        }
    }

    /* 配置 INT 引脚下降沿中断 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_int_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(s_int_gpio, touch_isr_handler, NULL);

    /* 启动触摸任务 */
    s_running = true;
    s_gesture_state = GESTURE_IDLE;
    BaseType_t ret = xTaskCreate(touch_task, "touch_task",
                                 TOUCH_TASK_STACK_SIZE, NULL,
                                 TOUCH_TASK_PRIORITY, &s_touch_task);
    if (ret != pdPASS) {
        s_running = false;
        ESP_LOGE(TAG, "Failed to create touch task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Touch task started (interrupt-driven, INT=GPIO%d)", s_int_gpio);
    return ESP_OK;
}

void ui_touch_stop(void) {
    if (!s_running) {
        return;
    }

    s_running = false;

    /* 释放信号量使任务退出等待 */
    if (s_touch_sem != NULL) {
        xSemaphoreGive(s_touch_sem);
    }

    /* 等待任务退出 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 禁用中断 */
    gpio_isr_handler_remove(s_int_gpio);
    gpio_intr_disable(s_int_gpio);

    s_touch_task = NULL;
    ESP_LOGI(TAG, "Touch task stopped");
}
