/**
 * @file sim_nvs.c
 * @brief Simple in-memory NVS implementation for simulator.
 * Uses a flat array of key-value entries, no persistence.
 */
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

#define MAX_NVS_ENTRIES 64
#define MAX_NVS_HANDLES 8
#define MAX_KEY_LEN     16
#define MAX_NS_LEN      16
#define MAX_VALUE_LEN   256

typedef struct {
    char ns[MAX_NS_LEN];
    char key[MAX_KEY_LEN];
    uint8_t value[MAX_VALUE_LEN];
    size_t value_len;
    bool used;
} nvs_entry_t;

typedef struct {
    char ns[MAX_NS_LEN];
    bool used;
} nvs_handle_entry_t;

static nvs_entry_t s_entries[MAX_NVS_ENTRIES];
static nvs_handle_entry_t s_handles[MAX_NVS_HANDLES];
static bool s_initialized = false;

esp_err_t nvs_flash_init(void) {
    if (!s_initialized) {
        memset(s_entries, 0, sizeof(s_entries));
        memset(s_handles, 0, sizeof(s_handles));
        s_initialized = true;
    }
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void) {
    memset(s_entries, 0, sizeof(s_entries));
    return ESP_OK;
}

esp_err_t nvs_open(const char* ns, nvs_open_mode_t mode, nvs_handle_t* handle) {
    (void)mode;
    if (!ns || !handle) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < MAX_NVS_HANDLES; i++) {
        if (!s_handles[i].used) {
            s_handles[i].used = true;
            strncpy(s_handles[i].ns, ns, MAX_NS_LEN - 1);
            s_handles[i].ns[MAX_NS_LEN - 1] = '\0';
            *handle = (nvs_handle_t)i;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

void nvs_close(nvs_handle_t handle) {
    if (handle < MAX_NVS_HANDLES) {
        s_handles[handle].used = false;
    }
}

/// Find an entry by namespace and key. Returns index or -1.
static int find_entry(const char* ns, const char* key) {
    for (int i = 0; i < MAX_NVS_ENTRIES; i++) {
        if (s_entries[i].used &&
            strcmp(s_entries[i].ns, ns) == 0 &&
            strcmp(s_entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

/// Find a free slot. Returns index or -1.
static int alloc_entry(void) {
    for (int i = 0; i < MAX_NVS_ENTRIES; i++) {
        if (!s_entries[i].used) return i;
    }
    return -1;
}

static const char* handle_ns(nvs_handle_t handle) {
    if (handle >= MAX_NVS_HANDLES || !s_handles[handle].used) return "";
    return s_handles[handle].ns;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out) {
    if (!key || !out) return ESP_ERR_INVALID_ARG;
    int idx = find_entry(handle_ns(handle), key);
    if (idx < 0) return ESP_ERR_NVS_NOT_FOUND;
    if (s_entries[idx].value_len != sizeof(uint8_t)) return ESP_FAIL;
    *out = s_entries[idx].value[0];
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value) {
    if (!key) return ESP_ERR_INVALID_ARG;
    const char* ns = handle_ns(handle);
    int idx = find_entry(ns, key);
    if (idx < 0) {
        idx = alloc_entry();
        if (idx < 0) return ESP_FAIL;
        s_entries[idx].used = true;
        strncpy(s_entries[idx].ns, ns, MAX_NS_LEN - 1);
        s_entries[idx].ns[MAX_NS_LEN - 1] = '\0';
        strncpy(s_entries[idx].key, key, MAX_KEY_LEN - 1);
        s_entries[idx].key[MAX_KEY_LEN - 1] = '\0';
    }
    s_entries[idx].value[0] = value;
    s_entries[idx].value_len = sizeof(uint8_t);
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out, size_t* length) {
    if (!key || !length) return ESP_ERR_INVALID_ARG;
    int idx = find_entry(handle_ns(handle), key);
    if (idx < 0) return ESP_ERR_NVS_NOT_FOUND;
    if (out == NULL) {
        /* Query required size */
        *length = s_entries[idx].value_len;
        return ESP_OK;
    }
    if (*length < s_entries[idx].value_len) return ESP_ERR_INVALID_ARG;
    memcpy(out, s_entries[idx].value, s_entries[idx].value_len);
    *length = s_entries[idx].value_len;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length) {
    if (!key || !value || length > MAX_VALUE_LEN) return ESP_ERR_INVALID_ARG;
    const char* ns = handle_ns(handle);
    int idx = find_entry(ns, key);
    if (idx < 0) {
        idx = alloc_entry();
        if (idx < 0) return ESP_FAIL;
        s_entries[idx].used = true;
        strncpy(s_entries[idx].ns, ns, MAX_NS_LEN - 1);
        s_entries[idx].ns[MAX_NS_LEN - 1] = '\0';
        strncpy(s_entries[idx].key, key, MAX_KEY_LEN - 1);
        s_entries[idx].key[MAX_KEY_LEN - 1] = '\0';
    }
    memcpy(s_entries[idx].value, value, length);
    s_entries[idx].value_len = length;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle) {
    (void)handle;
    return ESP_OK;
}
