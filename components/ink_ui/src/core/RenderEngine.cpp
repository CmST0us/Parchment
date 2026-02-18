/**
 * @file RenderEngine.cpp
 * @brief 墨水屏渲染引擎实现。
 *
 * 5 阶段渲染循环：Layout → 收集脏区域 → 重绘 → 多区域刷新 → 残影管理。
 */

#include "ink_ui/core/RenderEngine.h"
#include "ink_ui/core/Canvas.h"

extern "C" {
#include "esp_log.h"
}

static const char* TAG = "ink::RenderEngine";

namespace ink {

/// 递归清除 View 树的脏标记
static void clearDirtyFlagsRecursive(View* view) {
    view->clearDirtyFlags();
    for (auto& child : view->subviews()) {
        clearDirtyFlagsRecursive(child.get());
    }
}

RenderEngine::RenderEngine(EpdDriver& driver)
    : driver_(driver)
    , fb_(driver.framebuffer()) {
}

// ── 主渲染循环 ──

void RenderEngine::renderCycle(View* rootView) {
    if (!rootView) return;

    // Phase 1: Layout Pass
    layoutPass(rootView);

    // Phase 2: Collect Dirty
    dirtyCount_ = 0;
    collectDirty(rootView);

    if (dirtyCount_ == 0) {
        return;  // 无脏区域，跳过 Phase 3-5
    }

    // Phase 3: Draw Dirty
    drawDirty(rootView);

    // Phase 4: Flush
    flush();

    // Phase 5: Ghost Management
    ghostManagement();
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
        // 透明背景的 View 不清除区域，保留父 View 已绘制的内容
        if (view->backgroundColor() != Color::Transparent) {
            canvas.clear(view->backgroundColor());
        }
        view->onDraw(canvas);

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

void RenderEngine::flush() {
    mergeOverlappingRegions();

    // 检查是否退化为全屏（合并面积 > 60%）
    int totalArea = 0;
    for (int i = 0; i < dirtyCount_; i++) {
        totalArea += dirtyRegions_[i].rect.area();
    }
    int screenArea = kScreenWidth * kScreenHeight;

    if (totalArea > screenArea * 60 / 100) {
        // 退化为全屏 Quality 刷新
        Rect fullScreen = {0, 0, kScreenWidth, kScreenHeight};
        Rect phys = logicalToPhysical(fullScreen);
        driver_.updateArea(phys, EpdMode::GL16);
        partialCount_++;
        return;
    }

    // 逐区域刷新
    for (int i = 0; i < dirtyCount_; i++) {
        Rect phys = logicalToPhysical(dirtyRegions_[i].rect);
        EpdMode mode = hintToMode(dirtyRegions_[i].hint);
        driver_.updateArea(phys, mode);

        if (mode != EpdMode::GC16) {
            partialCount_++;
        }
    }
}

// ── Phase 5: Ghost Management ──

void RenderEngine::ghostManagement() {
    if (partialCount_ >= GHOST_THRESHOLD) {
        ESP_LOGI(TAG, "Ghost threshold reached (%d), executing GC16 full clear",
                 partialCount_);
        driver_.fullClear();
        partialCount_ = 0;
    }
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

EpdMode RenderEngine::hintToMode(RefreshHint hint) {
    switch (hint) {
        case RefreshHint::Fast:    return EpdMode::DU;
        case RefreshHint::Quality: return EpdMode::GL16;
        case RefreshHint::Full:    return EpdMode::GC16;
        case RefreshHint::Auto:    return EpdMode::GL16;
        default:                   return EpdMode::GL16;
    }
}

Rect RenderEngine::logicalToPhysical(const Rect& lr) {
    // 逻辑坐标 (540x960 portrait) → 物理坐标 (960x540 landscape)
    // physical_x = logical_y, physical_y = 540 - logical_x - logical_w
    return {lr.y, kScreenWidth - lr.x - lr.w, lr.h, lr.w};
}

void RenderEngine::forceFullClear() {
    ESP_LOGI(TAG, "Force full clear (GC16)");
    driver_.fullClear();
    partialCount_ = 0;
}

void RenderEngine::repairDamage(View* rootView, const Rect& damage) {
    if (!rootView || damage.isEmpty()) return;

    repairDrawView(rootView, damage);

    Rect phys = logicalToPhysical(damage);
    driver_.updateArea(phys, EpdMode::GL16);
    partialCount_++;

    ghostManagement();
}

void RenderEngine::repairDrawView(View* view, const Rect& damage) {
    if (view->isHidden()) return;

    Rect sf = view->screenFrame();
    if (!sf.intersects(damage)) return;

    Canvas canvas(fb_, sf);
    if (view->backgroundColor() != Color::Transparent) {
        canvas.clear(view->backgroundColor());
    }
    view->onDraw(canvas);

    for (auto& child : view->subviews()) {
        repairDrawView(child.get(), damage);
    }
}

} // namespace ink
