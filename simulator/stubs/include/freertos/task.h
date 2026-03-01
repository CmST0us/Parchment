#pragma once
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif

#define tskIDLE_PRIORITY 0

BaseType_t xTaskCreate(void(*fn)(void*), const char* name,
                        configSTACK_DEPTH_TYPE stack, void* arg,
                        int priority, TaskHandle_t* handle);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);

/// Pinned-to-core variant (core argument ignored in simulator)
static inline BaseType_t xTaskCreatePinnedToCore(
    void(*fn)(void*), const char* name,
    configSTACK_DEPTH_TYPE stack, void* arg,
    int priority, TaskHandle_t* handle, int core) {
    (void)core;
    return xTaskCreate(fn, name, stack, arg, priority, handle);
}

#ifdef __cplusplus
}
#endif
