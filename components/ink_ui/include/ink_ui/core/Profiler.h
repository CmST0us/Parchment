/**
 * @file Profiler.h
 * @brief InkUI 性能 Profiling 宏，通过 CONFIG_INKUI_PROFILE 条件编译控制。
 *
 * 提供管线阶段计时宏。关闭时所有宏编译为空，零运行时开销。
 * 开启方式：idf.py menuconfig → InkUI Configuration → Enable profiling
 */

#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_INKUI_PROFILE

#include "esp_timer.h"
#include "esp_log.h"

/// 开始计时（声明局部变量）
#define INKUI_PROFILE_BEGIN(name) \
    int64_t _prof_##name##_start = esp_timer_get_time()

/// 结束计时（声明结束时刻变量）
#define INKUI_PROFILE_END(name) \
    int64_t _prof_##name##_end = esp_timer_get_time()

/// 获取耗时（微秒）
#define INKUI_PROFILE_US(name) \
    ((int)(_prof_##name##_end - _prof_##name##_start))

/// 获取耗时（毫秒）
#define INKUI_PROFILE_MS(name) \
    ((int)((_prof_##name##_end - _prof_##name##_start) / 1000))

/// 获取当前时间戳（微秒）
#define INKUI_PROFILE_NOW() esp_timer_get_time()

/// 计算两个时间戳差值（毫秒）
#define INKUI_PROFILE_DIFF_MS(start, end) ((int)(((end) - (start)) / 1000))

/// Profiling 日志输出
#define INKUI_PROFILE_LOG(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)

#else  // !CONFIG_INKUI_PROFILE

#define INKUI_PROFILE_BEGIN(name)
#define INKUI_PROFILE_END(name)
#define INKUI_PROFILE_US(name) 0
#define INKUI_PROFILE_MS(name) 0
#define INKUI_PROFILE_NOW() ((int64_t)0)
#define INKUI_PROFILE_DIFF_MS(start, end) 0
#define INKUI_PROFILE_LOG(tag, fmt, ...)

#endif  // CONFIG_INKUI_PROFILE
