/**
 * @file RenderEngine.h
 * @brief 墨水屏渲染引擎 — 4 阶段渲染循环 (M5GFX 后端)。
 *
 * Layout → 收集脏区域 → 重绘 → M5GFX display() 刷新。
 */

#pragma once

#include "ink_ui/core/Geometry.h"
#include "ink_ui/core/View.h"

// 前向声明 M5GFX
namespace m5gfx { class M5GFX; }
using M5GFX = m5gfx::M5GFX;

namespace ink {

/// 最大独立脏区域数量
constexpr int MAX_DIRTY_REGIONS = 8;

/// 脏区域条目
struct DirtyEntry {
    Rect rect;
    RefreshHint hint = RefreshHint::Auto;
};

/// 墨水屏渲染引擎 (M5GFX 后端)
class RenderEngine {
public:
    /// 构造，绑定 M5GFX display
    explicit RenderEngine(M5GFX* display);

    /// 执行一次完整渲染循环（4 阶段）
    void renderCycle(View* rootView);

    /// 损伤修复：对指定区域裁剪重绘并刷新
    void repairDamage(View* rootView, const Rect& damage);

private:
    M5GFX* display_;

    DirtyEntry dirtyRegions_[MAX_DIRTY_REGIONS];
    int dirtyCount_ = 0;

    // Phase 1: Layout
    void layoutPass(View* rootView);

    // Phase 2: Collect dirty regions
    void collectDirty(View* view);

    // Phase 3: Draw dirty views
    void drawDirty(View* rootView);
    void drawView(View* view, bool forced);

    // Phase 4: Flush to EPD (via M5GFX display())
    void flush();

    /// 添加一个脏区域
    void addDirtyRegion(const Rect& rect, RefreshHint hint);

    /// 合并相邻且同 hint 的区域
    void mergeOverlappingRegions();

    /// 在 damage 区域内重绘 View 子树（用于 repairDamage）
    void repairDrawView(View* view, const Rect& damage);
};

} // namespace ink
