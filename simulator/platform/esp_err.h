/**
 * @file esp_err.h
 * @brief ESP-IDF esp_err.h shim for desktop simulator.
 */

#ifndef ESP_ERR_H
#define ESP_ERR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t esp_err_t;

#define ESP_OK              0
#define ESP_FAIL            (-1)
#define ESP_ERR_NO_MEM      0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT     0x107

#ifdef __cplusplus
}
#endif

#endif /* ESP_ERR_H */
