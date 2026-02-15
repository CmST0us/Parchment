/**
 * @file LibraryViewController.h
 * @brief 书库控制器 — 扫描 SD 卡书籍文件，分页列表展示，点击打开阅读。
 */

#pragma once

#include "ink_ui/InkUI.h"

extern "C" {
#include "book_store.h"
}

/// 书库页面控制器
class LibraryViewController : public ink::ViewController {
public:
    explicit LibraryViewController(ink::Application& app);

    void viewDidLoad() override;
    void viewWillAppear() override;
    void handleEvent(const ink::Event& event) override;

private:
    ink::Application& app_;

    // UI 元素指针（非拥有）
    ink::TextLabel* subtitleLabel_ = nullptr;
    ink::View* listContainer_ = nullptr;
    ink::PageIndicatorView* pageIndicator_ = nullptr;
    ink::TextLabel* emptyLabel_ = nullptr;

    int currentPage_ = 0;
    int totalPages_ = 0;

    static constexpr int kItemsPerPage = 9;
    static constexpr int kItemHeight = 96;

    /// 重建当前页的书籍列表
    void buildPage();

    /// 更新页面指示器和副标题
    void updatePageInfo();

    /// 翻到指定页
    void goToPage(int page);
};
