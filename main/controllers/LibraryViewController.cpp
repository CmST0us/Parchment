/**
 * @file LibraryViewController.cpp
 * @brief 书库控制器实现。
 *
 * 像素级还原 design/index.html 的 Library 页面:
 * Header(汉堡+标题+设置) → Subheader(图标+书数+排序) →
 * 书籍列表(封面+信息) → 翻页指示器
 */

#include "controllers/LibraryViewController.h"
#include "controllers/ReaderViewController.h"
#include "views/BookCoverView.h"

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

void LibraryViewController::loadView() {
    ESP_LOGI(TAG, "loadView");

    const EpdFont* fontLarge = ui_font_get(28);
    const EpdFont* fontSmall = ui_font_get(16);

    // 根 View: FlexBox Column（由 contentArea_ 约束尺寸）
    view_ = std::make_unique<ink::View>();
    view_->setBackgroundColor(ink::Color::White);
    view_->flexStyle_.direction = ink::FlexDirection::Column;
    view_->flexStyle_.alignItems = ink::Align::Stretch;

    // ── HeaderView: 汉堡菜单 + "Parchment" + 设置图标 ──
    auto header = std::make_unique<ink::HeaderView>();
    header->setFont(fontLarge);
    header->setTitle("Parchment");
    header->setLeftIcon(UI_ICON_MENU.data, []() {
        ESP_LOGI("LibraryVC", "Menu tapped (not implemented)");
    });
    header->setRightIcon(UI_ICON_SETTINGS.data, []() {
        ESP_LOGI("LibraryVC", "Settings tapped (not implemented)");
    });
    header->flexBasis_ = 48;
    view_->addSubview(std::move(header));

    // ── Header 与 Subheader 间的分隔线 ──
    auto headerSep = std::make_unique<ink::SeparatorView>();
    headerSep->setLineColor(ink::Color::Light);
    headerSep->flexBasis_ = 1;
    view_->addSubview(std::move(headerSep));

    // ── Subheader: 书本图标 + "共 N 本" + "按名称排序" ──
    auto subheader = std::make_unique<ink::View>();
    subheader->flexBasis_ = 36;
    subheader->setBackgroundColor(0xE8);  // 浅灰背景
    subheader->flexStyle_.direction = ink::FlexDirection::Row;
    subheader->flexStyle_.alignItems = ink::Align::Center;
    subheader->flexStyle_.padding = ink::Insets{0, 16, 0, 16};
    subheader->flexStyle_.gap = 6;

    // 书本图标
    auto bookIcon = std::make_unique<ink::IconView>();
    bookIcon->setIcon(UI_ICON_BOOK.data);
    bookIcon->setTintColor(ink::Color::Dark);
    bookIcon->flexBasis_ = 32;
    subheader->addSubview(std::move(bookIcon));

    // "共 N 本" 文字
    auto countLabel = std::make_unique<ink::TextLabel>();
    countLabel->setFont(fontSmall);
    countLabel->setColor(ink::Color::Dark);
    countLabel->flexGrow_ = 1;
    subtitleLabel_ = countLabel.get();
    subheader->addSubview(std::move(countLabel));

    // "按名称排序" 文字
    auto sortLabel = std::make_unique<ink::TextLabel>();
    sortLabel->setFont(fontSmall);
    sortLabel->setText("按名称排序");
    sortLabel->setColor(ink::Color::Dark);
    sortLabel->setAlignment(ink::Align::End);
    sortLabel->flexBasis_ = 100;
    subheader->addSubview(std::move(sortLabel));

    view_->addSubview(std::move(subheader));

    // ── 分隔线 ──
    auto sep = std::make_unique<ink::SeparatorView>();
    sep->flexBasis_ = 1;
    view_->addSubview(std::move(sep));

    // ── 列表容器（flexGrow 填满剩余空间）──
    auto listCont = std::make_unique<ink::View>();
    listCont->flexGrow_ = 1;
    listCont->setBackgroundColor(ink::Color::White);
    listCont->flexStyle_.direction = ink::FlexDirection::Column;
    listCont->flexStyle_.alignItems = ink::Align::Stretch;
    listContainer_ = listCont.get();
    view_->addSubview(std::move(listCont));

    // ── 底部分隔线 ──
    auto sepBottom = std::make_unique<ink::SeparatorView>();
    sepBottom->flexBasis_ = 1;
    view_->addSubview(std::move(sepBottom));

    // ── PageIndicatorView ──
    auto pageInd = std::make_unique<ink::PageIndicatorView>();
    pageInd->setFont(fontSmall);
    pageInd->setPage(0, 1);
    pageInd->setOnPageChange([this](int page) {
        goToPage(page);
    });
    pageInd->flexBasis_ = 48;
    pageIndicator_ = pageInd.get();
    view_->addSubview(std::move(pageInd));
}

void LibraryViewController::viewDidLoad() {
    ESP_LOGI(TAG, "viewDidLoad");

    // ── 初始数据加载 ──
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

    const EpdFont* fontMedium = ui_font_get(20);
    const EpdFont* fontSmall = ui_font_get(16);
    size_t bookCount = book_store_get_count();

    listContainer_->clearSubviews();

    // ── 空列表状态 ──
    if (bookCount == 0) {
        auto emptyMsg = std::make_unique<ink::TextLabel>();
        emptyMsg->setFont(fontMedium);
        emptyMsg->setText("未找到书籍");
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

        // ── 书籍条目容器: Row 布局（支持点击）──
        auto item = std::make_unique<TappableView>();
        item->flexBasis_ = kItemHeight;
        item->setBackgroundColor(ink::Color::White);
        item->flexStyle_.direction = ink::FlexDirection::Row;
        item->flexStyle_.padding = ink::Insets{12, 16, 12, 16};
        item->flexStyle_.gap = 16;
        item->flexStyle_.alignItems = ink::Align::Center;

        // ── 左侧: 封面缩略图 (52×72px) ──
        auto cover = std::make_unique<BookCoverView>();
        cover->setFont(fontSmall);
        cover->setFormatTag("TXT");
        cover->flexBasis_ = 52;
        item->addSubview(std::move(cover));

        // ── 右侧: 信息区域 (Column) ──
        auto infoCol = std::make_unique<ink::View>();
        infoCol->flexGrow_ = 1;
        infoCol->setBackgroundColor(ink::Color::White);
        infoCol->flexStyle_.direction = ink::FlexDirection::Column;
        infoCol->flexStyle_.alignItems = ink::Align::Stretch;
        infoCol->flexStyle_.gap = 4;

        // 书名（去掉 .txt 后缀）
        std::string bookName(book->name);
        if (bookName.size() > 4) {
            auto pos = bookName.rfind(".txt");
            if (pos != std::string::npos && pos == bookName.size() - 4) {
                bookName = bookName.substr(0, pos);
            }
        }

        auto nameLabel = std::make_unique<ink::TextLabel>();
        nameLabel->setFont(fontMedium);
        nameLabel->setText(bookName);
        nameLabel->setColor(ink::Color::Black);
        nameLabel->flexBasis_ = 28;
        infoCol->addSubview(std::move(nameLabel));

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
        infoCol->addSubview(std::move(sizeLabel));

        // 进度条行: Row(ProgressBar + 百分比文字)
        auto progressRow = std::make_unique<ink::View>();
        progressRow->flexBasis_ = 20;
        progressRow->setBackgroundColor(ink::Color::White);
        progressRow->flexStyle_.direction = ink::FlexDirection::Row;
        progressRow->flexStyle_.alignItems = ink::Align::Center;
        progressRow->flexStyle_.gap = 10;

        // 阅读进度条
        reading_progress_t progress = {};
        settings_store_load_progress(book->path, &progress);
        int pct = 0;
        if (progress.total_bytes > 0) {
            pct = static_cast<int>(progress.byte_offset * 100 /
                                   progress.total_bytes);
        }

        auto progressBar = std::make_unique<ink::ProgressBarView>();
        progressBar->setValue(pct);
        progressBar->setTrackHeight(6);
        progressBar->flexGrow_ = 1;
        progressRow->addSubview(std::move(progressBar));

        // 百分比文字
        char pctStr[8];
        snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
        auto pctLabel = std::make_unique<ink::TextLabel>();
        pctLabel->setFont(fontSmall);
        pctLabel->setText(pctStr);
        pctLabel->setColor(ink::Color::Medium);
        pctLabel->flexBasis_ = 40;
        pctLabel->setAlignment(ink::Align::End);
        progressRow->addSubview(std::move(pctLabel));

        infoCol->addSubview(std::move(progressRow));
        item->addSubview(std::move(infoCol));

        // 设置点击回调
        int bookIdx = i;
        item->onTap_ = [this, bookIdx]() {
            const book_info_t* bk = book_store_get_book(bookIdx);
            if (bk) {
                ESP_LOGI("LibraryVC", "Opening book: %s", bk->name);
                auto readerVC =
                    std::make_unique<ReaderViewController>(app_, *bk);
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
        snprintf(buf, sizeof(buf), "共 %zu 本", bookCount);
        subtitleLabel_->setText(buf);
    }

    // 更新分页
    if (bookCount == 0) {
        totalPages_ = 0;
    } else {
        totalPages_ = (static_cast<int>(bookCount) + kItemsPerPage - 1) /
                      kItemsPerPage;
    }

    if (currentPage_ >= totalPages_ && totalPages_ > 0) {
        currentPage_ = totalPages_ - 1;
    }

    if (pageIndicator_) {
        pageIndicator_->setPage(currentPage_,
                                totalPages_ > 0 ? totalPages_ : 1);
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
