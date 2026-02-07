/**
 * @file esp_timer.h
 * @brief ESP-IDF esp_timer.h shim for desktop simulator.
 */

#ifndef ESP_TIMER_H
#define ESP_TIMER_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get time in microseconds since boot (simulated).
 */
static inline int64_t esp_timer_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

#ifdef __cplusplus
}
#endif

#endif /* ESP_TIMER_H */
