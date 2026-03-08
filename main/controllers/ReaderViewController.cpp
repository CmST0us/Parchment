/**
 * @file ReaderViewController.cpp
 * @brief 阅读控制器实现 — TextSource 流式加载、翻页交互、进度保存。
 *        文本分页和渲染由 ReaderContentView 负责。
 */

#include "controllers/ReaderViewController.h"
#include "views/ReaderContentView.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

extern "C" {
#include "esp_log.h"
#include "mbedtls/md5.h"
#include "ui_font.h"
#include "ui_icon.h"
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
    std::function<void()> onTapMiddle_;

    bool onTouchEvent(const ink::TouchEvent& event) override {
        if (event.type == ink::TouchType::Tap) {
            int x = event.x - screenFrame().x;
            int w = frame().w;
            ESP_LOGI("ReaderTouch", "Tap x=%d (local), w=%d, zone=%s",
                     x, w,
                     (x < w / 3) ? "left" : (x > w * 2 / 3) ? "right" : "middle");
            if (x < w / 3) {
                if (onTapLeft_) onTapLeft_();
            } else if (x > w * 2 / 3) {
                if (onTapRight_) onTapRight_();
            } else {
                if (onTapMiddle_) onTapMiddle_();
            }
            return true;
        }
        return false;
    }
};

/// 叠层根 View: 第一个子 View 铺满 bounds，后续子 View 在顶部叠加显示
class OverlayRootView : public ink::View {
public:
    void onLayout() override {
        const ink::Rect b = bounds();
        for (size_t i = 0; i < subviews().size(); i++) {
            auto& child = subviews()[i];
            if (child->isHidden()) continue;
            if (i == 0) {
                // 底层内容铺满
                child->setFrame(b);
            } else {
                // 浮层：全宽，高度由内容决定
                ink::Size sz = child->intrinsicSize();
                int h = (sz.h > 0) ? sz.h : 49;  // 48 header + 1 sep
                child->setFrame({0, 0, b.w, h});
            }
            child->onLayout();
        }
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
    textSource_.close();
}

// ════════════════════════════════════════════════════════════════
//  生命周期
// ════════════════════════════════════════════════════════════════

void ReaderViewController::loadView() {
    ESP_LOGI(TAG, "loadView — %s", book_.name);

    // 加载阅读偏好（影响 view 树构建）
    settings_store_load_prefs(&prefs_);

    const EpdFont* fontSmall = ui_font_get(20);
    const EpdFont* fontReading = ui_font_get(prefs_.font_size);
    if (!fontReading) fontReading = fontSmall;

    int margin = prefs_.margin;

    // 根 View: 叠层布局（底层内容铺满，header 浮在上方）
    view_ = std::make_unique<OverlayRootView>();
    view_->setBackgroundColor(ink::Color::White);

    // ── 底层：内容区域（Column 布局，铺满全屏）──
    auto contentColumn = std::make_unique<ink::View>();
    contentColumn->setBackgroundColor(ink::Color::White);
    contentColumn->flexStyle_.direction = ink::FlexDirection::Column;
    contentColumn->flexStyle_.alignItems = ink::Align::Stretch;

    // 文本内容区域（触摸三分区）
    auto touchView = std::make_unique<ReaderTouchView>();
    touchView->flexGrow_ = 1;
    touchView->setBackgroundColor(ink::Color::White);
    touchView->flexStyle_.direction = ink::FlexDirection::Column;
    touchView->flexStyle_.padding = ink::Insets{4, static_cast<int>(margin), 4, static_cast<int>(margin)};
    touchView->flexStyle_.alignItems = ink::Align::Stretch;

    touchView->onTapLeft_ = [this]() { prevPage(); };
    touchView->onTapRight_ = [this]() { nextPage(); };
    touchView->onTapMiddle_ = [this]() {
        if (headerOverlay_) {
            bool show = headerOverlay_->isHidden();
            headerOverlay_->setHidden(!show);
            ESP_LOGI(TAG, "Header toggled: %s", show ? "visible" : "hidden");
            // 触发根 View 重新布局和绘制
            view()->setNeedsLayout();
            view()->setNeedsDisplay();
        }
    };

    // ReaderContentView（文本渲染）
    auto content = std::make_unique<ReaderContentView>();
    content->setFont(fontReading);
    content->setLineSpacing(prefs_.line_spacing);
    content->setParagraphSpacing(prefs_.paragraph_spacing);
    content->setTextColor(ink::Color::Black);
    content->flexGrow_ = 1;
    contentView_ = content.get();
    touchView->addSubview(std::move(content));

    contentColumn->addSubview(std::move(touchView));

    // 底部页脚容器
    auto footer = std::make_unique<ink::View>();
    footer->flexBasis_ = 32;
    footer->setBackgroundColor(ink::Color::White);
    footer->flexStyle_.direction = ink::FlexDirection::Row;
    footer->flexStyle_.padding = ink::Insets{4, static_cast<int>(margin), 4, static_cast<int>(margin)};
    footer->flexStyle_.alignItems = ink::Align::Center;

    const EpdFont* fontFooter = ui_font_get(16);

    auto fLeft = std::make_unique<ink::TextLabel>();
    fLeft->setFont(fontFooter);
    fLeft->setColor(ink::Color::Dark);
    fLeft->setAlignment(ink::Align::Start);
    fLeft->flexGrow_ = 1;
    footerLeft_ = fLeft.get();
    footer->addSubview(std::move(fLeft));

    auto fRight = std::make_unique<ink::TextLabel>();
    fRight->setFont(fontFooter);
    fRight->setColor(ink::Color::Dark);
    fRight->setAlignment(ink::Align::End);
    fRight->flexGrow_ = 1;
    footerRight_ = fRight.get();
    footer->addSubview(std::move(fRight));

    contentColumn->addSubview(std::move(footer));
    view_->addSubview(std::move(contentColumn));

    // ── 浮层：Header + 分隔线（叠加在内容上方）──
    auto headerBar = std::make_unique<ink::View>();
    headerBar->setBackgroundColor(ink::Color::White);
    headerBar->flexStyle_.direction = ink::FlexDirection::Column;
    headerBar->flexStyle_.alignItems = ink::Align::Stretch;

    auto header = std::make_unique<ink::HeaderView>();
    header->setFont(fontSmall);
    header->setTitle(bookDisplayName());
    header->setLeftIcon(UI_ICON_ARROW_LEFT.data, [this]() {
        ESP_LOGI(TAG, "Back button tapped");
        app_.navigator().pop();
    });
    header->flexBasis_ = 48;
    headerView_ = header.get();
    headerBar->addSubview(std::move(header));

    auto headerSep = std::make_unique<ink::SeparatorView>();
    headerSep->flexBasis_ = 1;
    headerSep_ = headerSep.get();
    headerBar->addSubview(std::move(headerSep));

    headerOverlay_ = headerBar.get();
    view_->addSubview(std::move(headerBar));

    // Header 浮层默认隐藏
    if (headerOverlay_) {
        headerOverlay_->setHidden(true);
        ESP_LOGI(TAG, "Header overlay initially hidden");
    }
}

void ReaderViewController::viewDidLoad() {
    ESP_LOGI(TAG, "viewDidLoad — %s", book_.name);

    // 计算缓存目录路径
    computeCacheDirPath();

    // 创建缓存目录（先确保父目录存在）
    mkdir("/sdcard/.cache", 0755);
    mkdir(cacheDirPath_, 0755);

    // 打开 TextSource
    if (!textSource_.open(book_.path, cacheDirPath_)) {
        ESP_LOGE(TAG, "Failed to open TextSource: %s", book_.path);
        if (footerRight_) {
            footerRight_->setText("Open failed");
        }
        return;
    }

    // 恢复阅读进度（必须在 setTextSource 之前，因为它会触发页索引加载）
    reading_progress_t progress = {};
    settings_store_load_progress(book_.path, &progress);
    if (progress.byte_offset > 0) {
        contentView_->setInitialByteOffset(progress.byte_offset);
    }

    // 设置文本源到 ReaderContentView（触发页索引加载/构建）
    contentView_->setTextSource(&textSource_);
    contentView_->setCacheDir(cacheDirPath_);

    // 设置状态回调（从后台 task 调用，postEvent 唤醒主循环触发渲染）
    contentView_->setStatusCallback([this]() {
        updateFooter();
        app_.postEvent(ink::Event::makeTimer(kStatusTimerId));
    });

    // 设置页脚左侧书名
    if (footerLeft_) {
        footerLeft_->setText(bookDisplayName());
    }

    // 初始页脚状态
    updateFooter();
}

void ReaderViewController::viewWillDisappear() {
    ESP_LOGI(TAG, "viewWillDisappear — saving progress");

    if (!contentView_) return;

    reading_progress_t progress = {};
    progress.byte_offset = contentView_->currentPageOffset();
    progress.total_bytes = textSource_.totalSize();
    progress.current_page = static_cast<uint32_t>(contentView_->currentPage());
    progress.total_pages = contentView_->isPageIndexComplete()
                               ? static_cast<uint32_t>(contentView_->totalPages())
                               : 0;

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
//  缓存目录路径
// ════════════════════════════════════════════════════════════════

void ReaderViewController::computeCacheDirPath() {
    uint8_t hash[16];
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    mbedtls_md5_update(&ctx, reinterpret_cast<const uint8_t*>(book_.path),
                       strlen(book_.path));
    mbedtls_md5_finish(&ctx, hash);
    mbedtls_md5_free(&ctx);

    char hexStr[33];
    for (int i = 0; i < 16; i++) {
        snprintf(hexStr + i * 2, 3, "%02x", hash[i]);
    }

    snprintf(cacheDirPath_, sizeof(cacheDirPath_),
             "/sdcard/.cache/%s", hexStr);
}

// ════════════════════════════════════════════════════════════════
//  翻页
// ════════════════════════════════════════════════════════════════

void ReaderViewController::nextPage() {
    if (!contentView_) return;

    int current = contentView_->currentPage();
    int total = contentView_->totalPages();

    // 允许翻页：索引未完成时也允许前进（sequential page turning）
    if (total > 0 && current + 1 >= total) return;

    contentView_->setCurrentPage(current + 1);
    applyPageFlipRefresh();

    // 翻页后 header 浮层需要重绘（内容重绘会覆盖其区域）
    if (headerOverlay_ && !headerOverlay_->isHidden()) {
        headerOverlay_->setNeedsDisplay();
    }

    updateFooter();
}

void ReaderViewController::prevPage() {
    if (!contentView_ || contentView_->currentPage() <= 0) return;

    contentView_->setCurrentPage(contentView_->currentPage() - 1);
    applyPageFlipRefresh();

    // 翻页后 header 浮层需要重绘（内容重绘会覆盖其区域）
    if (headerOverlay_ && !headerOverlay_->isHidden()) {
        headerOverlay_->setNeedsDisplay();
    }

    updateFooter();
}

void ReaderViewController::applyPageFlipRefresh() {
    pageFlipCount_++;
    if (pageFlipCount_ >= kGhostClearInterval) {
        // 每 N 次翻页触发 W>B>GL 全屏刷新消残影
        // 通过 Application 标脏整个 window 树（含 StatusBar），避免 W>B 后残留黑边
        pageFlipCount_ = 0;
        app_.requestTransitionRefresh();
        ESP_LOGI(TAG, "Ghost clear triggered after %d page flips", kGhostClearInterval);
    } else {
        contentView_->setRefreshHint(ink::RefreshHint::Quality);
    }
}

void ReaderViewController::updateFooter() {
    if (!contentView_ || !footerRight_) return;

    char buf[64];

    // 检查 TextSource 状态
    ink::TextSourceState srcState = textSource_.state();
    if (srcState == ink::TextSourceState::Converting) {
        float prog = textSource_.progress();
        int pct = static_cast<int>(prog * 100);
        snprintf(buf, sizeof(buf), "\xE6\xAD\xA3\xE5\x9C\xA8\xE8\xBD\xAC\xE6\x8D\xA2\xE7\xBC\x96\xE7\xA0\x81... %d%%", pct);
        // "正在转换编码... XX%"
        footerRight_->setText(buf);
        return;
    }

    // 检查 PageIndex 状态
    if (!contentView_->isPageIndexComplete()) {
        float prog = contentView_->pageIndexProgress();
        int pct = static_cast<int>(prog * 100);
        snprintf(buf, sizeof(buf), "\xE6\xAD\xA3\xE5\x9C\xA8\xE7\xB4\xA2\xE5\xBC\x95... %d%%", pct);
        // "正在索引... XX%"
        footerRight_->setText(buf);
        return;
    }

    // 全部就绪：显示页码
    int total = contentView_->totalPages();
    int current = contentView_->currentPage();
    int percent = (total > 0) ? ((current + 1) * 100 / total) : 0;

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
