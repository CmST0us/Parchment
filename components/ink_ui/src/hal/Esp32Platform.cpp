/**
 * @file Esp32Platform.cpp
 * @brief FreeRTOS 平台抽象的 ESP32 实现。
 */

#include "ink_ui/hal/Esp32Platform.h"

#include <cstdio>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"
}

namespace ink {

// ── esp_timer 回调上下文 ──

struct TimerCtx {
    TimerCallback callback;
    void* arg;
    esp_timer_handle_t timerHandle;
};

/// esp_timer 回调：调用用户回调后释放资源
static void espTimerCallback(void* arg) {
    auto* ctx = static_cast<TimerCtx*>(arg);
    ctx->callback(ctx->arg);
    // 删除定时器并释放上下文
    esp_timer_delete(ctx->timerHandle);
    delete ctx;
}

// ── Platform 接口实现 ──

QueueHandle Esp32Platform::createQueue(int maxItems, int itemSize) {
    return static_cast<QueueHandle>(xQueueCreate(maxItems, itemSize));
}

bool Esp32Platform::queueSend(QueueHandle queue, const void* item,
                               uint32_t timeout_ms) {
    return xQueueSend(static_cast<QueueHandle_t>(queue),
                      item, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool Esp32Platform::queueReceive(QueueHandle queue, void* item,
                                  uint32_t timeout_ms) {
    return xQueueReceive(static_cast<QueueHandle_t>(queue),
                         item, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

TaskHandle Esp32Platform::createTask(void(*entry)(void*), const char* name,
                                      int stackSize, void* arg, int priority) {
    TaskHandle_t handle = nullptr;
    BaseType_t ret = xTaskCreate(entry, name, stackSize, arg, priority, &handle);
    if (ret != pdPASS) {
        fprintf(stderr, "Esp32Platform: xTaskCreate '%s' failed\n", name);
        return nullptr;
    }
    return static_cast<TaskHandle>(handle);
}

bool Esp32Platform::startOneShotTimer(uint32_t delayMs, TimerCallback callback,
                                       void* arg) {
    auto* ctx = new TimerCtx{callback, arg, nullptr};

    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = espTimerCallback;
    timerArgs.arg = ctx;
    timerArgs.dispatch_method = ESP_TIMER_TASK;
    timerArgs.name = "ink_oneshot";

    esp_err_t err = esp_timer_create(&timerArgs, &ctx->timerHandle);
    if (err != ESP_OK) {
        fprintf(stderr, "Esp32Platform: esp_timer_create failed (%d)\n", err);
        delete ctx;
        return false;
    }

    err = esp_timer_start_once(ctx->timerHandle,
                               static_cast<uint64_t>(delayMs) * 1000);
    if (err != ESP_OK) {
        fprintf(stderr, "Esp32Platform: esp_timer_start_once failed (%d)\n", err);
        esp_timer_delete(ctx->timerHandle);
        delete ctx;
        return false;
    }

    return true;
}

int64_t Esp32Platform::getTimeUs() {
    return esp_timer_get_time();
}

void Esp32Platform::delayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace ink
