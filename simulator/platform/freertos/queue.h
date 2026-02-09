/**
 * @file queue.h
 * @brief FreeRTOS queue.h shim for desktop simulator.
 */

#ifndef FREERTOS_QUEUE_H
#define FREERTOS_QUEUE_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *QueueHandle_t;

/**
 * @brief Create a new queue (backed by pthread mutex + condition variable).
 */
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);

/**
 * @brief Send an item to the queue.
 */
BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue,
                       TickType_t xTicksToWait);

/**
 * @brief Receive an item from the queue.
 */
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer,
                          TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_QUEUE_H */
