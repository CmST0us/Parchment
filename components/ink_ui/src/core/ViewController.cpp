/**
 * @file ViewController.cpp
 * @brief 页面控制器基类实现。
 */

#include "ink_ui/core/ViewController.h"

namespace ink {

View* ViewController::getView() {
    if (!viewLoaded_) {
        viewDidLoad();
        viewLoaded_ = true;
        viewRawPtr_ = view_.get();
    }
    return viewRawPtr_;
}

std::unique_ptr<View> ViewController::takeView() {
    return std::move(view_);
}

void ViewController::returnView(std::unique_ptr<View> view) {
    view_ = std::move(view);
}

} // namespace ink
