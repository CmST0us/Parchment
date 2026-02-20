/**
 * @file battery.h
 * @brief 电池电压 ADC 采集与电量百分比计算。
 *
 * 通过 ESP32-S3 ADC1 读取经 22K/22K 分压后的电池电压，
 * 使用 LiPo 放电曲线查找表将电压映射为 0-100 百分比。
 */

#ifndef BATTERY_H
#define BATTERY_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化电池 ADC。
 *
 * 配置 ADC1 oneshot 模式，12-bit 分辨率，12dB 衰减，
 * 并创建校准方案（curve fitting 优先，fallback line fitting）。
 *
 * @return ESP_OK 成功，其他为错误码。
 */
esp_err_t battery_init(void);

/**
 * @brief 读取电池电量百分比。
 *
 * 执行 8 次 ADC 采样取均值，经校准转换为 mV，
 * 乘以 2 还原电池实际电压，通过查找表线性插值得到百分比。
 *
 * @return 0-100 的电量百分比。
 */
int battery_get_percent(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_H */
