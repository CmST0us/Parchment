/**
 * @file esp_log.h
 * @brief ESP-IDF esp_log.h shim for desktop simulator.
 *
 * Maps ESP_LOGx macros to printf with color coding.
 */

#ifndef ESP_LOG_H
#define ESP_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LOGE(tag, fmt, ...) \
    fprintf(stderr, "\033[31mE (%s) " fmt "\033[0m\n", tag, ##__VA_ARGS__)

#define ESP_LOGW(tag, fmt, ...) \
    fprintf(stderr, "\033[33mW (%s) " fmt "\033[0m\n", tag, ##__VA_ARGS__)

#define ESP_LOGI(tag, fmt, ...) \
    printf("\033[32mI (%s) " fmt "\033[0m\n", tag, ##__VA_ARGS__)

#define ESP_LOGD(tag, fmt, ...) \
    printf("D (%s) " fmt "\n", tag, ##__VA_ARGS__)

#define ESP_LOGV(tag, fmt, ...) \
    do { (void)(tag); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_H */
