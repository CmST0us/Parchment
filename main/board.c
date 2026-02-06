/**
 * @file board.c
 * @brief M5Stack PaperS3 板级初始化实现。
 */

#include "board.h"
#include "esp_log.h"

static const char *TAG = "board";

void board_init(void) {
    /* 拉高电源保持引脚，防止设备自动关机 */
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << BOARD_PWR_HOLD),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr_conf);
    gpio_set_level(BOARD_PWR_HOLD, 1);

    ESP_LOGI(TAG, "M5PaperS3 board initialized");
}
