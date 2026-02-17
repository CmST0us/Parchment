/**
 * @file ReaderViewController.cpp
 * @brief 阅读控制器实现 — TXT 文本分页、翻页交互、进度保存。
 */

#include "controllers/ReaderViewController.h"

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
    lineHeight_ = fontReading->advance_y * prefs_.line_spacing / 10;
    if (lineHeight_ < fontReading->advance_y) {
        lineHeight_ = fontReading->advance_y;
    }

    // 根 View（由 contentArea_ 约束尺寸）
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

    // 内容文本 TextLabel（多行）
    auto content = std::make_unique<ink::TextLabel>();
    content->setFont(fontReading);
    content->setColor(ink::Color::Black);
    content->setMaxLines(0); // 不限制行数
    content->setAlignment(ink::Align::Start);
    content->flexGrow_ = 1;
    contentLabel_ = content.get();
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
        if (contentLabel_) {
            contentLabel_->setText("Failed to load file");
            contentLabel_->setColor(ink::Color::Medium);
            contentLabel_->setAlignment(ink::Align::Center);
        }
        return;
    }

    // 计算布局参数
    // VC 可用高度 = 屏幕高度 - 状态栏(20px)
    int vcHeight = ink::kScreenHeight - 20;
    contentAreaWidth_ = ink::kScreenWidth - 2 * margin;
    // 可用内容高度：VC高度 - header(48) - headerLabel(24) - footer(32) - padding
    contentAreaHeight_ = vcHeight - 48 - 24 - 32 - 16;

    // 分页
    paginate();

    // 恢复阅读进度
    reading_progress_t progress = {};
    settings_store_load_progress(book_.path, &progress);
    if (progress.current_page > 0 && progress.current_page < static_cast<uint32_t>(pages_.size())) {
        currentPage_ = static_cast<int>(progress.current_page);
    }

    // 渲染首页
    renderPage();
}

void ReaderViewController::viewWillDisappear() {
    ESP_LOGI(TAG, "viewWillDisappear — saving progress");

    if (pages_.empty()) return;

    reading_progress_t progress = {};
    progress.byte_offset = pages_[currentPage_];
    progress.total_bytes = textSize_;
    progress.current_page = static_cast<uint32_t>(currentPage_);
    progress.total_pages = static_cast<uint32_t>(pages_.size());

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
        // GBK → UTF-8: 最坏情况 1.5 倍 + 1 (null terminator)
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
        // 剥离 BOM: 跳过前 3 字节
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
//  文本分页
// ════════════════════════════════════════════════════════════════

void ReaderViewController::paginate() {
    pages_.clear();
    if (!textBuffer_ || textSize_ == 0) return;

    const EpdFont* font = ui_font_get(prefs_.font_size);
    if (!font) return;

    int maxWidth = contentAreaWidth_;
    int maxHeight = contentAreaHeight_;
    int lh = lineHeight_;
    if (lh <= 0) lh = font->advance_y;

    int linesPerPage = maxHeight / lh;
    if (linesPerPage <= 0) linesPerPage = 1;

    uint32_t offset = 0;
    while (offset < textSize_) {
        pages_.push_back(offset);

        // 填充一页的行
        for (int line = 0; line < linesPerPage && offset < textSize_; line++) {
            // 处理换行符
            if (textBuffer_[offset] == '\n') {
                offset++;
                continue;
            }
            if (textBuffer_[offset] == '\r') {
                offset++;
                if (offset < textSize_ && textBuffer_[offset] == '\n') {
                    offset++;
                }
                continue;
            }

            // 一行：逐字符测量直到超出宽度
            uint32_t lineStart = offset;
            int lineWidth = 0;

            while (offset < textSize_ &&
                   textBuffer_[offset] != '\n' &&
                   textBuffer_[offset] != '\r') {
                int charLen = utf8CharLen(static_cast<uint8_t>(textBuffer_[offset]));
                if (offset + charLen > textSize_) break;

                // 测量这个字符的宽度
                char saved = textBuffer_[offset + charLen];
                textBuffer_[offset + charLen] = '\0';

                // 测量从 lineStart 到 offset+charLen 的总宽度
                // 为效率，只测量当前字符宽度
                int charWidth = 0;
                {
                    // 解码 codepoint 获取 glyph advance
                    uint32_t cp = 0;
                    uint8_t b = static_cast<uint8_t>(textBuffer_[offset]);
                    if (b < 0x80) {
                        cp = b;
                    } else if ((b & 0xE0) == 0xC0) {
                        cp = b & 0x1F;
                        for (int i = 1; i < charLen; i++)
                            cp = (cp << 6) | (static_cast<uint8_t>(textBuffer_[offset + i]) & 0x3F);
                    } else if ((b & 0xF0) == 0xE0) {
                        cp = b & 0x0F;
                        for (int i = 1; i < charLen; i++)
                            cp = (cp << 6) | (static_cast<uint8_t>(textBuffer_[offset + i]) & 0x3F);
                    } else if ((b & 0xF8) == 0xF0) {
                        cp = b & 0x07;
                        for (int i = 1; i < charLen; i++)
                            cp = (cp << 6) | (static_cast<uint8_t>(textBuffer_[offset + i]) & 0x3F);
                    }

                    const EpdGlyph* glyph = epd_get_glyph(font, cp);
                    charWidth = glyph ? glyph->advance_x : font->advance_y / 2;
                }

                textBuffer_[offset + charLen] = saved;

                if (lineWidth + charWidth > maxWidth && offset > lineStart) {
                    // 这行满了
                    break;
                }

                lineWidth += charWidth;
                offset += charLen;
            }

            // 如果一个字符都没装下（极端情况），至少前进一个字符
            if (offset == lineStart && offset < textSize_) {
                offset += utf8CharLen(static_cast<uint8_t>(textBuffer_[offset]));
            }
        }
    }

    ESP_LOGI(TAG, "Paginated: %zu pages", pages_.size());
}

// ════════════════════════════════════════════════════════════════
//  页面渲染
// ════════════════════════════════════════════════════════════════

void ReaderViewController::renderPage() {
    if (pages_.empty() || !contentLabel_) return;

    const EpdFont* font = ui_font_get(prefs_.font_size);
    if (!font) return;

    int maxWidth = contentAreaWidth_;
    int maxHeight = contentAreaHeight_;
    int lh = lineHeight_;
    if (lh <= 0) lh = font->advance_y;
    int linesPerPage = maxHeight / lh;
    if (linesPerPage <= 0) linesPerPage = 1;

    // 提取当前页文本范围
    uint32_t start = pages_[currentPage_];
    uint32_t end = (currentPage_ + 1 < static_cast<int>(pages_.size()))
                   ? pages_[currentPage_ + 1]
                   : textSize_;

    // 构建带 \n 的显示文本（将折行的文本转为换行符分隔）
    std::string displayText;
    displayText.reserve(end - start + linesPerPage);

    uint32_t pos = start;
    for (int line = 0; line < linesPerPage && pos < end; line++) {
        if (line > 0) displayText += '\n';

        // 跳过换行符
        if (pos < end && textBuffer_[pos] == '\n') {
            pos++;
            continue;
        }
        if (pos < end && textBuffer_[pos] == '\r') {
            pos++;
            if (pos < end && textBuffer_[pos] == '\n') pos++;
            continue;
        }

        // 一行文本
        int lineWidth = 0;
        uint32_t lineStart = pos;

        while (pos < end &&
               textBuffer_[pos] != '\n' &&
               textBuffer_[pos] != '\r') {
            int charLen = utf8CharLen(static_cast<uint8_t>(textBuffer_[pos]));
            if (pos + charLen > end) break;

            // 测量字符宽度
            uint32_t cp = 0;
            uint8_t b = static_cast<uint8_t>(textBuffer_[pos]);
            if (b < 0x80) {
                cp = b;
            } else if ((b & 0xE0) == 0xC0) {
                cp = b & 0x1F;
                for (int i = 1; i < charLen; i++)
                    cp = (cp << 6) | (static_cast<uint8_t>(textBuffer_[pos + i]) & 0x3F);
            } else if ((b & 0xF0) == 0xE0) {
                cp = b & 0x0F;
                for (int i = 1; i < charLen; i++)
                    cp = (cp << 6) | (static_cast<uint8_t>(textBuffer_[pos + i]) & 0x3F);
            } else if ((b & 0xF8) == 0xF0) {
                cp = b & 0x07;
                for (int i = 1; i < charLen; i++)
                    cp = (cp << 6) | (static_cast<uint8_t>(textBuffer_[pos + i]) & 0x3F);
            }

            const EpdGlyph* glyph = epd_get_glyph(font, cp);
            int charWidth = glyph ? glyph->advance_x : font->advance_y / 2;

            if (lineWidth + charWidth > maxWidth && pos > lineStart) {
                break;
            }

            lineWidth += charWidth;
            pos += charLen;
        }

        if (pos == lineStart && pos < end) {
            pos += utf8CharLen(static_cast<uint8_t>(textBuffer_[pos]));
        }

        displayText.append(textBuffer_ + lineStart, pos - lineStart);
    }

    contentLabel_->setText(displayText);

    // 更新页脚
    int totalPages = static_cast<int>(pages_.size());
    int percent = (totalPages > 0) ? ((currentPage_ + 1) * 100 / totalPages) : 0;

    if (footerLeft_) {
        footerLeft_->setText(bookDisplayName());
    }

    if (footerRight_) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d/%d  %d%%",
                 currentPage_ + 1, totalPages, percent);
        footerRight_->setText(buf);
    }
}

// ════════════════════════════════════════════════════════════════
//  翻页
// ════════════════════════════════════════════════════════════════

void ReaderViewController::nextPage() {
    if (currentPage_ + 1 >= static_cast<int>(pages_.size())) return;

    currentPage_++;
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

    renderPage();
}

void ReaderViewController::prevPage() {
    if (currentPage_ <= 0) return;

    currentPage_--;
    pageFlipCount_++;

    int refreshInterval = prefs_.full_refresh_pages;
    if (refreshInterval <= 0) refreshInterval = SETTINGS_DEFAULT_FULL_REFRESH;

    if (pageFlipCount_ >= refreshInterval) {
        pageFlipCount_ = 0;
        if (view_) view_->setRefreshHint(ink::RefreshHint::Full);
    } else {
        if (view_) view_->setRefreshHint(ink::RefreshHint::Quality);
    }

    renderPage();
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

int ReaderViewController::utf8CharLen(uint8_t byte) {
    if (byte < 0x80) return 1;
    if ((byte & 0xE0) == 0xC0) return 2;
    if ((byte & 0xF0) == 0xE0) return 3;
    if ((byte & 0xF8) == 0xF0) return 4;
    return 1; // 无效字节，跳过 1 字节
}
