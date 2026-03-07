/**
 * @file RenderEngine.h
 * @brief 墨水屏渲染引擎 — 4 阶段渲染循环。
 *
 * Layout → 收集脏区域 → 重绘 → 快速刷新。
 */

#pragma once

#include "ink_ui/core/Geometry.h"
#include "ink_ui/core/Profiler.h"
#include "ink_ui/core/View.h"
#include "ink_ui/hal/DisplayDriver.h"

namespace ink {

/// 最大独立脏区域数量
constexpr int MAX_DIRTY_REGIONS = 8;

/// 脏区域条目
struct DirtyEntry {
    Rect rect;
    RefreshHint hint = RefreshHint::Auto;
};

/// 墨水屏渲染引擎
class RenderEngine {
public:
    /// 构造，绑定 DisplayDriver
    explicit RenderEngine(DisplayDriver& driver);

    /// 执行一次完整渲染循环（5 阶段）
    void renderCycle(View* rootView);

    /// 损伤修复：对指定区域裁剪重绘并刷新
    void repairDamage(View* rootView, const Rect& damage);

    /// 设置下一次 flush 使用过渡刷新模式（W>B>GL）
    void setPendingTransition();

private:
    DisplayDriver& driver_;
    uint8_t* fb_;

    DirtyEntry dirtyRegions_[MAX_DIRTY_REGIONS];
    int dirtyCount_ = 0;
    bool pendingTransition_ = false;  ///< 下一次 flush 使用 W>B>GL 过渡

#ifdef CONFIG_INKUI_PROFILE
    int64_t profClearUs_ = 0;    ///< canvas.clear() 累计耗时
    int64_t profOnDrawUs_ = 0;   ///< view->onDraw() 累计耗时
    int profViewCount_ = 0;      ///< 绘制的 View 数量
#endif

    // Phase 1: Layout
    void layoutPass(View* rootView);

    // Phase 2: Collect dirty regions
    void collectDirty(View* view);

    // Phase 3: Draw dirty views
    void drawDirty(View* rootView);
    void drawView(View* view, bool forced);

    // Phase 4: Flush to EPD
    void flush();

    /// 添加一个脏区域
    void addDirtyRegion(const Rect& rect, RefreshHint hint);

    /// 合并相邻且同 hint 的区域
    void mergeOverlappingRegions();

    /// 在 damage 区域内重绘 View 子树（用于 repairDamage）
    void repairDrawView(View* view, const Rect& damage);

    /// 逻辑坐标矩形 → 物理坐标矩形
    static Rect logicalToPhysical(const Rect& lr);
};

} // namespace ink
