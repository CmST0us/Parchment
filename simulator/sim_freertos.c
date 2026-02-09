/**
 * @file sim_freertos.c
 * @brief FreeRTOS API implementation using pthreads for desktop simulator.
 *
 * Implements xQueueCreate/Send/Receive and xTaskCreate/vTaskDelay/vTaskDelete
 * using POSIX threads, mutexes, and condition variables.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

/* ========================================================================== */
/*  Queue implementation                                                       */
/* ========================================================================== */

typedef struct {
    uint8_t        *buffer;
    size_t          item_size;
    size_t          capacity;
    size_t          head;
    size_t          tail;
    size_t          count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
} sim_queue_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    sim_queue_t *q = calloc(1, sizeof(sim_queue_t));
    if (!q) return NULL;

    q->buffer = calloc(uxQueueLength, uxItemSize);
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    q->item_size = uxItemSize;
    q->capacity  = uxQueueLength;
    q->head      = 0;
    q->tail      = 0;
    q->count     = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);

    return (QueueHandle_t)q;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue,
                       TickType_t xTicksToWait) {
    (void)xTicksToWait; /* non-blocking send only (matches ESP-IDF usage) */
    sim_queue_t *q = (sim_queue_t *)xQueue;
    if (!q || !pvItemToQueue) return pdFALSE;

    pthread_mutex_lock(&q->mutex);

    if (q->count >= q->capacity) {
        pthread_mutex_unlock(&q->mutex);
        return pdFALSE;
    }

    memcpy(q->buffer + q->tail * q->item_size, pvItemToQueue, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);

    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer,
                          TickType_t xTicksToWait) {
    sim_queue_t *q = (sim_queue_t *)xQueue;
    if (!q || !pvBuffer) return pdFALSE;

    pthread_mutex_lock(&q->mutex);

    while (q->count == 0) {
        if (xTicksToWait == 0) {
            pthread_mutex_unlock(&q->mutex);
            return pdFALSE;
        }

        if (xTicksToWait == portMAX_DELAY) {
            pthread_cond_wait(&q->not_empty, &q->mutex);
        } else {
            /* Convert ticks (ms) to absolute timespec */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);

            uint64_t ns = (uint64_t)xTicksToWait * 1000000ULL;
            ts.tv_sec  += ns / 1000000000ULL;
            ts.tv_nsec += ns % 1000000000ULL;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }

            int ret = pthread_cond_timedwait(&q->not_empty, &q->mutex, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&q->mutex);
                return pdFALSE;
            }
        }
    }

    memcpy(pvBuffer, q->buffer + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_mutex_unlock(&q->mutex);
    return pdTRUE;
}

/* ========================================================================== */
/*  Task implementation                                                        */
/* ========================================================================== */

typedef struct {
    TaskFunction_t func;
    void          *arg;
} task_wrapper_t;

static void *task_thread_func(void *arg) {
    task_wrapper_t *tw = (task_wrapper_t *)arg;
    tw->func(tw->arg);
    free(tw);
    return NULL;
}

BaseType_t xTaskCreate(TaskFunction_t pvTaskCode,
                       const char *pcName,
                       uint32_t usStackDepth,
                       void *pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t *pxCreatedTask) {
    (void)pcName;
    (void)usStackDepth;
    (void)uxPriority;

    task_wrapper_t *tw = malloc(sizeof(task_wrapper_t));
    if (!tw) return pdFAIL;

    tw->func = pvTaskCode;
    tw->arg  = pvParameters;

    pthread_t *thread = malloc(sizeof(pthread_t));
    if (!thread) {
        free(tw);
        return pdFAIL;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(thread, &attr, task_thread_func, tw);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        free(tw);
        free(thread);
        return pdFAIL;
    }

    if (pxCreatedTask) {
        *pxCreatedTask = (TaskHandle_t)thread;
    } else {
        free(thread);
    }

    return pdPASS;
}

void vTaskDelay(TickType_t xTicksToDelay) {
    usleep((useconds_t)xTicksToDelay * 1000);
}

void vTaskDelete(TaskHandle_t xTaskToDelete) {
    (void)xTaskToDelete;
    /* Thread is detached and will clean up on exit */
    pthread_exit(NULL);
}
