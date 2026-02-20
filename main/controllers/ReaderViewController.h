/**
 * @file ReaderViewController.h
 * @brief 阅读控制器 — 加载 TXT 文件，通过 ReaderContentView 分页显示，支持翻页和进度保存。
 */

#pragma once

#include "ink_ui/InkUI.h"

class ReaderContentView;

extern "C" {
#include "book_store.h"
#include "settings_store.h"
}

/// 阅读页面控制器
class ReaderViewController : public ink::ViewController {
public:
    ReaderViewController(ink::Application& app, const book_info_t& book);
    ~ReaderViewController() override;

    void viewDidLoad() override;
    void viewWillDisappear() override;
    void handleEvent(const ink::Event& event) override;

private:
    ink::Application& app_;
    book_info_t book_;
    reading_prefs_t prefs_;

    // 文本数据
    char* textBuffer_ = nullptr;
    uint32_t textSize_ = 0;

    // 翻页残影管理
    int pageFlipCount_ = 0;

    // UI 元素指针（非拥有）
    ReaderContentView* contentView_ = nullptr;
    ink::HeaderView* headerView_ = nullptr;
    ink::TextLabel* footerLeft_ = nullptr;
    ink::TextLabel* footerRight_ = nullptr;

    /// 加载文件到内存
    bool loadFile();

    /// 翻到下一页
    void nextPage();

    /// 翻到上一页
    void prevPage();

    /// 更新页脚文本
    void updateFooter();

    /// 获取书名（不含 .txt 后缀）
    std::string bookDisplayName() const;
};
