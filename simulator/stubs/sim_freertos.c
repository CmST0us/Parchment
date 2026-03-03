/**
 * @file sim_freertos.c
 * @brief FreeRTOS stubs via pthreads for simulator.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* ========================================================================== */
/*  Task                                                                      */
/* ========================================================================== */

typedef struct {
    void (*fn)(void*);
    void* arg;
} task_trampoline_t;

static void* task_thread(void* arg) {
    task_trampoline_t* t = (task_trampoline_t*)arg;
    t->fn(t->arg);
    free(t);
    return NULL;
}

BaseType_t xTaskCreate(void(*fn)(void*), const char* name,
                        configSTACK_DEPTH_TYPE stack, void* arg,
                        int priority, TaskHandle_t* handle) {
    (void)name;
    (void)stack;
    (void)priority;

    task_trampoline_t* t = (task_trampoline_t*)malloc(sizeof(task_trampoline_t));
    if (!t) return pdFALSE;
    t->fn = fn;
    t->arg = arg;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(&tid, &attr, task_thread, t);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        free(t);
        return pdFALSE;
    }
    if (handle) *handle = (TaskHandle_t)(uintptr_t)tid;
    return pdPASS;
}

void vTaskDelete(TaskHandle_t handle) {
    (void)handle;
    /* Threads are detached; no-op */
}

void vTaskDelay(TickType_t ticks) {
    usleep((useconds_t)ticks * 1000);
}

TickType_t xTaskGetTickCount(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TickType_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* ========================================================================== */
/*  Semaphore                                                                 */
/* ========================================================================== */

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
} sim_sem_t;

SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    sim_sem_t* s = (sim_sem_t*)calloc(1, sizeof(sim_sem_t));
    if (!s) return NULL;
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->value = 0;  /* Binary semaphore starts "empty" */
    return (SemaphoreHandle_t)s;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) {
    if (!sem) return pdFALSE;
    sim_sem_t* s = (sim_sem_t*)sem;
    pthread_mutex_lock(&s->mutex);

    if (timeout == portMAX_DELAY) {
        while (s->value == 0) {
            pthread_cond_wait(&s->cond, &s->mutex);
        }
    } else if (timeout > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        uint64_t ns = (uint64_t)timeout * 1000000ULL;
        ts.tv_sec += (time_t)(ns / 1000000000ULL);
        ts.tv_nsec += (long)(ns % 1000000000ULL);
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
        while (s->value == 0) {
            int ret = pthread_cond_timedwait(&s->cond, &s->mutex, &ts);
            if (ret == ETIMEDOUT) break;
        }
    }

    BaseType_t result;
    if (s->value > 0) {
        s->value--;
        result = pdTRUE;
    } else {
        result = pdFALSE;
    }
    pthread_mutex_unlock(&s->mutex);
    return result;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
    if (!sem) return pdFALSE;
    sim_sem_t* s = (sim_sem_t*)sem;
    pthread_mutex_lock(&s->mutex);
    s->value = 1;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
    return pdTRUE;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t* woken) {
    if (woken) *woken = pdFALSE;
    return xSemaphoreGive(sem);
}

void vSemaphoreDelete(SemaphoreHandle_t sem) {
    if (!sem) return;
    sim_sem_t* s = (sim_sem_t*)sem;
    pthread_mutex_destroy(&s->mutex);
    pthread_cond_destroy(&s->cond);
    free(s);
}

/* ========================================================================== */
/*  Queue                                                                     */
/* ========================================================================== */

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    uint8_t* buffer;
    UBaseType_t item_size;
    UBaseType_t capacity;
    UBaseType_t count;
    UBaseType_t head;
    UBaseType_t tail;
} sim_queue_t;

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize) {
    sim_queue_t* q = (sim_queue_t*)calloc(1, sizeof(sim_queue_t));
    if (!q) return NULL;
    q->buffer = (uint8_t*)calloc(length, itemSize);
    if (!q->buffer) { free(q); return NULL; }
    q->item_size = itemSize;
    q->capacity = length;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return (QueueHandle_t)q;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t timeout) {
    if (!queue || !item) return pdFALSE;
    sim_queue_t* q = (sim_queue_t*)queue;
    pthread_mutex_lock(&q->mutex);

    if (q->count >= q->capacity) {
        if (timeout == 0) {
            pthread_mutex_unlock(&q->mutex);
            return pdFALSE;
        }
        if (timeout == portMAX_DELAY) {
            while (q->count >= q->capacity) {
                pthread_cond_wait(&q->not_full, &q->mutex);
            }
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            uint64_t ns = (uint64_t)timeout * 1000000ULL;
            ts.tv_sec += (time_t)(ns / 1000000000ULL);
            ts.tv_nsec += (long)(ns % 1000000000ULL);
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            while (q->count >= q->capacity) {
                int ret = pthread_cond_timedwait(&q->not_full, &q->mutex, &ts);
                if (ret == ETIMEDOUT) {
                    pthread_mutex_unlock(&q->mutex);
                    return pdFALSE;
                }
            }
        }
    }

    memcpy(q->buffer + q->tail * q->item_size, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t timeout) {
    if (!queue || !buffer) return pdFALSE;
    sim_queue_t* q = (sim_queue_t*)queue;
    pthread_mutex_lock(&q->mutex);

    if (q->count == 0) {
        if (timeout == 0) {
            pthread_mutex_unlock(&q->mutex);
            return pdFALSE;
        }
        if (timeout == portMAX_DELAY) {
            while (q->count == 0) {
                pthread_cond_wait(&q->not_empty, &q->mutex);
            }
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            uint64_t ns = (uint64_t)timeout * 1000000ULL;
            ts.tv_sec += (time_t)(ns / 1000000000ULL);
            ts.tv_nsec += (long)(ns % 1000000000ULL);
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            while (q->count == 0) {
                int ret = pthread_cond_timedwait(&q->not_empty, &q->mutex, &ts);
                if (ret == ETIMEDOUT) {
                    pthread_mutex_unlock(&q->mutex);
                    return pdFALSE;
                }
            }
        }
    }

    memcpy(buffer, q->buffer + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return pdTRUE;
}
