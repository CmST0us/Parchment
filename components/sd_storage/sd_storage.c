/**
 * @file sd_storage.c
 * @brief MicroSD 卡存储驱动实现。
 *
 * 使用 ESP-IDF sdmmc 组件的 SPI 模式挂载 FAT 文件系统。
 */

#include "sd_storage.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

static const char *TAG = "sd_storage";

static sdmmc_card_t *s_card = NULL;
static bool s_mounted = false;
static spi_host_device_t s_host = SPI2_HOST;

esp_err_t sd_storage_mount(const sd_storage_config_t *config) {
    if (s_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mounting SD card (SPI mode)...");

    /* SPI 总线配置 */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = config->mosi_gpio,
        .miso_io_num = config->miso_gpio,
        .sclk_io_num = config->clk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret = spi_bus_initialize(s_host, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* SDSPI 设备配置 */
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = config->cs_gpio;
    slot_config.host_id = s_host;

    /* FAT 文件系统挂载配置 */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &(sdmmc_host_t)SDSPI_HOST_DEFAULT(),
                                  &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        spi_bus_free(s_host);
        return ret;
    }

    /* 打印卡信息 */
    sdmmc_card_print_info(stdout, s_card);

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    return ESP_OK;
}

void sd_storage_unmount(void) {
    if (!s_mounted) {
        return;
    }
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    spi_bus_free(s_host);
    s_card = NULL;
    s_mounted = false;
    ESP_LOGI(TAG, "SD card unmounted");
}

bool sd_storage_is_mounted(void) {
    return s_mounted;
}
