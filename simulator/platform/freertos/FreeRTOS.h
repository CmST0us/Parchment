/**
 * @file FreeRTOS.h
 * @brief FreeRTOS shim for desktop simulator.
 *
 * Provides type definitions and macros used by the UI framework.
 * Actual implementations are in sim_freertos.c using pthreads.
 */

#ifndef FREERTOS_H
#define FREERTOS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t  BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;

#define pdTRUE   1
#define pdFALSE  0
#define pdPASS   1
#define pdFAIL   0

/* 1 tick = 1 ms in simulator */
#define configTICK_RATE_HZ   1000
#define portMAX_DELAY        0xFFFFFFFFUL
#define portTICK_PERIOD_MS   1

#define pdMS_TO_TICKS(ms)    ((TickType_t)(ms))

#define portYIELD_FROM_ISR() do {} while (0)

/* IRAM_ATTR is not needed on desktop */
#define IRAM_ATTR

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_H */
