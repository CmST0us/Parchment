/**
 * @file settings_store.c
 * @brief 设置与进度存储实现 — NVS 读写。
 */

#include "settings_store.h"

#include <string.h>

#include "esp_log.h"
#include "mbedtls/md5.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "settings_store";

/** NVS namespace 名称。 */
#define NS_SETTINGS "settings"
#define NS_PROGRESS "progress"

/** NVS key 名称 — 阅读偏好字段。 */
#define KEY_FONT_SIZE    "font_sz"
#define KEY_LINE_SPACING "line_sp"
#define KEY_PARA_SPACING "para_sp"
#define KEY_MARGIN       "margin"
#define KEY_FULL_REFRESH "full_ref"
#define KEY_SORT_ORDER   "sort_ord"

/**
 * @brief 将文件路径转换为 NVS key（MD5 hash 前 15 hex 字符）。
 *
 * NVS key 最大 15 字节，此函数对路径做 MD5 后取前 15 hex 字符。
 *
 * @param file_path 文件完整路径。
 * @param key_out   输出缓冲区（至少 16 字节）。
 */
static void path_to_nvs_key(const char *file_path, char *key_out) {
    uint8_t hash[16];
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    mbedtls_md5_update(&ctx, (const uint8_t *)file_path, strlen(file_path));
    mbedtls_md5_finish(&ctx, hash);
    mbedtls_md5_free(&ctx);

    /* 取前 7.5 字节 = 15 hex 字符 */
    for (int i = 0; i < 7; i++) {
        sprintf(key_out + i * 2, "%02x", hash[i]);
    }
    /* 第 8 字节取高 4 位 */
    sprintf(key_out + 14, "%x", (hash[7] >> 4) & 0x0F);
    key_out[15] = '\0';
}

esp_err_t settings_store_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition corrupt, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized");
    }
    return ret;
}

/**
 * @brief 辅助：从 NVS 读取 uint8，读失败时返回默认值。
 */
static uint8_t nvs_get_u8_or(nvs_handle_t h, const char *key, uint8_t def) {
    uint8_t val;
    if (nvs_get_u8(h, key, &val) != ESP_OK) {
        return def;
    }
    return val;
}

esp_err_t settings_store_save_prefs(const reading_prefs_t *prefs) {
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_SETTINGS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %s", NS_SETTINGS, esp_err_to_name(ret));
        return ret;
    }

    nvs_set_u8(h, KEY_FONT_SIZE, prefs->font_size);
    nvs_set_u8(h, KEY_LINE_SPACING, prefs->line_spacing);
    nvs_set_u8(h, KEY_PARA_SPACING, prefs->paragraph_spacing);
    nvs_set_u8(h, KEY_MARGIN, prefs->margin);
    nvs_set_u8(h, KEY_FULL_REFRESH, prefs->full_refresh_pages);

    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t settings_store_load_prefs(reading_prefs_t *prefs) {
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_SETTINGS, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        /* 首次使用，namespace 不存在，用默认值 */
        prefs->font_size = SETTINGS_DEFAULT_FONT_SIZE;
        prefs->line_spacing = SETTINGS_DEFAULT_LINE_SPACING;
        prefs->paragraph_spacing = SETTINGS_DEFAULT_PARAGRAPH_SPACING;
        prefs->margin = SETTINGS_DEFAULT_MARGIN;
        prefs->full_refresh_pages = SETTINGS_DEFAULT_FULL_REFRESH;
        return ESP_OK;
    }

    prefs->font_size = nvs_get_u8_or(h, KEY_FONT_SIZE, SETTINGS_DEFAULT_FONT_SIZE);
    prefs->line_spacing = nvs_get_u8_or(h, KEY_LINE_SPACING, SETTINGS_DEFAULT_LINE_SPACING);
    prefs->paragraph_spacing = nvs_get_u8_or(h, KEY_PARA_SPACING, SETTINGS_DEFAULT_PARAGRAPH_SPACING);
    prefs->margin = nvs_get_u8_or(h, KEY_MARGIN, SETTINGS_DEFAULT_MARGIN);
    prefs->full_refresh_pages = nvs_get_u8_or(h, KEY_FULL_REFRESH, SETTINGS_DEFAULT_FULL_REFRESH);

    nvs_close(h);
    return ESP_OK;
}

esp_err_t settings_store_save_progress(const char *file_path,
                                       const reading_progress_t *progress) {
    char key[16];
    path_to_nvs_key(file_path, key);

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_PROGRESS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %s", NS_PROGRESS, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(h, key, progress, sizeof(reading_progress_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(h);
    }
    nvs_close(h);
    return ret;
}

esp_err_t settings_store_load_progress(const char *file_path,
                                       reading_progress_t *progress) {
    char key[16];
    path_to_nvs_key(file_path, key);

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_PROGRESS, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        memset(progress, 0, sizeof(reading_progress_t));
        return ESP_ERR_NOT_FOUND;
    }

    size_t len = sizeof(reading_progress_t);
    ret = nvs_get_blob(h, key, progress, &len);
    nvs_close(h);

    if (ret != ESP_OK) {
        memset(progress, 0, sizeof(reading_progress_t));
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

esp_err_t settings_store_save_sort_order(uint8_t order) {
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_SETTINGS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(h, KEY_SORT_ORDER, order);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t settings_store_load_sort_order(uint8_t *order) {
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_SETTINGS, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        *order = 0; /* BOOK_SORT_NAME */
        return ESP_OK;
    }

    if (nvs_get_u8(h, KEY_SORT_ORDER, order) != ESP_OK) {
        *order = 0;
    }
    nvs_close(h);
    return ESP_OK;
}
