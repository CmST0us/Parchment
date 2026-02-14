/**
 * @file Geometry.cpp
 * @brief Rect 方法实现。
 */

#include "ink_ui/core/Geometry.h"

namespace ink {

bool Rect::contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
}

bool Rect::intersects(const Rect& other) const {
    return x < other.x + other.w && x + w > other.x &&
           y < other.y + other.h && y + h > other.y;
}

Rect Rect::intersection(const Rect& other) const {
    int nx = std::max(x, other.x);
    int ny = std::max(y, other.y);
    int nr = std::min(x + w, other.x + other.w);
    int nb = std::min(y + h, other.y + other.h);

    if (nr <= nx || nb <= ny) {
        return Rect::zero();
    }
    return {nx, ny, nr - nx, nb - ny};
}

Rect Rect::unionWith(const Rect& other) const {
    if (isEmpty()) return other;
    if (other.isEmpty()) return *this;

    int nx = std::min(x, other.x);
    int ny = std::min(y, other.y);
    int nr = std::max(x + w, other.x + other.w);
    int nb = std::max(y + h, other.y + other.h);

    return {nx, ny, nr - nx, nb - ny};
}

Rect Rect::inset(const Insets& i) const {
    return {x + i.left, y + i.top,
            w - i.left - i.right, h - i.top - i.bottom};
}

Rect Rect::inset(int amount) const {
    return {x + amount, y + amount, w - 2 * amount, h - 2 * amount};
}

} // namespace ink
