/**
 * @file board.h
 * @brief M5Stack PaperS3 板级引脚定义和初始化接口。
 *
 * 硬件配置:
 *   - MCU: ESP32-S3R8 (8MB PSRAM, 16MB Flash)
 *   - 显示: 4.7" E-Ink ED047TC2 (960x540, 16级灰度)
 *   - 触摸: GT911 电容触摸
 *   - 存储: MicroSD (SPI)
 */

#ifndef BOARD_H
#define BOARD_H

#include "driver/gpio.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  EPD 显示引脚 (8-bit 并行接口)                                              */
/* -------------------------------------------------------------------------- */
#define BOARD_EPD_DATA0     GPIO_NUM_6
#define BOARD_EPD_DATA1     GPIO_NUM_14
#define BOARD_EPD_DATA2     GPIO_NUM_7
#define BOARD_EPD_DATA3     GPIO_NUM_12
#define BOARD_EPD_DATA4     GPIO_NUM_9
#define BOARD_EPD_DATA5     GPIO_NUM_11
#define BOARD_EPD_DATA6     GPIO_NUM_8
#define BOARD_EPD_DATA7     GPIO_NUM_10

#define BOARD_EPD_CKH       GPIO_NUM_16   /* 像素时钟 */
#define BOARD_EPD_CKV       GPIO_NUM_18   /* 行时钟 */
#define BOARD_EPD_STH       GPIO_NUM_13   /* 起始脉冲 */
#define BOARD_EPD_SPV       GPIO_NUM_17   /* 帧起始脉冲 */
#define BOARD_EPD_XLE       GPIO_NUM_15   /* 锁存使能 */
#define BOARD_EPD_EN        GPIO_NUM_45   /* 显示使能 */
#define BOARD_EPD_BST_EN    GPIO_NUM_46   /* 升压使能 */

/* -------------------------------------------------------------------------- */
/*  GT911 触摸引脚 (I2C)                                                       */
/* -------------------------------------------------------------------------- */
#define BOARD_TOUCH_SDA     GPIO_NUM_41
#define BOARD_TOUCH_SCL     GPIO_NUM_42
#define BOARD_TOUCH_INT     GPIO_NUM_48
#define BOARD_TOUCH_RST     GPIO_NUM_NC   /* 未连接 */
#define BOARD_TOUCH_I2C_NUM I2C_NUM_1
#define BOARD_TOUCH_I2C_FREQ 400000

/* -------------------------------------------------------------------------- */
/*  SD 卡引脚 (SPI)                                                            */
/* -------------------------------------------------------------------------- */
#define BOARD_SD_MISO       GPIO_NUM_40
#define BOARD_SD_MOSI       GPIO_NUM_38
#define BOARD_SD_CLK        GPIO_NUM_39
#define BOARD_SD_CS         GPIO_NUM_47

/* -------------------------------------------------------------------------- */
/*  其他引脚                                                                    */
/* -------------------------------------------------------------------------- */
#define BOARD_PWR_HOLD      GPIO_NUM_44   /* 电源保持 */

/* -------------------------------------------------------------------------- */
/*  显示参数                                                                    */
/* -------------------------------------------------------------------------- */
#define BOARD_EPD_WIDTH     960
#define BOARD_EPD_HEIGHT    540

/**
 * @brief 初始化 M5PaperS3 板级硬件。
 *
 * 配置电源保持引脚，确保设备不会自动关机。
 */
void board_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
