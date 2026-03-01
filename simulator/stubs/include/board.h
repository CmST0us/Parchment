/**
 * @file board.h
 * @brief Simulator stub for M5Stack PaperS3 board definitions.
 * Provides the same constants as main/board.h without hardware dependencies.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* EPD 显示引脚 (stub values) */
#define BOARD_EPD_DATA0     6
#define BOARD_EPD_DATA1     14
#define BOARD_EPD_DATA2     7
#define BOARD_EPD_DATA3     12
#define BOARD_EPD_DATA4     9
#define BOARD_EPD_DATA5     11
#define BOARD_EPD_DATA6     8
#define BOARD_EPD_DATA7     10

#define BOARD_EPD_CKH       16
#define BOARD_EPD_CKV       18
#define BOARD_EPD_STH       13
#define BOARD_EPD_SPV       17
#define BOARD_EPD_XLE       15
#define BOARD_EPD_EN        45
#define BOARD_EPD_BST_EN    46

/* GT911 触摸引脚 */
#define BOARD_TOUCH_SDA     41
#define BOARD_TOUCH_SCL     42
#define BOARD_TOUCH_INT     48
#define BOARD_TOUCH_RST     (-1)
#define BOARD_TOUCH_I2C_NUM 1
#define BOARD_TOUCH_I2C_FREQ 400000

/* SD 卡引脚 */
#define BOARD_SD_MISO       40
#define BOARD_SD_MOSI       38
#define BOARD_SD_CLK        39
#define BOARD_SD_CS         47

/* 电池 ADC */
#define BOARD_BAT_ADC       3

/* 其他引脚 */
#define BOARD_PWR_HOLD      44

/* 显示参数 */
#define BOARD_EPD_WIDTH     960
#define BOARD_EPD_HEIGHT    540

/**
 * @brief Board init stub (no-op in simulator).
 */
void board_init(void);

#ifdef __cplusplus
}
#endif
