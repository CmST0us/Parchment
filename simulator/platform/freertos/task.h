/**
 * @file task.h
 * @brief FreeRTOS task.h shim for desktop simulator.
 */

#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

/**
 * @brief Create a new task (backed by a pthread).
 */
BaseType_t xTaskCreate(TaskFunction_t pvTaskCode,
                       const char *pcName,
                       uint32_t usStackDepth,
                       void *pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t *pxCreatedTask);

/**
 * @brief Delay the calling task.
 */
void vTaskDelay(TickType_t xTicksToDelay);

/**
 * @brief Delete a task.
 */
void vTaskDelete(TaskHandle_t xTaskToDelete);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_TASK_H */
