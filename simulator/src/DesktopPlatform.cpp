/**
 * @file DesktopPlatform.cpp
 * @brief 桌面平台 HAL 实现。
 *
 * 队列使用 deque + mutex + condition_variable 实现。
 * 任务和定时器使用 detached std::thread 实现。
 */

#include "DesktopPlatform.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

/// 模拟器队列内部结构
struct SimQueue {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::vector<uint8_t>> items;
    int itemSize;
    int maxItems;
};

ink::QueueHandle DesktopPlatform::createQueue(int maxItems, int itemSize) {
    auto* q = new SimQueue();
    q->itemSize = itemSize;
    q->maxItems = maxItems;
    return q;
}

bool DesktopPlatform::queueSend(ink::QueueHandle queue, const void* item,
                                 uint32_t timeout_ms) {
    auto* q = static_cast<SimQueue*>(queue);
    std::unique_lock<std::mutex> lock(q->mutex);

    // 如果队列已满，等待空间
    if (static_cast<int>(q->items.size()) >= q->maxItems) {
        if (timeout_ms == 0) {
            return false;
        }
        bool ok = q->cv.wait_for(
            lock, std::chrono::milliseconds(timeout_ms),
            [q] { return static_cast<int>(q->items.size()) < q->maxItems; });
        if (!ok) return false;
    }

    // 拷贝数据到 vector 并入队
    std::vector<uint8_t> data(q->itemSize);
    std::memcpy(data.data(), item, q->itemSize);
    q->items.push_back(std::move(data));
    q->cv.notify_one();
    return true;
}

bool DesktopPlatform::queueReceive(ink::QueueHandle queue, void* item,
                                    uint32_t timeout_ms) {
    auto* q = static_cast<SimQueue*>(queue);
    std::unique_lock<std::mutex> lock(q->mutex);

    if (q->items.empty()) {
        if (timeout_ms == 0) {
            return false;
        }
        bool ok = q->cv.wait_for(
            lock, std::chrono::milliseconds(timeout_ms),
            [q] { return !q->items.empty(); });
        if (!ok) return false;
    }

    std::memcpy(item, q->items.front().data(), q->itemSize);
    q->items.pop_front();
    q->cv.notify_one();  // 通知可能等待入队的发送方
    return true;
}

ink::TaskHandle DesktopPlatform::createTask(void(*entry)(void*),
                                             const char* /*name*/,
                                             int /*stackSize*/, void* arg,
                                             int /*priority*/) {
    auto* t = new std::thread(entry, arg);
    t->detach();
    return t;
}

bool DesktopPlatform::startOneShotTimer(uint32_t delayMs,
                                         ink::TimerCallback callback,
                                         void* arg) {
    std::thread([delayMs, callback, arg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        callback(arg);
    }).detach();
    return true;
}

int64_t DesktopPlatform::getTimeUs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
               now.time_since_epoch())
        .count();
}

void DesktopPlatform::delayMs(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
