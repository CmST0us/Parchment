/**
 * @file epd_driver.h
 * @brief E-Ink 显示驱动封装层。
 *
 * 对 epdiy 库进行封装，提供适用于 Parchment 阅读器的高层接口。
 * 内部管理帧缓冲区和 epdiy 高层状态。
 */

#ifndef EPD_DRIVER_H
#define EPD_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 E-Ink 显示驱动。
 *
 * 调用 epdiy 初始化 M5PaperS3 板级定义和 ED047TC2 显示屏，
 * 分配 PSRAM 帧缓冲区，并执行全屏清除。
 *
 * @return ESP_OK 成功，ESP_FAIL 失败。
 */
esp_err_t epd_driver_init(void);

/**
 * @brief 反初始化 E-Ink 显示驱动，释放资源。
 */
void epd_driver_deinit(void);

/**
 * @brief 获取帧缓冲区指针。
 *
 * 返回的缓冲区为 4bpp 灰度格式（每字节2像素）。
 * 可直接写入像素数据，然后调用刷新函数更新屏幕。
 *
 * @return 帧缓冲区指针，未初始化时返回 NULL。
 */
uint8_t *epd_driver_get_framebuffer(void);

/**
 * @brief 全屏刷新。
 *
 * 将帧缓冲区内容完整输出到屏幕（MODE_GL16）。
 *
 * @return ESP_OK 成功。
 */
esp_err_t epd_driver_update_screen(void);

/**
 * @brief 局部刷新指定区域（MODE_DU）。
 *
 * @param x      区域左上角 X 坐标。
 * @param y      区域左上角 Y 坐标。
 * @param w      区域宽度。
 * @param h      区域高度。
 * @return ESP_OK 成功。
 */
esp_err_t epd_driver_update_area(int x, int y, int w, int h);

/**
 * @brief 局部刷新指定区域（指定 epdiy 刷新模式）。
 *
 * @param x      区域左上角 X 坐标（物理坐标）。
 * @param y      区域左上角 Y 坐标（物理坐标）。
 * @param w      区域宽度。
 * @param h      区域高度。
 * @param mode   epdiy 刷新模式（MODE_DU / MODE_GL16 / MODE_GC16 等）。
 * @return ESP_OK 成功。
 */
esp_err_t epd_driver_update_area_mode(int x, int y, int w, int h, int mode);

/**
 * @brief 全屏清除为白色。
 *
 * 清除帧缓冲区并刷新屏幕，消除残影。
 */
void epd_driver_clear(void);

/**
 * @brief 在帧缓冲区中绘制单个像素。
 *
 * @param x     X 坐标 (0 ~ 959)。
 * @param y     Y 坐标 (0 ~ 539)。
 * @param color 灰度值，高 4 位有效（0x00=黑, 0xF0=白）。
 */
void epd_driver_draw_pixel(int x, int y, uint8_t color);

/**
 * @brief 在帧缓冲区中填充矩形区域。
 *
 * @param x     左上角 X 坐标。
 * @param y     左上角 Y 坐标。
 * @param w     宽度。
 * @param h     高度。
 * @param color 灰度值，高 4 位有效。
 */
void epd_driver_fill_rect(int x, int y, int w, int h, uint8_t color);

/**
 * @brief 白 DU → GL16 全屏切换。
 *
 * 第一步用 MODE_DU 快速将屏幕刷白，
 * 第二步用 MODE_GL16 从纯白显示目标内容。
 *
 * @return ESP_OK 成功。
 */
esp_err_t epd_driver_white_du_then_gl16(void);

/**
 * @brief 白 DU → 黑 DU → GL16 全屏切换。
 *
 * 第一步用 MODE_DU 快速将屏幕刷白，
 * 第二步用 MODE_DU 快速将屏幕刷黑，
 * 第三步用 MODE_GL16 从纯黑显示目标内容。
 * 通过白→黑两步彻底消除残影。
 *
 * @return ESP_OK 成功。
 */
esp_err_t epd_driver_white_black_du_then_gl16(void);

/**
 * @brief 文字模式全屏刷新（参考 M5GFX 先白后涂策略）。
 *
 * 使用自定义 14 相位波形：
 * Phase 0-1: 消去，将当前像素偏移到中灰（减少残影）
 * Phase 2-13: 先白后涂，渐变刷白后逐级涂至目标灰度
 * 单次调用完成，约 112ms，支持 16 级灰度，无闪烁。
 *
 * @return ESP_OK 成功。
 */
esp_err_t epd_driver_update_screen_text_mode(void);

/**
 * @brief 将帧缓冲区全部设为白色（不刷新屏幕）。
 */
void epd_driver_set_all_white(void);

#ifdef __cplusplus
}
#endif

#endif /* EPD_DRIVER_H */
