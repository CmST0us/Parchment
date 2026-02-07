/**
 * @file semphr.h
 * @brief FreeRTOS semphr.h shim for desktop simulator.
 *
 * Provides stub declarations. The simulator's touch input is handled
 * via SDL events, so semaphores are not needed in practice.
 */

#ifndef FREERTOS_SEMPHR_H
#define FREERTOS_SEMPHR_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) { return NULL; }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s; (void)t; return pdTRUE; }
static inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *w) { (void)s; (void)w; return pdTRUE; }

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_SEMPHR_H */
