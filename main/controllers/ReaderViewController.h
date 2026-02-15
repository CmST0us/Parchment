/**
 * @file ReaderViewController.h
 * @brief 阅读控制器 — 加载 TXT 文件分页显示，支持翻页和进度保存。
 */

#pragma once

#include <vector>

#include "ink_ui/InkUI.h"

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

    // 分页表: pages_[i] = 第 i 页的起始字节偏移
    std::vector<uint32_t> pages_;
    int currentPage_ = 0;
    int pageFlipCount_ = 0;

    // UI 元素指针（非拥有）
    ink::TextLabel* contentLabel_ = nullptr;
    ink::TextLabel* headerLabel_ = nullptr;
    ink::TextLabel* footerLeft_ = nullptr;
    ink::TextLabel* footerRight_ = nullptr;

    // 布局参数
    int contentAreaTop_ = 0;
    int contentAreaHeight_ = 0;
    int contentAreaWidth_ = 0;
    int lineHeight_ = 0;

    /// 加载文件到内存
    bool loadFile();

    /// 计算分页表
    void paginate();

    /// 渲染指定页面
    void renderPage();

    /// 翻到下一页
    void nextPage();

    /// 翻到上一页
    void prevPage();

    /// 获取书名（不含 .txt 后缀）
    std::string bookDisplayName() const;

    /// 计算 UTF-8 字符字节长度
    static int utf8CharLen(uint8_t byte);
};
