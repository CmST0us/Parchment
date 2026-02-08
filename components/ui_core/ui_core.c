/**
 * @file ui_core.c
 * @brief UI 框架初始化和主循环。
 */

#include "ui_core.h"
#include "epd_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ui_core";

/** UI 主循环事件等待超时（毫秒）。 */
#define UI_LOOP_TIMEOUT_MS 30000

#define UI_TASK_STACK_SIZE 8192
#define UI_TASK_PRIORITY   5

/**
 * @brief UI 主循环任务函数。
 */
static void ui_loop_task(void *arg) {
    ui_event_t event;

    ESP_LOGI(TAG, "UI main loop started");

    for (;;) {
        /* 先检查脏区域并渲染（确保启动时立即刷新） */
        if (ui_render_is_dirty()) {
            ui_page_t *page = ui_page_current();
            if (page != NULL && page->on_render != NULL) {
                uint8_t *fb = epd_driver_get_framebuffer();
                if (fb != NULL) {
                    page->on_render(fb);
                }
            }
            ui_render_flush();
        }

        /* 阻塞等待事件 */
        esp_err_t ret = ui_event_receive(&event, UI_LOOP_TIMEOUT_MS);

        if (ret == ESP_OK) {
            /* 分发事件给当前页面 */
            ui_page_t *page = ui_page_current();
            if (page != NULL && page->on_event != NULL) {
                page->on_event(&event);
            }
        }
    }
}

esp_err_t ui_core_init(void) {
    ESP_LOGI(TAG, "Initializing UI core...");

    /* 初始化事件系统 */
    esp_err_t ret = ui_event_init();
    if (ret != ESP_OK) {
        return ret;
    }

    /* 初始化页面栈 */
    ui_page_stack_init();

    /* 初始化渲染管线 */
    ui_render_init();

    ESP_LOGI(TAG, "UI core initialized");
    return ESP_OK;
}

esp_err_t ui_core_run(void) {
    BaseType_t ret = xTaskCreate(ui_loop_task, "ui_task",
                                 UI_TASK_STACK_SIZE, NULL,
                                 UI_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UI main loop task created");
    return ESP_OK;
}
