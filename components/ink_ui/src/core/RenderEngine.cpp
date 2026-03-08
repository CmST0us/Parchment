/**
 * @file RenderEngine.cpp
 * @brief 墨水屏渲染引擎实现。
 *
 * 4 阶段渲染循环：Layout → 收集脏区域 → 重绘 → 快速刷新。
 */

#include "ink_ui/core/RenderEngine.h"
#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/Profiler.h"

#include <cstdio>

namespace ink {

/// 递归清除 View 树的脏标记
static void clearDirtyFlagsRecursive(View* view) {
    view->clearDirtyFlags();
    for (auto& child : view->subviews()) {
        clearDirtyFlagsRecursive(child.get());
    }
}

RenderEngine::RenderEngine(DisplayDriver& driver)
    : driver_(driver)
    , fb_(driver.framebuffer()) {
}

// ── 主渲染循环 ──

void RenderEngine::renderCycle(View* rootView) {
    if (!rootView) return;

    // Phase 1: Layout Pass
    INKUI_PROFILE_BEGIN(layout);
    layoutPass(rootView);
    INKUI_PROFILE_END(layout);

    // Phase 2: Collect Dirty
    INKUI_PROFILE_BEGIN(collect);
    dirtyCount_ = 0;
    collectDirty(rootView);
    INKUI_PROFILE_END(collect);

    if (dirtyCount_ == 0) {
        pendingTransition_ = false;
        return;  // 无脏区域，跳过后续阶段
    }

    // Phase 2.5: Transition — W>B 全屏 DU 快刷消残影
    if (pendingTransition_) {
        INKUI_PROFILE_BEGIN(transition);
        driver_.setAllWhite();
        driver_.updateArea(0, 0, driver_.width(), driver_.height(),
                           RefreshMode::Fast);
        driver_.setAllBlack();
        driver_.updateArea(0, 0, driver_.width(), driver_.height(),
                           RefreshMode::Fast);
        pendingTransition_ = false;
        INKUI_PROFILE_END(transition);
    }

    // Phase 3: Draw Dirty
#ifdef CONFIG_INKUI_PROFILE
    profClearUs_ = 0;
    profOnDrawUs_ = 0;
    profViewCount_ = 0;
#endif

    INKUI_PROFILE_BEGIN(draw);
    drawDirty(rootView);
    INKUI_PROFILE_END(draw);

    // Phase 4: Flush
    INKUI_PROFILE_BEGIN(flush);
    flush();
    INKUI_PROFILE_END(flush);

#ifdef CONFIG_INKUI_PROFILE
    int maxW = 0, maxH = 0;
    for (int i = 0; i < dirtyCount_; i++) {
        if (dirtyRegions_[i].rect.w > maxW) maxW = dirtyRegions_[i].rect.w;
        if (dirtyRegions_[i].rect.h > maxH) maxH = dirtyRegions_[i].rect.h;
    }
    INKUI_PROFILE_LOG("PERF", "cycle: layout=%dms collect=%dms draw=%dms flush=%dms total=%dms dirty=%d (%dx%d)",
        INKUI_PROFILE_MS(layout), INKUI_PROFILE_MS(collect),
        INKUI_PROFILE_MS(draw), INKUI_PROFILE_MS(flush),
        INKUI_PROFILE_MS(layout) + INKUI_PROFILE_MS(collect) +
        INKUI_PROFILE_MS(draw) + INKUI_PROFILE_MS(flush),
        dirtyCount_, maxW, maxH);
    INKUI_PROFILE_LOG("PERF", "  draw: clear=%dms onDraw=%dms views=%d",
        (int)(profClearUs_ / 1000), (int)(profOnDrawUs_ / 1000), profViewCount_);
#endif
}

// ── Phase 1: Layout Pass ──

void RenderEngine::layoutPass(View* rootView) {
    if (rootView->needsLayout()) {
        rootView->onLayout();
    }
}

// ── Phase 2: Collect Dirty ──

void RenderEngine::collectDirty(View* view) {
    if (view->isHidden()) return;

    if (view->needsDisplay()) {
        addDirtyRegion(view->screenFrame(), view->refreshHint());
    } else if (view->subtreeNeedsDisplay()) {
        for (auto& child : view->subviews()) {
            collectDirty(child.get());
        }
    }
}

// ── Phase 3: Draw Dirty ──

void RenderEngine::drawDirty(View* rootView) {
    drawView(rootView, false);
    clearDirtyFlagsRecursive(rootView);
}

void RenderEngine::drawView(View* view, bool forced) {
    if (view->isHidden()) return;

    bool shouldDraw = forced || view->needsDisplay();

    if (shouldDraw) {
        Rect sf = view->screenFrame();
        Canvas canvas(fb_, sf);

#ifdef CONFIG_INKUI_PROFILE
        int64_t clearStart = INKUI_PROFILE_NOW();
#endif
        if (view->backgroundColor() != Color::Clear) {
            canvas.clear(view->backgroundColor());
        } else if (!forced && view->isOpaque()) {
            // 不透明的透明背景 View 自身内容变化时，用最近祖先的背景色清除旧内容
            // 非不透明 View（如 overlayRoot_）跳过清除，保留底层内容
            uint8_t inheritedBg = Color::White;
            for (View* p = view->parent(); p; p = p->parent()) {
                if (p->backgroundColor() != Color::Clear) {
                    inheritedBg = p->backgroundColor();
                    break;
                }
            }
            canvas.clear(inheritedBg);
        }
#ifdef CONFIG_INKUI_PROFILE
        int64_t clearEnd = INKUI_PROFILE_NOW();
        profClearUs_ += (clearEnd - clearStart);

        int64_t drawStart = INKUI_PROFILE_NOW();
#endif
        view->onDraw(canvas);
#ifdef CONFIG_INKUI_PROFILE
        int64_t drawEnd = INKUI_PROFILE_NOW();
        profOnDrawUs_ += (drawEnd - drawStart);
        profViewCount_++;
#endif

        // 强制重绘所有子 View（父 View 的 clear 覆盖了子 View 区域）
        for (auto& child : view->subviews()) {
            drawView(child.get(), true);
        }
    } else if (view->subtreeNeedsDisplay()) {
        // 递归进入子 View，不重绘父 View
        for (auto& child : view->subviews()) {
            drawView(child.get(), false);
        }
    }
}

// ── Phase 4: Flush ──

/// RefreshHint → RefreshMode 映射
static RefreshMode hintToMode(RefreshHint hint) {
    switch (hint) {
        case RefreshHint::Fast:     return RefreshMode::TextFast;
        case RefreshHint::Standard: return RefreshMode::TextStd;
        case RefreshHint::Quality:  return RefreshMode::TextQuality;
        case RefreshHint::Full:     return RefreshMode::Clear;
        default:                    return RefreshMode::Full;  // Auto → GL16
    }
}

void RenderEngine::flush() {
    mergeOverlappingRegions();

    for (int i = 0; i < dirtyCount_; i++) {
        Rect phys = logicalToPhysical(dirtyRegions_[i].rect);
        RefreshMode mode = hintToMode(dirtyRegions_[i].hint);
        driver_.updateArea(phys.x, phys.y, phys.w, phys.h, mode);
    }
}

void RenderEngine::setPendingTransition() {
    pendingTransition_ = true;
}

// ── 辅助函数 ──

void RenderEngine::addDirtyRegion(const Rect& rect, RefreshHint hint) {
    if (rect.isEmpty()) return;

    if (dirtyCount_ < MAX_DIRTY_REGIONS) {
        dirtyRegions_[dirtyCount_] = {rect, hint};
        dirtyCount_++;
    } else {
        // 区域数量超限，合并后再添加
        mergeOverlappingRegions();
        if (dirtyCount_ < MAX_DIRTY_REGIONS) {
            dirtyRegions_[dirtyCount_] = {rect, hint};
            dirtyCount_++;
        } else {
            // 仍然满了，与最后一个合并
            dirtyRegions_[MAX_DIRTY_REGIONS - 1].rect =
                dirtyRegions_[MAX_DIRTY_REGIONS - 1].rect.unionWith(rect);
        }
    }
}

void RenderEngine::mergeOverlappingRegions() {
    bool merged = true;
    while (merged) {
        merged = false;
        for (int i = 0; i < dirtyCount_ && !merged; i++) {
            for (int j = i + 1; j < dirtyCount_ && !merged; j++) {
                if (dirtyRegions_[i].hint != dirtyRegions_[j].hint) continue;

                const Rect& a = dirtyRegions_[i].rect;
                const Rect& b = dirtyRegions_[j].rect;

                // 检查是否相邻或重叠（8px 容差）
                bool adjacent = a.intersects(b) ||
                    (a.bottom() + 8 >= b.y && b.bottom() + 8 >= a.y &&
                     a.right() + 8 >= b.x && b.right() + 8 >= a.x);

                if (adjacent) {
                    dirtyRegions_[i].rect = a.unionWith(b);
                    // 移除 j
                    for (int k = j; k < dirtyCount_ - 1; k++) {
                        dirtyRegions_[k] = dirtyRegions_[k + 1];
                    }
                    dirtyCount_--;
                    merged = true;
                }
            }
        }
    }
}

Rect RenderEngine::logicalToPhysical(const Rect& lr) {
    // 逻辑坐标 (540x960 portrait) → 物理坐标 (960x540 landscape)
    // physical_x = logical_y, physical_y = 540 - logical_x - logical_w
    return {lr.y, kScreenWidth - lr.x - lr.w, lr.h, lr.w};
}

void RenderEngine::repairDamage(View* rootView, const Rect& damage) {
    if (!rootView || damage.isEmpty()) return;

    repairDrawView(rootView, damage);

    Rect phys = logicalToPhysical(damage);
    driver_.updateArea(phys.x, phys.y, phys.w, phys.h, RefreshMode::Full);
}

void RenderEngine::repairDrawView(View* view, const Rect& damage) {
    if (view->isHidden()) return;

    Rect sf = view->screenFrame();
    if (!sf.intersects(damage)) return;

    Canvas canvas(fb_, sf);
    if (view->backgroundColor() != Color::Clear) {
        // 仅清除 damage 与当前 view 的交集区域，避免破坏 damage 外的 framebuffer
        Rect inter = sf.intersection(damage);
        Rect local = {inter.x - sf.x, inter.y - sf.y, inter.w, inter.h};
        canvas.fillRect(local, view->backgroundColor());
    }
    view->onDraw(canvas);

    for (auto& child : view->subviews()) {
        repairDrawView(child.get(), damage);
    }
}

} // namespace ink
