/**
 * @file FlexLayout.cpp
 * @brief FlexBox 布局算法实现。
 *
 * 支持 Column/Row 方向、flexGrow 弹性分配、gap 间距、padding 内边距、
 * alignItems/alignSelf 交叉轴对齐、justifyContent 主轴对齐。
 */

#include "ink_ui/core/FlexLayout.h"
#include "ink_ui/core/View.h"

#include <algorithm>
#include <vector>

namespace ink {

void flexLayout(View* container) {
    if (!container) return;

    const FlexStyle& style = container->flexStyle_;
    const Rect bounds = container->bounds();

    // 内容区域（扣除 padding）
    const int contentX = style.padding.left;
    const int contentY = style.padding.top;
    const int contentW = bounds.w - style.padding.horizontalTotal();
    const int contentH = bounds.h - style.padding.verticalTotal();

    if (contentW <= 0 || contentH <= 0) return;

    const bool isColumn = (style.direction == FlexDirection::Column);
    const int mainSize = isColumn ? contentH : contentW;
    const int crossSize = isColumn ? contentW : contentH;

    // 收集可见子 View
    std::vector<View*> children;
    for (auto& child : container->subviews()) {
        if (!child->isHidden()) {
            children.push_back(child.get());
        }
    }

    if (children.empty()) return;

    const int N = static_cast<int>(children.size());
    const int totalGap = (N > 1) ? (N - 1) * style.gap : 0;

    // ── 测量阶段：计算固定尺寸总和和 flexGrow 总和 ──

    int fixedTotal = 0;
    int totalGrow = 0;

    // 每个子 View 的主轴基础尺寸（暂时只记录固定子 View 的）
    std::vector<int> mainSizes(N, 0);

    for (int i = 0; i < N; i++) {
        View* child = children[i];
        if (child->flexGrow_ > 0) {
            totalGrow += child->flexGrow_;
        } else {
            int basis = child->flexBasis_;
            if (basis == -1) {
                // 使用 intrinsicSize
                Size intrinsic = child->intrinsicSize();
                basis = isColumn ? intrinsic.h : intrinsic.w;
                if (basis < 0) basis = 0;
            }
            mainSizes[i] = basis;
            fixedTotal += basis;
        }
    }

    // ── 分配阶段：剩余空间按 flexGrow 比例分配 ──

    int available = mainSize - totalGap - fixedTotal;
    if (available < 0) available = 0;

    if (totalGrow > 0) {
        int remainingSpace = available;
        int remainingGrow = totalGrow;

        for (int i = 0; i < N; i++) {
            View* child = children[i];
            if (child->flexGrow_ > 0) {
                int alloc = remainingSpace * child->flexGrow_ / remainingGrow;
                mainSizes[i] = alloc;
                remainingSpace -= alloc;
                remainingGrow -= child->flexGrow_;
            }
        }
    }

    // ── justifyContent 计算起始偏移和额外间距 ──

    int totalContent = 0;
    for (int i = 0; i < N; i++) {
        totalContent += mainSizes[i];
    }
    totalContent += totalGap;

    int mainCursor = 0;
    int extraGap = 0;       // 额外间距（SpaceBetween/SpaceAround 使用）
    int halfAround = 0;     // SpaceAround 的首尾半间距

    // justifyContent 仅在没有 flex 子 View（或剩余空间为 0）时有意义
    int remaining = mainSize - totalContent;
    if (remaining < 0) remaining = 0;

    if (remaining > 0) {
        switch (style.justifyContent) {
            case Justify::Start:
                break;
            case Justify::Center:
                mainCursor = remaining / 2;
                break;
            case Justify::End:
                mainCursor = remaining;
                break;
            case Justify::SpaceBetween:
                if (N > 1) {
                    extraGap = remaining / (N - 1);
                }
                break;
            case Justify::SpaceAround:
                if (N > 0) {
                    int perItem = remaining / N;
                    halfAround = perItem / 2;
                    extraGap = perItem;
                    mainCursor = halfAround;
                }
                break;
        }
    }

    // ── 定位阶段：设置每个子 View 的 frame 并递归 onLayout ──

    int cursor = mainCursor;

    for (int i = 0; i < N; i++) {
        View* child = children[i];
        int childMain = mainSizes[i];

        // 交叉轴对齐
        Align effectiveAlign = child->alignSelf_;
        if (effectiveAlign == Align::Auto) {
            effectiveAlign = style.alignItems;
        }

        int crossPos = 0;
        int crossDim = crossSize;

        if (effectiveAlign != Align::Stretch) {
            // 获取交叉轴固有尺寸
            Size intrinsic = child->intrinsicSize();
            int crossIntrinsic = isColumn ? intrinsic.w : intrinsic.h;
            if (crossIntrinsic < 0) {
                // 无固有尺寸，退化为 stretch
                crossDim = crossSize;
            } else {
                crossDim = crossIntrinsic;
                if (crossDim > crossSize) crossDim = crossSize;
            }

            switch (effectiveAlign) {
                case Align::Start:
                    crossPos = 0;
                    break;
                case Align::Center:
                    crossPos = (crossSize - crossDim) / 2;
                    break;
                case Align::End:
                    crossPos = crossSize - crossDim;
                    break;
                default:
                    break;
            }
        }

        // 设置 frame
        if (isColumn) {
            child->setFrame({contentX + crossPos, contentY + cursor,
                             crossDim, childMain});
        } else {
            child->setFrame({contentX + cursor, contentY + crossPos,
                             childMain, crossDim});
        }

        // 递归布局子 View
        child->onLayout();

        cursor += childMain + style.gap + extraGap;
    }
}

} // namespace ink
