#pragma once
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize);
BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t timeout);
BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t timeout);
#ifdef __cplusplus
}
#endif
