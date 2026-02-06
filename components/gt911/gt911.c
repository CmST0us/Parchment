/**
 * @file gt911.c
 * @brief GT911 电容触摸驱动实现。
 *
 * GT911 通过 I2C 通信，地址 0x5D (默认) 或 0x14。
 * 寄存器 0x814E: 触摸点数和状态
 * 寄存器 0x814F-0x8156: 第一个触摸点数据
 */

#include "gt911.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "gt911";

/* GT911 I2C 地址 */
#define GT911_ADDR_PRIMARY   0x5D
#define GT911_ADDR_SECONDARY 0x14

/* GT911 寄存器 */
#define GT911_REG_STATUS     0x814E
#define GT911_REG_POINT1     0x814F
#define GT911_REG_PRODUCT_ID 0x8140

/* 内部状态 */
static int s_i2c_port = -1;
static uint8_t s_i2c_addr = GT911_ADDR_PRIMARY;
static bool s_initialized = false;

/**
 * @brief 向 GT911 写入数据。
 */
static esp_err_t gt911_write_reg(uint16_t reg, const uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    /* GT911 使用 16-bit 寄存器地址，大端序 */
    uint8_t reg_hi = (reg >> 8) & 0xFF;
    uint8_t reg_lo = reg & 0xFF;
    i2c_master_write_byte(cmd, reg_hi, true);
    i2c_master_write_byte(cmd, reg_lo, true);
    if (data != NULL && len > 0) {
        i2c_master_write(cmd, data, len, true);
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief 从 GT911 读取数据。
 */
static esp_err_t gt911_read_reg(uint16_t reg, uint8_t *data, size_t len) {
    /* 先写寄存器地址 */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    uint8_t reg_hi = (reg >> 8) & 0xFF;
    uint8_t reg_lo = reg & 0xFF;
    i2c_master_write_byte(cmd, reg_hi, true);
    i2c_master_write_byte(cmd, reg_lo, true);
    /* 重新 START 读取 */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_i2c_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(s_i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief 尝试检测 GT911 的 I2C 地址。
 */
static esp_err_t gt911_detect_addr(void) {
    uint8_t product_id[4] = {0};

    /* 尝试主地址 0x5D */
    s_i2c_addr = GT911_ADDR_PRIMARY;
    if (gt911_read_reg(GT911_REG_PRODUCT_ID, product_id, 4) == ESP_OK) {
        ESP_LOGI(TAG, "GT911 detected at 0x%02X, Product ID: %.4s",
                 s_i2c_addr, product_id);
        return ESP_OK;
    }

    /* 尝试备选地址 0x14 */
    s_i2c_addr = GT911_ADDR_SECONDARY;
    if (gt911_read_reg(GT911_REG_PRODUCT_ID, product_id, 4) == ESP_OK) {
        ESP_LOGI(TAG, "GT911 detected at 0x%02X, Product ID: %.4s",
                 s_i2c_addr, product_id);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "GT911 not detected on I2C bus");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t gt911_init(const gt911_config_t *config) {
    if (s_initialized) {
        ESP_LOGW(TAG, "GT911 already initialized");
        return ESP_OK;
    }

    s_i2c_port = config->i2c_port;

    /* 配置 I2C 主机 */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->i2c_freq,
    };
    esp_err_t ret = i2c_param_config(s_i2c_port, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(s_i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置中断引脚（输入） */
    if (config->int_gpio >= 0) {
        gpio_config_t int_conf = {
            .pin_bit_mask = (1ULL << config->int_gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&int_conf);
    }

    /* 等待 GT911 启动 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 检测设备 */
    ret = gt911_detect_addr();
    if (ret != ESP_OK) {
        i2c_driver_delete(s_i2c_port);
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "GT911 touch driver initialized");
    return ESP_OK;
}

void gt911_deinit(void) {
    if (!s_initialized) {
        return;
    }
    i2c_driver_delete(s_i2c_port);
    s_i2c_port = -1;
    s_initialized = false;
    ESP_LOGI(TAG, "GT911 deinitialized");
}

esp_err_t gt911_read(gt911_touch_point_t *point) {
    if (!s_initialized || point == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    point->pressed = false;
    point->x = 0;
    point->y = 0;

    /* 读取触摸状态寄存器 */
    uint8_t status = 0;
    esp_err_t ret = gt911_read_reg(GT911_REG_STATUS, &status, 1);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Bit 7: buffer status (1=有数据), Bit 3-0: 触摸点数 */
    uint8_t touch_count = status & 0x0F;
    bool buffer_ready = (status & 0x80) != 0;

    if (buffer_ready && touch_count > 0) {
        /* 读取第一个触摸点 (8 bytes: id, x_lo, x_hi, y_lo, y_hi, size_lo, size_hi, reserved) */
        uint8_t data[8] = {0};
        ret = gt911_read_reg(GT911_REG_POINT1, data, 8);
        if (ret == ESP_OK) {
            point->x = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
            point->y = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
            point->pressed = true;
        }
    }

    /* 清除状态寄存器 */
    uint8_t clear = 0;
    gt911_write_reg(GT911_REG_STATUS, &clear, 1);

    return ESP_OK;
}
