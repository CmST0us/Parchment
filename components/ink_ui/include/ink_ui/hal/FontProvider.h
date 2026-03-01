/**
 * @file FontProvider.h
 * @brief 字体加载 HAL 接口 -- 抽象字体资源访问。
 *
 * 定义与具体存储无关的字体加载接口。ESP32 从 LittleFS 加载 .pfnt，
 * SDL 模拟器可从本地文件系统加载。
 */

#pragma once

struct EpdFont;  // epdiy 字体结构前向声明

namespace ink {

/// 字体提供者抽象接口
class FontProvider {
public:
    virtual ~FontProvider() = default;

    /// 初始化字体子系统
    virtual bool init() = 0;

    /// 获取指定像素大小的字体（返回 nullptr 表示未找到）
    virtual const EpdFont* getFont(int sizePx) = 0;
};

} // namespace ink
