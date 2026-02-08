/**
 * @file littlefs_storage.h
 * @brief LittleFS 内部 Flash 存储驱动。
 *
 * 将 flash storage 分区挂载为 LittleFS 文件系统到 VFS，
 * 用于存放内置字体等只读资源。挂载点: /littlefs
 */

#ifndef LITTLEFS_STORAGE_H
#define LITTLEFS_STORAGE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** LittleFS 挂载点路径。 */
#define LITTLEFS_MOUNT_POINT "/littlefs"

/** LittleFS 分区标签。 */
#define LITTLEFS_PARTITION_LABEL "storage"

/**
 * @brief 挂载 LittleFS 文件系统。
 *
 * 将 flash 中 storage 分区挂载到 /littlefs。
 * 如果分区不含有效文件系统，将自动格式化后重试挂载。
 *
 * @return ESP_OK 成功，其他值表示错误。
 */
esp_err_t littlefs_storage_mount(void);

/**
 * @brief 卸载 LittleFS 文件系统。
 */
void littlefs_storage_unmount(void);

/**
 * @brief 查询 LittleFS 是否已挂载。
 *
 * @return true 已挂载，false 未挂载。
 */
bool littlefs_storage_is_mounted(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEFS_STORAGE_H */
