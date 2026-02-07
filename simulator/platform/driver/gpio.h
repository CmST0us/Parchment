/**
 * @file gpio.h
 * @brief ESP-IDF driver/gpio.h stub for desktop simulator.
 */

#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int gpio_num_t;

/* GPIO number definitions used in board.h */
#define GPIO_NUM_NC  (-1)
#define GPIO_NUM_6    6
#define GPIO_NUM_7    7
#define GPIO_NUM_8    8
#define GPIO_NUM_9    9
#define GPIO_NUM_10  10
#define GPIO_NUM_11  11
#define GPIO_NUM_12  12
#define GPIO_NUM_13  13
#define GPIO_NUM_14  14
#define GPIO_NUM_15  15
#define GPIO_NUM_16  16
#define GPIO_NUM_17  17
#define GPIO_NUM_18  18
#define GPIO_NUM_38  38
#define GPIO_NUM_39  39
#define GPIO_NUM_40  40
#define GPIO_NUM_41  41
#define GPIO_NUM_42  42
#define GPIO_NUM_44  44
#define GPIO_NUM_45  45
#define GPIO_NUM_46  46
#define GPIO_NUM_47  47
#define GPIO_NUM_48  48

/* Stub types for gpio_config */
typedef enum { GPIO_MODE_INPUT = 0, GPIO_MODE_OUTPUT = 1 } gpio_mode_t;
typedef enum { GPIO_PULLUP_DISABLE = 0, GPIO_PULLUP_ENABLE = 1 } gpio_pullup_t;
typedef enum { GPIO_PULLDOWN_DISABLE = 0, GPIO_PULLDOWN_ENABLE = 1 } gpio_pulldown_t;
typedef enum { GPIO_INTR_DISABLE = 0, GPIO_INTR_NEGEDGE = 2 } gpio_int_type_t;

typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

static inline int gpio_config(const gpio_config_t *c) { (void)c; return 0; }
static inline int gpio_set_level(int pin, int level) { (void)pin; (void)level; return 0; }
static inline int gpio_install_isr_service(int flags) { (void)flags; return 0; }
static inline int gpio_isr_handler_add(int pin, void(*h)(void*), void *a) { (void)pin; (void)h; (void)a; return 0; }
static inline int gpio_isr_handler_remove(int pin) { (void)pin; return 0; }
static inline int gpio_intr_disable(int pin) { (void)pin; return 0; }

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_GPIO_H */
