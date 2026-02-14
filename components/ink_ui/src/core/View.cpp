/**
 * @file View.cpp
 * @brief View 基类实现 — 树操作、几何、脏标记、命中测试。
 */

#include "ink_ui/core/View.h"

namespace ink {

// ── 构造与析构 ──

View::View() = default;

View::~View() = default;

// ── 树操作 ──

void View::addSubview(std::unique_ptr<View> child) {
    if (!child) return;
    child->parent_ = this;
    subviews_.push_back(std::move(child));
}

std::unique_ptr<View> View::removeFromParent() {
    if (!parent_) return nullptr;

    auto& siblings = parent_->subviews_;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == this) {
            std::unique_ptr<View> self = std::move(*it);
            siblings.erase(it);
            parent_ = nullptr;
            return self;
        }
    }
    return nullptr;
}

void View::clearSubviews() {
    subviews_.clear();
}

// ── 几何属性 ──

void View::setFrame(const Rect& frame) {
    if (frame_ != frame) {
        frame_ = frame;
        setNeedsDisplay();
        setNeedsLayout();
    }
}

Rect View::screenFrame() const {
    Rect sf = frame_;
    const View* v = parent_;
    while (v) {
        sf.x += v->frame_.x;
        sf.y += v->frame_.y;
        v = v->parent_;
    }
    return sf;
}

// ── 脏标记 ──

void View::setNeedsDisplay() {
    needsDisplay_ = true;
    propagateDirtyUp();
}

void View::propagateDirtyUp() {
    View* v = parent_;
    while (v) {
        if (v->subtreeNeedsDisplay_) {
            break;  // 短路：祖先已标记
        }
        v->subtreeNeedsDisplay_ = true;
        v = v->parent_;
    }
}

void View::setNeedsLayout() {
    needsLayout_ = true;
    // 布局脏标记冒泡到根
    View* v = parent_;
    while (v) {
        v->needsLayout_ = true;
        v = v->parent_;
    }
}

// ── 属性 setter ──

void View::setHidden(bool hidden) {
    if (hidden_ != hidden) {
        hidden_ = hidden;
        setNeedsDisplay();
    }
}

void View::setBackgroundColor(uint8_t gray) {
    if (backgroundColor_ != gray) {
        backgroundColor_ = gray;
        setNeedsDisplay();
    }
}

// ── 命中测试 ──

View* View::hitTest(int x, int y) {
    if (hidden_) return nullptr;
    if (!frame_.contains(x, y)) return nullptr;

    // 转换为局部坐标
    int localX = x - frame_.x;
    int localY = y - frame_.y;

    // 逆序遍历子 View（后添加的在上层）
    for (auto it = subviews_.rbegin(); it != subviews_.rend(); ++it) {
        View* hit = (*it)->hitTest(localX, localY);
        if (hit) return hit;
    }

    return this;
}

// ── 事件处理 ──

bool View::onTouchEvent(const TouchEvent& /*event*/) {
    return false;  // 默认不消费
}

// ── 绘制与布局 ──

void View::onDraw(Canvas& /*canvas*/) {
    // 子类覆写
}

void View::onLayout() {
    flexLayout(this);
}

Size View::intrinsicSize() const {
    return {-1, -1};
}

} // namespace ink
