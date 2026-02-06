/**
 * @file gt911.h
 * @brief GT911 电容触摸驱动。
 *
 * 通过 I2C 与 GT911 触摸控制器通信，读取触摸坐标和状态。
 * 支持单点触摸检测。
 */

#ifndef GT911_H
#define GT911_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** GT911 触摸点数据。 */
typedef struct {
    uint16_t x;       /**< 触摸 X 坐标 */
    uint16_t y;       /**< 触摸 Y 坐标 */
    bool pressed;     /**< 是否按下 */
} gt911_touch_point_t;

/** GT911 配置参数。 */
typedef struct {
    int sda_gpio;     /**< I2C SDA 引脚 */
    int scl_gpio;     /**< I2C SCL 引脚 */
    int int_gpio;     /**< 中断引脚 */
    int rst_gpio;     /**< 复位引脚 (-1 表示未连接) */
    int i2c_port;     /**< I2C 端口号 */
    uint32_t i2c_freq; /**< I2C 时钟频率 (Hz) */
} gt911_config_t;

/**
 * @brief 初始化 GT911 触摸驱动。
 *
 * 配置 I2C 总线并验证与 GT911 的通信。
 *
 * @param config 配置参数。
 * @return ESP_OK 成功，其他值表示失败。
 */
esp_err_t gt911_init(const gt911_config_t *config);

/**
 * @brief 反初始化 GT911 触摸驱动，释放 I2C 资源。
 */
void gt911_deinit(void);

/**
 * @brief 读取触摸状态。
 *
 * 非阻塞读取当前触摸状态。如果无触摸，point->pressed 为 false。
 *
 * @param[out] point 输出触摸点数据。
 * @return ESP_OK 成功读取，其他值表示通信错误。
 */
esp_err_t gt911_read(gt911_touch_point_t *point);

#ifdef __cplusplus
}
#endif

#endif /* GT911_H */
