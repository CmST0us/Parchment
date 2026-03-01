/**
 * @file sim_sd_storage.c
 * @brief SD storage stub for simulator. Maps to local filesystem.
 */
#include "sd_storage.h"

static bool s_mounted = false;

esp_err_t sd_storage_mount(const sd_storage_config_t *config) {
    (void)config;
    s_mounted = true;
    return 0;
}

void sd_storage_unmount(void) {
    s_mounted = false;
}

bool sd_storage_is_mounted(void) {
    return s_mounted;
}
