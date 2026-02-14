/**
 * @file View.h
 * @brief View 基类 — InkUI 组件树的骨架。
 *
 * 所有 UI 组件继承自 View。提供树操作、几何属性、脏标记冒泡、
 * 命中测试和触摸事件冒泡。
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "ink_ui/core/Geometry.h"
#include "ink_ui/core/Event.h"
#include "ink_ui/core/FlexLayout.h"

namespace ink {

class Canvas;

/// EPD 刷新提示，View 携带此提示供 RenderEngine 选择刷新模式
enum class RefreshHint {
    Fast,       ///< MODE_DU，快速单色刷新
    Quality,    ///< MODE_GL16，灰度准确不闪
    Full,       ///< MODE_GC16，闪黑消残影
    Auto,       ///< 由 RenderEngine 根据内容自动决定
};

/// View 基类
class View {
public:
    View();
    virtual ~View();

    // 不可拷贝
    View(const View&) = delete;
    View& operator=(const View&) = delete;

    // ── 树操作 ──

    /// 添加子 View，设置其 parent_ 指针
    void addSubview(std::unique_ptr<View> child);

    /// 从父 View 中移除自身，返回 unique_ptr 归还所有权
    std::unique_ptr<View> removeFromParent();

    /// 移除所有子 View
    void clearSubviews();

    /// 返回父 View 指针（非拥有）
    View* parent() const { return parent_; }

    /// 返回子 View 列表的 const 引用
    const std::vector<std::unique_ptr<View>>& subviews() const { return subviews_; }

    // ── 几何属性 ──

    /// 设置 View 在父坐标系中的 frame
    void setFrame(const Rect& frame);

    /// 获取 frame（父坐标系中的位置和尺寸）
    Rect frame() const { return frame_; }

    /// 获取 bounds（{0, 0, frame_.w, frame_.h}）
    Rect bounds() const { return {0, 0, frame_.w, frame_.h}; }

    /// 计算屏幕绝对坐标（递归累加祖先 origin）
    Rect screenFrame() const;

    // ── 脏标记 ──

    /// 标记自身需要重绘，冒泡到祖先
    void setNeedsDisplay();

    /// 标记自身需要重新布局，冒泡到根
    void setNeedsLayout();

    /// 自身是否需要重绘
    bool needsDisplay() const { return needsDisplay_; }

    /// 子树中是否有需要重绘的节点
    bool subtreeNeedsDisplay() const { return subtreeNeedsDisplay_; }

    /// 自身是否需要重新布局
    bool needsLayout() const { return needsLayout_; }

    /// 清除脏标记（RenderEngine 重绘后调用）
    void clearDirtyFlags() {
        needsDisplay_ = false;
        subtreeNeedsDisplay_ = false;
        needsLayout_ = false;
    }

    // ── 命中测试与事件 ──

    /// 递归命中测试，返回最深层包含该点的可见 View。坐标为父 View 坐标系。
    virtual View* hitTest(int x, int y);

    /// 触摸事件处理。返回 true 表示已消费，false 冒泡给 parent。
    virtual bool onTouchEvent(const TouchEvent& event);

    // ── 绘制与布局 ──

    /// 绘制自身内容（子类覆写）
    virtual void onDraw(Canvas& canvas);

    /// 布局子 View（FlexLayout 实现后填充，默认为空）
    virtual void onLayout();

    /// 返回 View 的固有尺寸（用于布局计算，默认返回 {0, 0}）
    virtual Size intrinsicSize() const;

    // ── 可配置属性 ──

    void setBackgroundColor(uint8_t gray);
    uint8_t backgroundColor() const { return backgroundColor_; }

    void setHidden(bool hidden);
    bool isHidden() const { return hidden_; }

    void setOpaque(bool opaque) { opaque_ = opaque; }
    bool isOpaque() const { return opaque_; }

    void setRefreshHint(RefreshHint hint) { refreshHint_ = hint; }
    RefreshHint refreshHint() const { return refreshHint_; }

    // ── FlexBox 布局属性 ──

    FlexStyle flexStyle_;           ///< 容器布局样式
    int flexGrow_ = 0;             ///< 伸展因子，0 表示固定尺寸
    int flexBasis_ = -1;           ///< 基准尺寸，-1 表示 auto
    Align alignSelf_ = Align::Auto; ///< 交叉轴自身对齐

private:
    View* parent_ = nullptr;
    std::vector<std::unique_ptr<View>> subviews_;

    Rect frame_ = Rect::zero();

    bool needsDisplay_ = true;
    bool subtreeNeedsDisplay_ = true;
    bool needsLayout_ = true;

    uint8_t backgroundColor_ = 0xFF;  ///< 默认白色
    bool hidden_ = false;
    bool opaque_ = true;
    RefreshHint refreshHint_ = RefreshHint::Auto;

    /// 沿 parent 链向上设置 subtreeNeedsDisplay_（含短路）
    void propagateDirtyUp();
};

} // namespace ink
