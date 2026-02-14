/**
 * @file RenderEngine.h
 * @brief 墨水屏渲染引擎 — 5 阶段渲染循环。
 *
 * Layout → 收集脏区域 → 重绘 → 多区域刷新 → 残影管理。
 */

#pragma once

#include "ink_ui/core/Geometry.h"
#include "ink_ui/core/View.h"
#include "ink_ui/core/EpdDriver.h"

namespace ink {

/// 最大独立脏区域数量
constexpr int MAX_DIRTY_REGIONS = 8;

/// 残影阈值：累计多少次局部刷新后自动 GC16
constexpr int GHOST_THRESHOLD = 10;

/// 脏区域条目
struct DirtyEntry {
    Rect rect;
    RefreshHint hint = RefreshHint::Auto;
};

/// 墨水屏渲染引擎
class RenderEngine {
public:
    /// 构造，绑定 EpdDriver
    explicit RenderEngine(EpdDriver& driver);

    /// 执行一次完整渲染循环（5 阶段）
    void renderCycle(View* rootView);

    /// 损伤修复：对指定区域裁剪重绘并刷新
    void repairDamage(View* rootView, const Rect& damage);

    /// 立即执行全屏 GC16 清除残影
    void forceFullClear();

    /// 获取局部刷新计数
    int partialCount() const { return partialCount_; }

private:
    EpdDriver& driver_;
    uint8_t* fb_;
    int partialCount_ = 0;

    DirtyEntry dirtyRegions_[MAX_DIRTY_REGIONS];
    int dirtyCount_ = 0;

    // Phase 1: Layout
    void layoutPass(View* rootView);

    // Phase 2: Collect dirty regions
    void collectDirty(View* view);

    // Phase 3: Draw dirty views
    void drawDirty(View* rootView);
    void drawView(View* view, bool forced);

    // Phase 4: Flush to EPD
    void flush();

    // Phase 5: Ghost management
    void ghostManagement();

    /// 添加一个脏区域
    void addDirtyRegion(const Rect& rect, RefreshHint hint);

    /// 合并相邻且同 hint 的区域
    void mergeOverlappingRegions();

    /// 在 damage 区域内重绘 View 子树（用于 repairDamage）
    void repairDrawView(View* view, const Rect& damage);

    /// RefreshHint → EpdMode 映射
    static EpdMode hintToMode(RefreshHint hint);

    /// 逻辑坐标矩形 → 物理坐标矩形
    static Rect logicalToPhysical(const Rect& lr);
};

} // namespace ink
