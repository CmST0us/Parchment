#pragma once
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
BaseType_t xTaskCreate(void(*fn)(void*), const char* name,
                        configSTACK_DEPTH_TYPE stack, void* arg,
                        int priority, TaskHandle_t* handle);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
#ifdef __cplusplus
}
#endif
