/**
 * @file DesktopFontProvider.h
 * @brief 桌面字体提供者 -- 封装 ui_font 子系统供模拟器使用。
 *
 * 在模拟器环境中，FONTS_MOUNT_POINT 被 CMake 重定义为本地目录路径，
 * LittleFS 注册为空操作，因此 ui_font_init() 直接通过 opendir() 扫描
 * 本地 .pfnt 文件。
 */

#pragma once

#include "ink_ui/hal/FontProvider.h"

#include <string>

/// 桌面字体提供者实现
class DesktopFontProvider : public ink::FontProvider {
public:
    /**
     * @brief 构造桌面字体提供者。
     * @param fontsDir 字体文件所在目录路径（供参考记录，实际路径由 FONTS_MOUNT_POINT 宏控制）
     */
    explicit DesktopFontProvider(const std::string& fontsDir);

    // -- FontProvider 接口 --
    bool init() override;
    const EpdFont* getFont(int sizePx) override;

private:
    std::string fontsDir_;
};
