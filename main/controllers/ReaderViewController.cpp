/**
 * @file ReaderViewController.cpp
 * @brief 阅读控制器实现 — 文件加载、翻页交互、进度保存。
 *        文本分页和渲染由 ReaderContentView 负责。
 */

#include "controllers/ReaderViewController.h"
#include "views/ReaderContentView.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "ui_font.h"
#include "ui_icon.h"
#include "text_encoding.h"
}

static const char* TAG = "ReaderVC";

// ════════════════════════════════════════════════════════════════
//  触摸三分区翻页 View
// ════════════════════════════════════════════════════════════════

/// 阅读内容区域 View，处理 tap 的三分区翻页
class ReaderTouchView : public ink::View {
public:
    std::function<void()> onTapLeft_;
    std::function<void()> onTapRight_;

    bool onTouchEvent(const ink::TouchEvent& event) override {
        if (event.type == ink::TouchType::Tap) {
            int x = event.x - screenFrame().x;
            int w = frame().w;
            if (x < w / 3) {
                if (onTapLeft_) onTapLeft_();
            } else if (x > w * 2 / 3) {
                if (onTapRight_) onTapRight_();
            }
            // 中间 1/3 暂不处理（未来工具栏）
            return true;
        }
        return false;
    }
};

// ════════════════════════════════════════════════════════════════
//  构造与析构
// ════════════════════════════════════════════════════════════════

ReaderViewController::ReaderViewController(ink::Application& app,
                                           const book_info_t& book)
    : app_(app), book_(book) {
    title_ = bookDisplayName();
    memset(&prefs_, 0, sizeof(prefs_));
}

ReaderViewController::~ReaderViewController() {
    if (textBuffer_) {
        heap_caps_free(textBuffer_);
        textBuffer_ = nullptr;
    }
}

// ════════════════════════════════════════════════════════════════
//  生命周期
// ════════════════════════════════════════════════════════════════

void ReaderViewController::viewDidLoad() {
    ESP_LOGI(TAG, "viewDidLoad — %s", book_.name);

    // 加载阅读偏好
    settings_store_load_prefs(&prefs_);

    const EpdFont* fontSmall = ui_font_get(20);
    const EpdFont* fontReading = ui_font_get(prefs_.font_size);
    if (!fontReading) fontReading = fontSmall;

    int margin = prefs_.margin;

    // 根 View
    view_ = std::make_unique<ink::View>();
    view_->setBackgroundColor(ink::Color::White);
    view_->flexStyle_.direction = ink::FlexDirection::Column;
    view_->flexStyle_.alignItems = ink::Align::Stretch;

    // 顶部 HeaderView（返回按钮 + 书名）
    auto header = std::make_unique<ink::HeaderView>();
    header->setFont(fontSmall);
    header->setTitle(bookDisplayName());
    header->setLeftIcon(UI_ICON_ARROW_LEFT.data, [this]() {
        ESP_LOGI(TAG, "Back button tapped");
        app_.navigator().pop();
    });
    header->flexBasis_ = 48;
    view_->addSubview(std::move(header));

    // 顶部文件名标示
    auto headerLbl = std::make_unique<ink::TextLabel>();
    headerLbl->setFont(fontSmall);
    headerLbl->setText(bookDisplayName());
    headerLbl->setColor(ink::Color::Dark);
    headerLbl->flexBasis_ = 24;
    headerLabel_ = headerLbl.get();

    auto headerContainer = std::make_unique<ink::View>();
    headerContainer->flexBasis_ = 24;
    headerContainer->setBackgroundColor(ink::Color::White);
    headerContainer->flexStyle_.direction = ink::FlexDirection::Column;
    headerContainer->flexStyle_.padding = ink::Insets{2, static_cast<int>(margin), 2, static_cast<int>(margin)};
    headerContainer->flexStyle_.alignItems = ink::Align::Stretch;
    headerLbl->flexGrow_ = 1;
    headerContainer->addSubview(std::move(headerLbl));
    view_->addSubview(std::move(headerContainer));

    // 文本内容区域（触摸三分区）
    auto touchView = std::make_unique<ReaderTouchView>();
    touchView->flexGrow_ = 1;
    touchView->setBackgroundColor(ink::Color::White);
    touchView->flexStyle_.direction = ink::FlexDirection::Column;
    touchView->flexStyle_.padding = ink::Insets{4, static_cast<int>(margin), 4, static_cast<int>(margin)};
    touchView->flexStyle_.alignItems = ink::Align::Stretch;

    touchView->onTapLeft_ = [this]() { prevPage(); };
    touchView->onTapRight_ = [this]() { nextPage(); };

    // ReaderContentView（文本渲染）
    auto content = std::make_unique<ReaderContentView>();
    content->setFont(fontReading);
    content->setLineSpacing(prefs_.line_spacing);
    content->setParagraphSpacing(prefs_.paragraph_spacing);
    content->setTextColor(ink::Color::Black);
    content->onPaginationComplete = [this]() { updateFooter(); };
    content->flexGrow_ = 1;
    contentView_ = content.get();
    touchView->addSubview(std::move(content));

    view_->addSubview(std::move(touchView));

    // 底部页脚容器
    auto footer = std::make_unique<ink::View>();
    footer->flexBasis_ = 32;
    footer->setBackgroundColor(ink::Color::White);
    footer->flexStyle_.direction = ink::FlexDirection::Row;
    footer->flexStyle_.padding = ink::Insets{4, static_cast<int>(margin), 4, static_cast<int>(margin)};
    footer->flexStyle_.alignItems = ink::Align::Center;

    auto fLeft = std::make_unique<ink::TextLabel>();
    fLeft->setFont(fontSmall);
    fLeft->setColor(ink::Color::Dark);
    fLeft->setAlignment(ink::Align::Start);
    fLeft->flexGrow_ = 1;
    footerLeft_ = fLeft.get();
    footer->addSubview(std::move(fLeft));

    auto fRight = std::make_unique<ink::TextLabel>();
    fRight->setFont(fontSmall);
    fRight->setColor(ink::Color::Dark);
    fRight->setAlignment(ink::Align::End);
    fRight->flexGrow_ = 1;
    footerRight_ = fRight.get();
    footer->addSubview(std::move(fRight));

    view_->addSubview(std::move(footer));

    // 加载文件
    if (!loadFile()) {
        // 文件加载失败时，用 headerLabel 显示错误
        if (headerLabel_) {
            headerLabel_->setText("Failed to load file");
            headerLabel_->setColor(ink::Color::Medium);
        }
        return;
    }

    // 设置文本到 ReaderContentView（懒分页将在首次 onDraw 时执行）
    contentView_->setTextBuffer(textBuffer_, textSize_);

    // 恢复阅读进度
    reading_progress_t progress = {};
    settings_store_load_progress(book_.path, &progress);
    if (progress.byte_offset > 0) {
        contentView_->setInitialByteOffset(progress.byte_offset);
    }

    // 设置页脚左侧书名
    if (footerLeft_) {
        footerLeft_->setText(bookDisplayName());
    }
}

void ReaderViewController::viewWillDisappear() {
    ESP_LOGI(TAG, "viewWillDisappear — saving progress");

    if (!contentView_ || contentView_->totalPages() == 0) return;

    reading_progress_t progress = {};
    progress.byte_offset = contentView_->currentPageOffset();
    progress.total_bytes = textSize_;
    progress.current_page = static_cast<uint32_t>(contentView_->currentPage());
    progress.total_pages = static_cast<uint32_t>(contentView_->totalPages());

    settings_store_save_progress(book_.path, &progress);
}

void ReaderViewController::handleEvent(const ink::Event& event) {
    if (event.type == ink::EventType::Swipe) {
        if (event.swipe.direction == ink::SwipeDirection::Left) {
            nextPage();
        } else if (event.swipe.direction == ink::SwipeDirection::Right) {
            prevPage();
        }
    }
}

// ════════════════════════════════════════════════════════════════
//  文件加载
// ════════════════════════════════════════════════════════════════

bool ReaderViewController::loadFile() {
    FILE* f = fopen(book_.path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", book_.path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 8 * 1024 * 1024) {
        ESP_LOGE(TAG, "File size invalid or too large: %ld", fsize);
        fclose(f);
        return false;
    }

    textSize_ = static_cast<uint32_t>(fsize);
    textBuffer_ = static_cast<char*>(
        heap_caps_malloc(textSize_ + 1, MALLOC_CAP_SPIRAM));

    if (!textBuffer_) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes in PSRAM",
                 (unsigned long)textSize_);
        fclose(f);
        return false;
    }

    size_t read = fread(textBuffer_, 1, textSize_, f);
    fclose(f);

    if (read != textSize_) {
        ESP_LOGE(TAG, "Read mismatch: %zu / %lu", read, (unsigned long)textSize_);
        heap_caps_free(textBuffer_);
        textBuffer_ = nullptr;
        return false;
    }

    // 编码检测与转码
    text_encoding_t enc = text_encoding_detect(textBuffer_, textSize_);
    ESP_LOGI(TAG, "Detected encoding: %d", (int)enc);

    if (enc == TEXT_ENCODING_GBK) {
        size_t dst_cap = (size_t)textSize_ * 3 / 2 + 1;
        char* utf8Buf = static_cast<char*>(
            heap_caps_malloc(dst_cap + 1, MALLOC_CAP_SPIRAM));
        if (!utf8Buf) {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes for GBK→UTF-8", dst_cap);
            heap_caps_free(textBuffer_);
            textBuffer_ = nullptr;
            return false;
        }

        size_t out_len = dst_cap;
        esp_err_t err = text_encoding_gbk_to_utf8(textBuffer_, textSize_,
                                                    utf8Buf, &out_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "GBK→UTF-8 conversion failed");
            heap_caps_free(utf8Buf);
            heap_caps_free(textBuffer_);
            textBuffer_ = nullptr;
            return false;
        }

        heap_caps_free(textBuffer_);
        textBuffer_ = utf8Buf;
        textSize_ = static_cast<uint32_t>(out_len);
        ESP_LOGI(TAG, "Converted GBK→UTF-8: %lu bytes", (unsigned long)textSize_);
    } else if (enc == TEXT_ENCODING_UTF8_BOM) {
        memmove(textBuffer_, textBuffer_ + 3, textSize_ - 3);
        textSize_ -= 3;
        ESP_LOGI(TAG, "Stripped UTF-8 BOM, size now %lu", (unsigned long)textSize_);
    }

    textBuffer_[textSize_] = '\0';

    ESP_LOGI(TAG, "Loaded %lu bytes from %s",
             (unsigned long)textSize_, book_.path);
    return true;
}

// ════════════════════════════════════════════════════════════════
//  翻页
// ════════════════════════════════════════════════════════════════

void ReaderViewController::nextPage() {
    if (!contentView_ || contentView_->currentPage() + 1 >= contentView_->totalPages()) return;

    contentView_->setCurrentPage(contentView_->currentPage() + 1);
    pageFlipCount_++;

    // 残影管理
    int refreshInterval = prefs_.full_refresh_pages;
    if (refreshInterval <= 0) refreshInterval = SETTINGS_DEFAULT_FULL_REFRESH;

    if (pageFlipCount_ >= refreshInterval) {
        pageFlipCount_ = 0;
        if (view_) view_->setRefreshHint(ink::RefreshHint::Full);
    } else {
        if (view_) view_->setRefreshHint(ink::RefreshHint::Quality);
    }

    updateFooter();
}

void ReaderViewController::prevPage() {
    if (!contentView_ || contentView_->currentPage() <= 0) return;

    contentView_->setCurrentPage(contentView_->currentPage() - 1);
    pageFlipCount_++;

    int refreshInterval = prefs_.full_refresh_pages;
    if (refreshInterval <= 0) refreshInterval = SETTINGS_DEFAULT_FULL_REFRESH;

    if (pageFlipCount_ >= refreshInterval) {
        pageFlipCount_ = 0;
        if (view_) view_->setRefreshHint(ink::RefreshHint::Full);
    } else {
        if (view_) view_->setRefreshHint(ink::RefreshHint::Quality);
    }

    updateFooter();
}

void ReaderViewController::updateFooter() {
    if (!contentView_ || !footerRight_) return;

    int total = contentView_->totalPages();
    int current = contentView_->currentPage();
    int percent = (total > 0) ? ((current + 1) * 100 / total) : 0;

    char buf[64];
    snprintf(buf, sizeof(buf), "%d/%d  %d%%", current + 1, total, percent);
    footerRight_->setText(buf);
}

// ════════════════════════════════════════════════════════════════
//  工具方法
// ════════════════════════════════════════════════════════════════

std::string ReaderViewController::bookDisplayName() const {
    std::string name(book_.name);
    auto pos = name.rfind(".txt");
    if (pos != std::string::npos && pos == name.size() - 4) {
        name = name.substr(0, pos);
    }
    return name;
}
