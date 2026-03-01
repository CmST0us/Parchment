#pragma once
#include <stdint.h>
#include <stddef.h>
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef int32_t BaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t UBaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define configSTACK_DEPTH_TYPE uint32_t
