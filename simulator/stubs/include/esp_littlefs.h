#pragma once
#include "esp_err.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char* base_path;
    const char* partition_label;
    size_t max_files;
    bool format_if_mount_failed;
} esp_vfs_littlefs_conf_t;
static inline esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t* conf) { (void)conf; return 0; }
static inline esp_err_t esp_littlefs_info(const char* label, size_t* total, size_t* used) {
    (void)label; if (total) *total = 16*1024*1024; if (used) *used = 0; return 0;
}
#ifdef __cplusplus
}
#endif
