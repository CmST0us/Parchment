/**
 * @file GestureRecognizer.h
 * @brief 触摸手势识别器 — 从 GT911 原始数据识别 Tap / LongPress / Swipe。
 *
 * 运行在独立 FreeRTOS 任务中，通过 GT911 中断/轮询读取触摸数据，
 * 识别手势后发送 Event 到 Application 的事件队列。
 */

#pragma once

#include "ink_ui/core/Event.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
}

namespace ink {

/// 触摸手势识别器
class GestureRecognizer {
public:
    /// 构造，绑定事件队列
    explicit GestureRecognizer(QueueHandle_t eventQueue);
    ~GestureRecognizer();

    // 不可拷贝
    GestureRecognizer(const GestureRecognizer&) = delete;
    GestureRecognizer& operator=(const GestureRecognizer&) = delete;

    /// 创建触摸任务并开始识别
    bool start();

    /// 停止触摸任务
    void stop();

private:
    QueueHandle_t eventQueue_;
    SemaphoreHandle_t touchSem_ = nullptr;
    TaskHandle_t taskHandle_ = nullptr;
    volatile bool running_ = false;

    // ── 手势识别参数 ──
    static constexpr int kTapMaxDist       = 20;    // px
    static constexpr int kTapMaxTimeMs     = 500;   // ms
    static constexpr int kSwipeMinDist     = 40;    // px
    static constexpr int kLongPressMs      = 800;   // ms
    static constexpr int kLongPressDist    = 20;    // px
    static constexpr int kPollIntervalMs   = 20;    // ms
    static constexpr int kTaskStackSize    = 4096;
    static constexpr int kTaskPriority     = 6;
    static constexpr int kIntGpio          = 48;    // GT911 INT

    // ── 手势状态 ──
    enum class GestureState {
        Idle,
        Tracking,
        LongFired,
    };

    GestureState state_ = GestureState::Idle;
    int startX_ = 0;
    int startY_ = 0;
    int currentX_ = 0;
    int currentY_ = 0;
    int64_t startTimeUs_ = 0;

    /// 发送事件到队列
    void sendEvent(const Event& event);

    /// 坐标映射（GT911 直通，裁剪到有效范围）
    static void mapCoords(uint16_t tx, uint16_t ty, int& sx, int& sy);

    /// 距离的平方
    static int distanceSq(int x0, int y0, int x1, int y1);

    // ── 手势状态机 ──
    void onPress(int x, int y);
    void onMove(int x, int y);
    void onRelease();

    /// 触摸任务入口（static wrapper）
    static void taskEntry(void* arg);

    /// 触摸任务主循环
    void taskLoop();

    /// GPIO 中断处理（IRAM）
    static void IRAM_ATTR isrHandler(void* arg);
};

} // namespace ink
