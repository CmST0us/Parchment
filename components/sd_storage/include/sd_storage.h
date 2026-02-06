/**
 * @file sd_storage.h
 * @brief MicroSD 卡存储驱动。
 *
 * 通过 SPI 接口挂载 SD 卡到 VFS，支持标准 C 文件操作。
 * 挂载点: /sdcard
 */

#ifndef SD_STORAGE_H
#define SD_STORAGE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** SD 卡挂载点路径。 */
#define SD_MOUNT_POINT "/sdcard"

/** SD 卡配置参数。 */
typedef struct {
    int miso_gpio;    /**< SPI MISO 引脚 */
    int mosi_gpio;    /**< SPI MOSI 引脚 */
    int clk_gpio;     /**< SPI CLK 引脚 */
    int cs_gpio;      /**< SPI CS 引脚 */
} sd_storage_config_t;

/**
 * @brief 挂载 SD 卡。
 *
 * 通过 SPI 接口挂载 FAT 文件系统到 /sdcard。
 *
 * @param config SPI 引脚配置。
 * @return ESP_OK 成功，ESP_FAIL SD 卡未插入或挂载失败。
 */
esp_err_t sd_storage_mount(const sd_storage_config_t *config);

/**
 * @brief 卸载 SD 卡。
 */
void sd_storage_unmount(void);

/**
 * @brief 查询 SD 卡是否已挂载。
 *
 * @return true 已挂载，false 未挂载。
 */
bool sd_storage_is_mounted(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_STORAGE_H */
