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
    }
    return view_.get();
}

} // namespace ink
