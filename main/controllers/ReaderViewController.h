/**
 * @file ReaderViewController.h
 * @brief 阅读控制器 — 通过 TextSource 流式加载文件，ReaderContentView 分页显示。
 */

#pragma once

#include "ink_ui/InkUI.h"
#include "text_source/TextSource.h"

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

    void loadView() override;
    void viewDidLoad() override;
    void viewWillDisappear() override;
    void handleEvent(const ink::Event& event) override;

private:
    ink::Application& app_;
    book_info_t book_;
    reading_prefs_t prefs_;

    // 文本数据源（owned）
    ink::TextSource textSource_;

    // 缓存目录路径
    char cacheDirPath_[256] = {};

    // 翻页残影管理
    int pageFlipCount_ = 0;

    // UI 元素指针（非拥有）
    ReaderContentView* contentView_ = nullptr;
    ink::HeaderView* headerView_ = nullptr;
    ink::SeparatorView* headerSep_ = nullptr;
    ink::View* headerOverlay_ = nullptr;  ///< Header 浮层容器（叠加在内容上方）
    ink::TextLabel* footerLeft_ = nullptr;
    ink::TextLabel* footerRight_ = nullptr;

    /// 计算缓存目录路径
    void computeCacheDirPath();

    /// 翻到下一页
    void nextPage();

    /// 翻到上一页
    void prevPage();

    /// 更新页脚文本（根据当前状态）
    void updateFooter();

    /// 获取书名（不含 .txt 后缀）
    std::string bookDisplayName() const;

    /// 隐藏 header 浮层（自动隐藏回调）
    void hideHeaderOverlay();

    static constexpr int kStatusTimerId = 100;      ///< 状态更新唤醒定时器
    static constexpr int kHeaderHideTimerBase = 200; ///< Header 自动隐藏定时器基址
    int headerTimerGen_ = 0;                         ///< Header timer generation（防 stale）
};
