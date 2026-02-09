/**
 * @file sim_stubs.c
 * @brief Stub implementations for hardware-specific functions.
 *
 * Provides no-op implementations of board_init(), gt911_init/deinit/read(),
 * and sd_storage_mount/unmount/is_mounted() for the desktop simulator.
 */

#include "board.h"
#include "gt911.h"
#include "sd_storage.h"

#include <stdio.h>

/* ---- Board ---- */

void board_init(void) {
    printf("sim_stubs: board_init() (no-op)\n");
}

/* ---- GT911 touch ---- */

esp_err_t gt911_init(const gt911_config_t *config) {
    (void)config;
    printf("sim_stubs: gt911_init() (no-op, touch via SDL mouse)\n");
    return ESP_OK;
}

void gt911_deinit(void) {
}

esp_err_t gt911_read(gt911_touch_point_t *point) {
    if (point) {
        point->pressed = false;
    }
    return ESP_OK;
}

/* ---- SD storage ---- */

esp_err_t sd_storage_mount(const sd_storage_config_t *config) {
    (void)config;
    printf("sim_stubs: sd_storage_mount() (no-op, using local filesystem)\n");
    return ESP_OK;
}

void sd_storage_unmount(void) {
}

bool sd_storage_is_mounted(void) {
    return true;
}
