/**
 * @file epd_driver.c
 * @brief E-Ink 显示驱动封装层实现。
 */

#include "epd_driver.h"
#include "epdiy.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "epd_driver";

/* epdiy 高层状态 */
static EpdiyHighlevelState s_hl_state;
static uint8_t *s_framebuffer = NULL;
static bool s_initialized = false;

/* ── 自定义刷新波形（基于 M5GFX lut_text）──
 *
 * 三种模式共用同一套 7 个精细控制相位（M5GFX lut_text phase 5-11），
 * 通过增减前置刷白相位实现速度与质量的权衡。
 * 动作只取决于目标灰度 (to)，from==to 时 noop。
 *
 * epdiy 编码: 0=noop 1=darken 2=lighten
 */

/* 7 个精细控制相位（M5GFX lut_text phase 5-11，三种模式共用） */
#define CONTROL_PHASES 7
static const uint8_t s_control_lut[CONTROL_PHASES][16] = {
    {1,2,2,1,1,1,1,1,0,0,1,1,0,0,1,2},
    {1,0,0,1,1,1,1,0,0,1,1,1,1,0,1,2},
    {1,0,0,1,2,2,1,1,1,1,2,1,1,1,1,2},
    {0,1,0,2,2,2,1,1,1,2,2,1,1,1,2,0},
    {1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2},
    {1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2},
    {1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,2},
};

/* 标准模式前置：2 个温和刷白（M5GFX lut_text phase 3-4） */
#define STANDARD_PREFIX 2
static const uint8_t s_standard_prefix[STANDARD_PREFIX][16] = {
    {2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,1},
    {2,2,2,2,1,2,2,2,2,2,2,1,2,2,2,1},
};

/* 质量模式前置：3 个强力刷白 + 2 个温和刷白（M5GFX lut_text phase 0-4） */
#define QUALITY_PREFIX 5
static const uint8_t s_quality_prefix[QUALITY_PREFIX][16] = {
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,1},
    {2,2,2,2,1,2,2,2,2,2,2,1,2,2,2,1},
};

/* 各模式总相位数 */
#define FAST_PHASES     (CONTROL_PHASES)                     /* 7 */
#define STANDARD_PHASES (STANDARD_PREFIX + CONTROL_PHASES)   /* 9 */
#define QUALITY_PHASES  (QUALITY_PREFIX + CONTROL_PHASES)    /* 12 */
#define MAX_CUSTOM_PHASES QUALITY_PHASES                     /* 12 */

static const EpdWaveformTempInterval s_custom_temp[1] = {
    { .min = 0, .max = 50 }
};
static uint8_t s_custom_data[MAX_CUSTOM_PHASES][16][4];
static EpdWaveformPhases s_custom_phases;
static const EpdWaveformPhases *s_custom_ranges[1];
static EpdWaveformMode s_custom_mode;
static const EpdWaveformMode *s_custom_modes[1];
static EpdWaveform s_custom_waveform;

/** 将单行 LUT (action[16]) 编码为 epdiy 格式 (data[to][from_byte]) */
static void encode_lut_phase(const uint8_t action[16], int phase_idx) {
    for (int t = 0; t < 16; t++) {
        for (int bi = 0; bi < 4; bi++) {
            uint8_t byte_val = 0;
            for (int fi = 0; fi < 4; fi++) {
                int f = bi * 4 + fi;
                uint8_t a = (f == t) ? 0 : action[t];  /* from==to → noop */
                byte_val |= (a << (6 - 2 * fi));
            }
            s_custom_data[phase_idx][t][bi] = byte_val;
        }
    }
}

/** 构建指定模式的波形数据 */
static void build_custom_waveform(epd_refresh_mode_t mode) {
    int total_phases = 0;

    switch (mode) {
    case EPD_REFRESH_FAST:
        for (int p = 0; p < CONTROL_PHASES; p++)
            encode_lut_phase(s_control_lut[p], total_phases++);
        break;
    case EPD_REFRESH_STANDARD:
        for (int p = 0; p < STANDARD_PREFIX; p++)
            encode_lut_phase(s_standard_prefix[p], total_phases++);
        for (int p = 0; p < CONTROL_PHASES; p++)
            encode_lut_phase(s_control_lut[p], total_phases++);
        break;
    case EPD_REFRESH_QUALITY:
        for (int p = 0; p < QUALITY_PREFIX; p++)
            encode_lut_phase(s_quality_prefix[p], total_phases++);
        for (int p = 0; p < CONTROL_PHASES; p++)
            encode_lut_phase(s_control_lut[p], total_phases++);
        break;
    }

    s_custom_phases.phases = total_phases;
    s_custom_phases.phase_times = NULL;
    s_custom_phases.luts = (const uint8_t *)s_custom_data;

    s_custom_ranges[0] = &s_custom_phases;

    s_custom_mode.type = 5;  /* MODE_GL16 */
    s_custom_mode.temp_ranges = 1;
    s_custom_mode.range_data = s_custom_ranges;

    s_custom_modes[0] = &s_custom_mode;

    s_custom_waveform.num_modes = 1;
    s_custom_waveform.num_temp_ranges = 1;
    s_custom_waveform.mode_data = s_custom_modes;
    s_custom_waveform.temp_intervals = s_custom_temp;
}

esp_err_t epd_driver_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "EPD driver already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing EPD driver (ED047TC2, 960x540)...");

    /* 初始化 epdiy: M5PaperS3 板级 + ED047TC2 显示屏 */
    epd_init(&epd_board_m5papers3, &ED047TC2, EPD_LUT_64K);

    /* 初始化高层状态，分配帧缓冲区（PSRAM） */
    s_hl_state = epd_hl_init(EPD_BUILTIN_WAVEFORM);
    s_framebuffer = epd_hl_get_framebuffer(&s_hl_state);

    if (s_framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM");
        epd_deinit();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Framebuffer allocated at %p", s_framebuffer);

    /* 全屏清除，消除上电残影 */
    ESP_LOGI(TAG, "Calling epd_fullclear...");
    epd_poweron();
    epd_fullclear(&s_hl_state, 25);
    epd_poweroff();
    ESP_LOGI(TAG, "epd_fullclear done");

    /* 预构建标准模式波形（默认） */
    build_custom_waveform(EPD_REFRESH_STANDARD);

    s_initialized = true;
    ESP_LOGI(TAG, "EPD driver initialized successfully");
    return ESP_OK;
}

void epd_driver_deinit(void) {
    if (!s_initialized) {
        return;
    }
    epd_poweroff();
    epd_deinit();
    s_framebuffer = NULL;
    s_initialized = false;
    ESP_LOGI(TAG, "EPD driver deinitialized");
}

uint8_t *epd_driver_get_framebuffer(void) {
    return s_framebuffer;
}

esp_err_t epd_driver_update_screen(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_GL16, 25);
    epd_poweroff();

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Screen update failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t epd_driver_update_area(int x, int y, int w, int h) {
    return epd_driver_update_area_mode(x, y, w, h, MODE_DU);
}

esp_err_t epd_driver_update_area_mode(int x, int y, int w, int h, int mode) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    EpdRect area = {.x = x, .y = y, .width = w, .height = h};

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_area(&s_hl_state, (enum EpdDrawMode)mode, 25, area);
    epd_poweroff();

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Area update failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void epd_driver_clear(void) {
    if (!s_initialized) {
        return;
    }
    epd_poweron();
    epd_fullclear(&s_hl_state, 25);
    epd_poweroff();
    ESP_LOGI(TAG, "Screen cleared");
}

void epd_driver_draw_pixel(int x, int y, uint8_t color) {
    if (s_framebuffer != NULL) {
        epd_draw_pixel(x, y, color, s_framebuffer);
    }
}

void epd_driver_fill_rect(int x, int y, int w, int h, uint8_t color) {
    if (s_framebuffer != NULL) {
        EpdRect rect = {.x = x, .y = y, .width = w, .height = h};
        epd_fill_rect(rect, color, s_framebuffer);
    }
}

void epd_driver_set_all_white(void) {
    if (s_initialized) {
        epd_hl_set_all_white(&s_hl_state);
    }
}

void epd_driver_set_all_black(void) {
    if (s_initialized) {
        memset(epd_hl_get_framebuffer(&s_hl_state), 0x00,
               epd_width() / 2 * epd_height());
    }
}

esp_err_t epd_driver_white_black_du_then_gl16(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    int fb_size = epd_width() / 2 * epd_height();

    /* 保存目标帧内容 */
    uint8_t *target = (uint8_t *)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (target == NULL) {
        ESP_LOGE(TAG, "Failed to allocate temp buffer for W>B>GL16");
        return ESP_ERR_NO_MEM;
    }
    memcpy(target, s_framebuffer, fb_size);

    epd_poweron();

    /* 第一步：DU 快速刷白 */
    memset(s_framebuffer, 0xFF, fb_size);
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_DU, 25);
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "White DU step failed: %d", err);
        memcpy(s_framebuffer, target, fb_size);
        heap_caps_free(target);
        epd_poweroff();
        return ESP_FAIL;
    }

    /* 第二步：DU 快速刷黑 */
    memset(s_framebuffer, 0x00, fb_size);
    err = epd_hl_update_screen(&s_hl_state, MODE_DU, 25);
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Black DU step failed: %d", err);
        memcpy(s_framebuffer, target, fb_size);
        heap_caps_free(target);
        epd_poweroff();
        return ESP_FAIL;
    }

    /* 第三步：GL16 显示目标内容 */
    memcpy(s_framebuffer, target, fb_size);
    err = epd_hl_update_screen(&s_hl_state, MODE_GL16, 25);
    epd_poweroff();

    heap_caps_free(target);

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "GL16 target step failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t epd_driver_update_screen_custom(epd_refresh_mode_t mode) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 构建指定模式的波形 */
    build_custom_waveform(mode);

    /* 临时切换到自定义波形 */
    const EpdWaveform *original = s_hl_state.waveform;
    s_hl_state.waveform = &s_custom_waveform;

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_GL16, 25);
    epd_poweroff();

    /* 恢复原始波形 */
    s_hl_state.waveform = original;

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Custom refresh (mode %d) failed: %d", mode, err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t epd_driver_update_screen_text_mode(void) {
    return epd_driver_update_screen_custom(EPD_REFRESH_STANDARD);
}
