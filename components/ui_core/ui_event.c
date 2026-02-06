/**
 * @file ui_event.c
 * @brief 事件系统实现。
 */

#include "ui_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "ui_event";

/** 事件队列容量。 */
#define UI_EVENT_QUEUE_SIZE 16

/** 事件队列句柄。 */
static QueueHandle_t s_event_queue = NULL;

esp_err_t ui_event_init(void) {
    if (s_event_queue != NULL) {
        return ESP_OK;
    }

    s_event_queue = xQueueCreate(UI_EVENT_QUEUE_SIZE, sizeof(ui_event_t));
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Event system initialized (queue size: %d)", UI_EVENT_QUEUE_SIZE);
    return ESP_OK;
}

esp_err_t ui_event_send(const ui_event_t *event) {
    if (s_event_queue == NULL || event == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(s_event_queue, event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping event type %d", event->type);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t ui_event_receive(ui_event_t *event, uint32_t timeout_ms) {
    if (s_event_queue == NULL || event == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                       ? portMAX_DELAY
                       : pdMS_TO_TICKS(timeout_ms);

    if (xQueueReceive(s_event_queue, event, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}
