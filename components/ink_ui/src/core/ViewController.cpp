/**
 * @file ViewController.cpp
 * @brief 页面控制器基类实现。
 */

#include "ink_ui/core/ViewController.h"

namespace ink {

void ViewController::loadView() {
    view_ = std::make_unique<View>();
}

View* ViewController::view() {
    if (!viewLoaded_) {
        loadView();
        viewRawPtr_ = view_.get();
        viewLoaded_ = true;
        viewDidLoad();
    }
    return viewRawPtr_;
}

View* ViewController::getView() {
    return view();
}

std::unique_ptr<View> ViewController::takeView() {
    return std::move(view_);
}

void ViewController::returnView(std::unique_ptr<View> view) {
    view_ = std::move(view);
}

} // namespace ink
