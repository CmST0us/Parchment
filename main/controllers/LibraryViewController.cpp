/**
 * @file LibraryViewController.cpp
 * @brief 书库控制器实现。
 */

#include "controllers/LibraryViewController.h"
#include "controllers/ReaderViewController.h"

extern "C" {
#include "esp_log.h"
#include "book_store.h"
#include "settings_store.h"
#include "ui_font.h"
#include "ui_icon.h"
}

static const char* TAG = "LibraryVC";

/// 支持 tap 回调的简单 View
class TappableView : public ink::View {
public:
    std::function<void()> onTap_;

    bool onTouchEvent(const ink::TouchEvent& event) override {
        if (event.type == ink::TouchType::Tap && onTap_) {
            onTap_();
            return true;
        }
        return false;
    }
};

LibraryViewController::LibraryViewController(ink::Application& app)
    : app_(app) {
    title_ = "Parchment";
}

void LibraryViewController::viewDidLoad() {
    ESP_LOGI(TAG, "viewDidLoad");

    const EpdFont* fontLarge = ui_font_get(28);
    const EpdFont* fontSmall = ui_font_get(20);

    // 根 View: 全屏 FlexBox Column
    view_ = std::make_unique<ink::View>();
    view_->setFrame({0, 0, ink::kScreenWidth, ink::kScreenHeight});
    view_->setBackgroundColor(ink::Color::White);
    view_->flexStyle_.direction = ink::FlexDirection::Column;
    view_->flexStyle_.alignItems = ink::Align::Stretch;

    // HeaderView: 标题栏
    auto header = std::make_unique<ink::HeaderView>();
    header->setFont(fontLarge);
    header->setTitle("Parchment");
    header->setRightIcon(UI_ICON_SETTINGS.data, []() {
        ESP_LOGI("LibraryVC", "Settings tapped (not implemented)");
    });
    header->flexBasis_ = 48;
    view_->addSubview(std::move(header));

    // 副标题: "N 本书"
    auto subtitle = std::make_unique<ink::TextLabel>();
    subtitle->setFont(fontSmall);
    subtitle->setColor(ink::Color::Dark);
    subtitle->flexBasis_ = 32;
    subtitleLabel_ = subtitle.get();

    // 包装副标题带左 padding
    auto subtitleContainer = std::make_unique<ink::View>();
    subtitleContainer->flexBasis_ = 32;
    subtitleContainer->setBackgroundColor(ink::Color::White);
    subtitleContainer->flexStyle_.direction = ink::FlexDirection::Column;
    subtitleContainer->flexStyle_.padding = ink::Insets{4, 16, 4, 16};
    subtitleContainer->flexStyle_.alignItems = ink::Align::Stretch;
    subtitle->flexGrow_ = 1;
    subtitleContainer->addSubview(std::move(subtitle));
    view_->addSubview(std::move(subtitleContainer));

    // 分隔线
    auto sep = std::make_unique<ink::SeparatorView>();
    sep->flexBasis_ = 1;
    view_->addSubview(std::move(sep));

    // 列表容器（flexGrow 填满剩余空间）
    auto listCont = std::make_unique<ink::View>();
    listCont->flexGrow_ = 1;
    listCont->setBackgroundColor(ink::Color::White);
    listCont->flexStyle_.direction = ink::FlexDirection::Column;
    listCont->flexStyle_.alignItems = ink::Align::Stretch;
    listCont->flexStyle_.padding = ink::Insets{0, 16, 0, 16};
    listContainer_ = listCont.get();
    view_->addSubview(std::move(listCont));

    // 空列表提示（初始隐藏）
    // 会在 buildPage 中按需显示/隐藏

    // 底部分隔线
    auto sepBottom = std::make_unique<ink::SeparatorView>();
    sepBottom->flexBasis_ = 1;
    view_->addSubview(std::move(sepBottom));

    // PageIndicatorView
    auto pageInd = std::make_unique<ink::PageIndicatorView>();
    pageInd->setFont(fontSmall);
    pageInd->setPage(0, 1);
    pageInd->setOnPageChange([this](int page) {
        goToPage(page);
    });
    pageInd->flexBasis_ = 48;
    pageIndicator_ = pageInd.get();
    view_->addSubview(std::move(pageInd));

    // 初始数据加载
    updatePageInfo();
    buildPage();
}

void LibraryViewController::viewWillAppear() {
    ESP_LOGI(TAG, "viewWillAppear — refreshing data");

    // 重新扫描和加载
    book_store_scan();
    currentPage_ = 0;
    updatePageInfo();
    buildPage();
}

void LibraryViewController::handleEvent(const ink::Event& event) {
    if (event.type == ink::EventType::Swipe) {
        if (event.swipe.direction == ink::SwipeDirection::Left) {
            goToPage(currentPage_ + 1);
        } else if (event.swipe.direction == ink::SwipeDirection::Right) {
            goToPage(currentPage_ - 1);
        }
    }
}

void LibraryViewController::buildPage() {
    if (!listContainer_) return;

    const EpdFont* fontSmall = ui_font_get(20);
    size_t bookCount = book_store_get_count();

    listContainer_->clearSubviews();

    // 空列表状态
    if (bookCount == 0) {
        auto emptyMsg = std::make_unique<ink::TextLabel>();
        emptyMsg->setFont(fontSmall);
        emptyMsg->setText("No books found");
        emptyMsg->setColor(ink::Color::Medium);
        emptyMsg->setAlignment(ink::Align::Center);
        emptyMsg->flexGrow_ = 1;
        listContainer_->addSubview(std::move(emptyMsg));

        if (pageIndicator_) {
            pageIndicator_->setHidden(true);
        }
        return;
    }

    if (pageIndicator_) {
        pageIndicator_->setHidden(false);
    }

    // 计算当前页的书籍范围
    int startIdx = currentPage_ * kItemsPerPage;
    int endIdx = startIdx + kItemsPerPage;
    if (endIdx > static_cast<int>(bookCount)) {
        endIdx = static_cast<int>(bookCount);
    }

    for (int i = startIdx; i < endIdx; i++) {
        const book_info_t* book = book_store_get_book(i);
        if (!book) continue;

        // 书籍条目容器（支持点击）
        auto item = std::make_unique<TappableView>();
        item->flexBasis_ = kItemHeight;
        item->setBackgroundColor(ink::Color::White);
        item->flexStyle_.direction = ink::FlexDirection::Column;
        item->flexStyle_.padding = ink::Insets{8, 0, 8, 0};
        item->flexStyle_.gap = 4;
        item->flexStyle_.alignItems = ink::Align::Stretch;

        // 书名（去掉 .txt 后缀）
        std::string bookName(book->name);
        if (bookName.size() > 4) {
            auto pos = bookName.rfind(".txt");
            if (pos != std::string::npos && pos == bookName.size() - 4) {
                bookName = bookName.substr(0, pos);
            }
        }

        auto nameLabel = std::make_unique<ink::TextLabel>();
        nameLabel->setFont(fontSmall);
        nameLabel->setText(bookName);
        nameLabel->setColor(ink::Color::Black);
        nameLabel->flexBasis_ = 28;
        item->addSubview(std::move(nameLabel));

        // 文件大小
        char sizeStr[32];
        if (book->size_bytes >= 1024 * 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f MB",
                     book->size_bytes / (1024.0 * 1024.0));
        } else if (book->size_bytes >= 1024) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f KB",
                     book->size_bytes / 1024.0);
        } else {
            snprintf(sizeStr, sizeof(sizeStr), "%lu B",
                     (unsigned long)book->size_bytes);
        }

        auto sizeLabel = std::make_unique<ink::TextLabel>();
        sizeLabel->setFont(fontSmall);
        sizeLabel->setText(sizeStr);
        sizeLabel->setColor(ink::Color::Dark);
        sizeLabel->flexBasis_ = 20;
        item->addSubview(std::move(sizeLabel));

        // 阅读进度条
        reading_progress_t progress = {};
        settings_store_load_progress(book->path, &progress);

        auto progressBar = std::make_unique<ink::ProgressBarView>();
        int pct = 0;
        if (progress.total_bytes > 0) {
            pct = static_cast<int>(progress.byte_offset * 100 / progress.total_bytes);
        }
        progressBar->setValue(pct);
        progressBar->setTrackHeight(6);
        progressBar->flexBasis_ = 16;
        item->addSubview(std::move(progressBar));

        // 设置点击回调
        int bookIdx = i;
        item->onTap_ = [this, bookIdx]() {
            const book_info_t* bk = book_store_get_book(bookIdx);
            if (bk) {
                ESP_LOGI("LibraryVC", "Opening book: %s", bk->name);
                auto readerVC = std::make_unique<ReaderViewController>(app_, *bk);
                app_.navigator().push(std::move(readerVC));
            }
        };

        listContainer_->addSubview(std::move(item));

        // 分隔线（最后一项不加）
        if (i < endIdx - 1) {
            auto sep = std::make_unique<ink::SeparatorView>();
            sep->flexBasis_ = 1;
            listContainer_->addSubview(std::move(sep));
        }
    }

    listContainer_->setNeedsLayout();
    listContainer_->setNeedsDisplay();
}

void LibraryViewController::updatePageInfo() {
    size_t bookCount = book_store_get_count();

    // 更新副标题
    if (subtitleLabel_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu 本书", bookCount);
        subtitleLabel_->setText(buf);
    }

    // 更新分页
    if (bookCount == 0) {
        totalPages_ = 0;
    } else {
        totalPages_ = (static_cast<int>(bookCount) + kItemsPerPage - 1) / kItemsPerPage;
    }

    if (currentPage_ >= totalPages_ && totalPages_ > 0) {
        currentPage_ = totalPages_ - 1;
    }

    if (pageIndicator_) {
        pageIndicator_->setPage(currentPage_, totalPages_ > 0 ? totalPages_ : 1);
    }
}

void LibraryViewController::goToPage(int page) {
    if (page < 0 || page >= totalPages_ || page == currentPage_) return;
    currentPage_ = page;
    if (pageIndicator_) {
        pageIndicator_->setPage(currentPage_, totalPages_);
    }
    buildPage();
}
