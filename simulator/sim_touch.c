/**
 * @file sim_touch.c
 * @brief Stub ui_touch implementation for desktop simulator.
 *
 * Touch input is handled via SDL mouse events in sim_main.c,
 * so the FreeRTOS-based touch task is not needed.
 */

#include "ui_touch.h"
#include <stdio.h>

esp_err_t ui_touch_start(int int_gpio) {
    (void)int_gpio;
    printf("sim_touch: ui_touch_start() (touch via SDL mouse)\n");
    return ESP_OK;
}

void ui_touch_stop(void) {
}
