/**
 * @file Geometry.h
 * @brief 基础几何类型: Point, Size, Rect, Insets。
 */

#pragma once

#include <algorithm>

namespace ink {

/// 二维点
struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return !(*this == o); }

    Point offset(int dx, int dy) const { return {x + dx, y + dy}; }
};

/// 二维尺寸
struct Size {
    int w = 0;
    int h = 0;

    bool operator==(const Size& o) const { return w == o.w && h == o.h; }
    bool operator!=(const Size& o) const { return !(*this == o); }

    bool isEmpty() const { return w <= 0 || h <= 0; }
};

/// 内边距
struct Insets {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;

    static Insets uniform(int v) { return {v, v, v, v}; }
    static Insets symmetric(int vertical, int horizontal) {
        return {vertical, horizontal, vertical, horizontal};
    }

    int horizontalTotal() const { return left + right; }
    int verticalTotal() const { return top + bottom; }
};

/// 轴对齐矩形 (x, y 为左上角)
struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    static Rect zero() { return {0, 0, 0, 0}; }

    // ── 访问器 ──
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    int area() const { return w * h; }
    bool isEmpty() const { return w <= 0 || h <= 0; }
    Point origin() const { return {x, y}; }
    Size size() const { return {w, h}; }

    bool operator==(const Rect& o) const {
        return x == o.x && y == o.y && w == o.w && h == o.h;
    }
    bool operator!=(const Rect& o) const { return !(*this == o); }

    /// 点是否在矩形内
    bool contains(int px, int py) const;

    /// 另一个矩形是否与此矩形相交
    bool intersects(const Rect& other) const;

    /// 返回两个矩形的交集 (可能为空)
    Rect intersection(const Rect& other) const;

    /// 返回包围两个矩形的最小矩形
    Rect unionWith(const Rect& other) const;

    /// 向内收缩
    Rect inset(const Insets& i) const;

    /// 向内收缩 (四边等量)
    Rect inset(int amount) const;
};

} // namespace ink
