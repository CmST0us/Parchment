/**
 * @file littlefs_storage.c
 * @brief LittleFS 内部 Flash 存储驱动实现。
 */

#include "littlefs_storage.h"
#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "littlefs_storage";

static bool s_mounted = false;

esp_err_t littlefs_storage_mount(void) {
    if (s_mounted) {
        ESP_LOGW(TAG, "Already mounted");
        return ESP_OK;
    }

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LITTLEFS_MOUNT_POINT,
        .partition_label = LITTLEFS_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(LITTLEFS_PARTITION_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted: total=%zu, used=%zu", total, used);
    }

    s_mounted = true;
    return ESP_OK;
}

void littlefs_storage_unmount(void) {
    if (!s_mounted) {
        return;
    }
    esp_vfs_littlefs_unregister(LITTLEFS_PARTITION_LABEL);
    s_mounted = false;
    ESP_LOGI(TAG, "LittleFS unmounted");
}

bool littlefs_storage_is_mounted(void) {
    return s_mounted;
}
