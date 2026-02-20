/**
 * @file battery.c
 * @brief 电池电压 ADC 采集与电量百分比计算实现。
 */

#include "battery.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char* TAG = "battery";

/* ADC 配置: G3 = ADC1_CH2 */
#define BAT_ADC_UNIT       ADC_UNIT_1
#define BAT_ADC_CHANNEL    ADC_CHANNEL_2
#define BAT_ADC_ATTEN      ADC_ATTEN_DB_12

/* 采样次数 */
#define SAMPLE_COUNT       8

/* 分压比: 22K / (22K + 22K) = 1/2, 实际电压 = ADC 电压 × 2 */
#define VOLTAGE_DIVIDER_MULT  2

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;

/* ── LiPo 放电曲线查找表 ── */

typedef struct {
    int voltage_mv;  /* 电池电压 (mV) */
    int percent;     /* 对应百分比 */
} battery_lut_entry_t;

static const battery_lut_entry_t s_lut[] = {
    {4150, 100},
    {4050,  85},
    {3950,  70},
    {3850,  55},
    {3750,  40},
    {3650,  25},
    {3500,  10},
    {3300,   0},
};

static const int s_lut_size = sizeof(s_lut) / sizeof(s_lut[0]);

/**
 * @brief 通过查找表线性插值计算电量百分比。
 * @param voltage_mv 电池实际电压 (mV)。
 * @return 0-100 百分比。
 */
static int voltage_to_percent(int voltage_mv) {
    if (voltage_mv >= s_lut[0].voltage_mv) {
        return s_lut[0].percent;
    }
    if (voltage_mv <= s_lut[s_lut_size - 1].voltage_mv) {
        return s_lut[s_lut_size - 1].percent;
    }

    /* 找到区间并线性插值 */
    for (int i = 0; i < s_lut_size - 1; i++) {
        if (voltage_mv >= s_lut[i + 1].voltage_mv) {
            int v_hi = s_lut[i].voltage_mv;
            int v_lo = s_lut[i + 1].voltage_mv;
            int p_hi = s_lut[i].percent;
            int p_lo = s_lut[i + 1].percent;
            return p_lo + (voltage_mv - v_lo) * (p_hi - p_lo) / (v_hi - v_lo);
        }
    }

    return 0;
}

/**
 * @brief 初始化 ADC 校准方案。
 * @return true 成功创建校准 handle。
 */
static bool init_calibration(void) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BAT_ADC_UNIT,
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle) == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration: curve fitting");
        return true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg_line = {
        .unit_id = BAT_ADC_UNIT,
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg_line, &s_cali_handle) == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration: line fitting");
        return true;
    }
#endif

    ESP_LOGW(TAG, "ADC calibration not available");
    return false;
}

esp_err_t battery_init(void) {
    /* 初始化 ADC oneshot */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BAT_ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置通道 */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(s_adc_handle, BAT_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 校准（非致命，无校准时使用 raw 值近似） */
    init_calibration();

    ESP_LOGI(TAG, "Battery ADC initialized (ADC1_CH2, 12dB atten)");
    return ESP_OK;
}

int battery_get_percent(void) {
    if (!s_adc_handle) {
        return 0;
    }

    /* 多次采样取均值 */
    int raw_sum = 0;
    int valid = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int raw;
        if (adc_oneshot_read(s_adc_handle, BAT_ADC_CHANNEL, &raw) == ESP_OK) {
            raw_sum += raw;
            valid++;
        }
    }
    if (valid == 0) {
        return 0;
    }
    int raw_avg = raw_sum / valid;

    /* 转换为 mV */
    int adc_mv;
    if (s_cali_handle) {
        adc_cali_raw_to_voltage(s_cali_handle, raw_avg, &adc_mv);
    } else {
        /* 无校准时粗略估算: 12dB 衰减, 12-bit, 约 0-3100mV */
        adc_mv = raw_avg * 3100 / 4095;
    }

    /* 还原电池实际电压 */
    int bat_mv = adc_mv * VOLTAGE_DIVIDER_MULT;

    int percent = voltage_to_percent(bat_mv);
    ESP_LOGI(TAG, "Battery: %dmV (ADC: %dmV, raw: %d), %d%%", bat_mv, adc_mv, raw_avg, percent);

    return percent;
}
