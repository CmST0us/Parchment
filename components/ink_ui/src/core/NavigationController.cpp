/**
 * @file NavigationController.cpp
 * @brief 页面栈导航控制器实现。
 */

#include "ink_ui/core/NavigationController.h"

extern "C" {
#include "esp_log.h"
}

static const char* TAG = "ink::NavCtrl";

namespace ink {

void NavigationController::setOnViewControllerChange(
    std::function<void(ViewController*)> callback) {
    onVCChange_ = std::move(callback);
}

void NavigationController::push(std::unique_ptr<ViewController> vc) {
    if (!vc) return;

    if (static_cast<int>(stack_.size()) >= MAX_NAV_DEPTH) {
        ESP_LOGE(TAG, "Stack full (depth=%d), push rejected", MAX_NAV_DEPTH);
        return;
    }

    // 旧栈顶生命周期
    if (!stack_.empty()) {
        auto* old = stack_.back().get();
        old->viewWillDisappear();
        old->viewDidDisappear();
    }

    // 推入新 VC
    ViewController* newVc = vc.get();
    stack_.push_back(std::move(vc));

    // 触发 viewDidLoad（如首次）+ 回调 + 生命周期
    newVc->getView();
    if (onVCChange_) onVCChange_(newVc);
    newVc->viewWillAppear();
    newVc->viewDidAppear();

    ESP_LOGI(TAG, "Push: depth=%d, title='%s'",
             static_cast<int>(stack_.size()), newVc->title_.c_str());
}

void NavigationController::pop() {
    if (stack_.size() <= 1) {
        ESP_LOGW(TAG, "Cannot pop: stack has %d item(s)",
                 static_cast<int>(stack_.size()));
        return;
    }

    // 栈顶生命周期
    auto* top = stack_.back().get();
    top->viewWillDisappear();
    top->viewDidDisappear();

    // 确定新栈顶，并在销毁旧 VC 之前触发回调（归还旧 VC 的 View 所有权）
    auto* newTop = stack_[stack_.size() - 2].get();
    if (onVCChange_) onVCChange_(newTop);

    // 销毁旧栈顶（此时其 view_ 已归还，正常释放）
    stack_.pop_back();

    // 新栈顶生命周期
    newTop->viewWillAppear();
    newTop->viewDidAppear();

    ESP_LOGI(TAG, "Pop: depth=%d, now='%s'",
             static_cast<int>(stack_.size()), newTop->title_.c_str());
}

void NavigationController::replace(std::unique_ptr<ViewController> vc) {
    if (!vc) return;

    if (stack_.empty()) {
        // 空栈等同于 push
        push(std::move(vc));
        return;
    }

    // 旧栈顶生命周期
    auto* old = stack_.back().get();
    old->viewWillDisappear();
    old->viewDidDisappear();

    // 加载新 VC 的 View，在销毁旧 VC 之前触发回调
    ViewController* newVc = vc.get();
    newVc->getView();
    if (onVCChange_) onVCChange_(newVc);

    // 替换（销毁旧 VC，此时其 view_ 已归还）
    stack_.back() = std::move(vc);

    // 新 VC 生命周期
    newVc->viewWillAppear();
    newVc->viewDidAppear();

    ESP_LOGI(TAG, "Replace: depth=%d, title='%s'",
             static_cast<int>(stack_.size()), newVc->title_.c_str());
}

ViewController* NavigationController::current() const {
    if (stack_.empty()) return nullptr;
    return stack_.back().get();
}

int NavigationController::depth() const {
    return static_cast<int>(stack_.size());
}

} // namespace ink
