/**
 * @file DesktopFontProvider.cpp
 * @brief 桌面字体提供者实现。
 *
 * 直接调用 ui_font_init() 和 ui_font_get()。
 * 在模拟器环境中，FONTS_MOUNT_POINT 宏已被 CMake 重定义为本地目录，
 * esp_vfs_littlefs_register() 为空操作 stub，所以 opendir() 会直接
 * 扫描本地文件系统中的 .pfnt 文件。
 */

#include "DesktopFontProvider.h"

extern "C" {
#include "ui_font.h"
}

DesktopFontProvider::DesktopFontProvider(const std::string& fontsDir)
    : fontsDir_(fontsDir) {}

bool DesktopFontProvider::init() {
    ui_font_init();
    return true;
}

const EpdFont* DesktopFontProvider::getFont(int sizePx) {
    return ui_font_get(sizePx);
}
