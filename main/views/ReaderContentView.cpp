/**
 * @file ReaderContentView.cpp
 * @brief 阅读文本渲染 View 实现 — 统一布局引擎驱动后台分页和绘制。
 */

#include "views/ReaderContentView.h"

#include <cstring>

#include "ink_ui/core/Canvas.h"
#include "text_source/TextSource.h"

extern "C" {
#include "esp_log.h"
#include "esp_heap_caps.h"
}

static const char* TAG = "ReaderContent";

// ════════════════════════════════════════════════════════════════
//  构造与析构
// ════════════════════════════════════════════════════════════════

ReaderContentView::ReaderContentView() {
    setBackgroundColor(ink::Color::White);
}

ReaderContentView::~ReaderContentView() {
    stopPaginateTask();
    freeWidthCache();
}

// ════════════════════════════════════════════════════════════════
//  配置 API
// ════════════════════════════════════════════════════════════════

void ReaderContentView::setTextSource(ink::TextSource* source) {
    textSource_ = source;
    invalidatePages();
}

void ReaderContentView::setFont(const EpdFont* font) {
    font_ = font;
    buildWidthCache();
    invalidatePages();
}

void ReaderContentView::setLineSpacing(uint8_t spacing10x) {
    if (spacing10x < 10) spacing10x = 10;
    if (spacing10x > 25) spacing10x = 25;
    lineSpacing10x_ = spacing10x;
    invalidatePages();
}

void ReaderContentView::setParagraphSpacing(uint8_t px) {
    if (px > 24) px = 24;
    paragraphSpacing_ = px;
    invalidatePages();
}

void ReaderContentView::setTextColor(uint8_t color) {
    textColor_ = color;
    setNeedsDisplay();
}

void ReaderContentView::setCacheDir(const char* cacheDirPath) {
    strncpy(cacheDirPath_, cacheDirPath, sizeof(cacheDirPath_) - 1);
}

void ReaderContentView::setStatusCallback(std::function<void()> callback) {
    statusCallback_ = callback;
}

void ReaderContentView::invalidatePages() {
    stopPaginateTask();
    pageIndex_.clear();
    paginateStarted_ = false;
    setNeedsDisplay();
}

// ════════════════════════════════════════════════════════════════
//  分页 API
// ════════════════════════════════════════════════════════════════

int ReaderContentView::totalPages() const {
    if (!pageIndex_.isComplete()) return -1;
    return static_cast<int>(pageIndex_.pageCount());
}

int ReaderContentView::currentPage() const {
    return currentPage_;
}

void ReaderContentView::setCurrentPage(int page) {
    uint32_t count = pageIndex_.pageCount();
    if (count == 0) return;

    int maxPage = static_cast<int>(count) - 1;
    if (page < 0) page = 0;

    if (page > maxPage) {
        if (pageIndex_.isComplete()) {
            // 索引已完成，不能超出
            page = maxPage;
        } else if (textSource_ && page == maxPage + 1) {
            // 顺序翻到超出已知范围：用 layoutPage 即时扩展一页
            uint32_t lastOffset = pageIndex_.pageOffset(static_cast<uint32_t>(maxPage));
            PageLayout layout = layoutPage(lastOffset);
            if (layout.endOffset > lastOffset) {
                pageIndex_.addPage(layout.endOffset);
                // 现在 maxPage 已经增加了
            } else {
                page = maxPage;
            }
        } else {
            page = maxPage;
        }
    }

    if (page != currentPage_) {
        currentPage_ = page;
        setNeedsDisplay();
    }
}

uint32_t ReaderContentView::currentPageOffset() const {
    if (pageIndex_.pageCount() == 0) return 0;
    return pageIndex_.pageOffset(static_cast<uint32_t>(currentPage_));
}

int ReaderContentView::pageForByteOffset(uint32_t offset) const {
    if (pageIndex_.pageCount() == 0) return -1;
    return static_cast<int>(pageIndex_.findPage(offset));
}

void ReaderContentView::setInitialByteOffset(uint32_t offset) {
    initialByteOffset_ = offset;
    hasInitialByteOffset_ = true;
}

bool ReaderContentView::isPageIndexComplete() const {
    return pageIndex_.isComplete();
}

float ReaderContentView::pageIndexProgress() const {
    if (!textSource_ || pageIndex_.isComplete()) {
        return pageIndex_.isComplete() ? 1.0f : 0.0f;
    }
    uint32_t totalSize = textSource_->totalSize();
    if (totalSize == 0) {
        // 使用 availableSize 作为估算基准
        totalSize = textSource_->availableSize();
        if (totalSize == 0) return 0.0f;
    }
    uint32_t count = pageIndex_.pageCount();
    if (count == 0) return 0.0f;
    uint32_t lastOffset = pageIndex_.pageOffset(count - 1);
    return static_cast<float>(lastOffset) / static_cast<float>(totalSize);
}

// ════════════════════════════════════════════════════════════════
//  统一布局引擎
// ════════════════════════════════════════════════════════════════

int ReaderContentView::lineHeight() const {
    if (!font_) return 0;
    int lh = font_->advance_y * lineSpacing10x_ / 10;
    if (lh < font_->advance_y) lh = font_->advance_y;
    return lh;
}

void ReaderContentView::buildWidthCache() {
    freeWidthCache();
    if (!font_) return;

    glyphWidthCache_ = static_cast<uint8_t*>(
        heap_caps_malloc(65536, MALLOC_CAP_SPIRAM));
    if (!glyphWidthCache_) {
        ESP_LOGW(TAG, "Failed to allocate glyph width cache (64KB PSRAM)");
        return;
    }
    memset(glyphWidthCache_, 0, 65536);

    // 遍历 font 的所有 intervals，填入每个 glyph 的 advance_x
    const EpdUnicodeInterval* intervals = font_->intervals;
    for (uint32_t i = 0; i < font_->interval_count; i++) {
        uint32_t first = intervals[i].first;
        uint32_t last = intervals[i].last;
        uint32_t offset = intervals[i].offset;
        // 只处理 BMP 范围
        if (first > 0xFFFF) continue;
        if (last > 0xFFFF) last = 0xFFFF;
        for (uint32_t cp = first; cp <= last; cp++) {
            uint16_t ax = font_->glyph[offset + (cp - first)].advance_x;
            glyphWidthCache_[cp] = ax > 255 ? 255 : static_cast<uint8_t>(ax);
        }
    }

    ESP_LOGI(TAG, "Glyph width cache built (64KB PSRAM)");
}

void ReaderContentView::freeWidthCache() {
    if (glyphWidthCache_) {
        heap_caps_free(glyphWidthCache_);
        glyphWidthCache_ = nullptr;
    }
}

int ReaderContentView::charWidth(uint32_t codepoint) const {
    if (!font_) return 0;
    // BMP 范围：查缓存表
    if (codepoint <= 0xFFFF && glyphWidthCache_) {
        uint8_t w = glyphWidthCache_[codepoint];
        return w > 0 ? w : font_->advance_y / 2;
    }
    // 非 BMP 或无缓存：回退到二分搜索
    const EpdGlyph* glyph = epd_get_glyph(font_, codepoint);
    return glyph ? glyph->advance_x : font_->advance_y / 2;
}

int ReaderContentView::utf8CharLen(uint8_t byte) {
    if (byte < 0x80) return 1;
    if ((byte & 0xE0) == 0xC0) return 2;
    if ((byte & 0xF0) == 0xE0) return 3;
    if ((byte & 0xF8) == 0xF0) return 4;
    return 1;
}

uint32_t ReaderContentView::decodeCodepoint(const char* p, int len) {
    uint8_t b = static_cast<uint8_t>(p[0]);
    uint32_t cp = 0;
    if (b < 0x80) {
        cp = b;
    } else if ((b & 0xE0) == 0xC0) {
        cp = b & 0x1F;
        for (int i = 1; i < len; i++)
            cp = (cp << 6) | (static_cast<uint8_t>(p[i]) & 0x3F);
    } else if ((b & 0xF0) == 0xE0) {
        cp = b & 0x0F;
        for (int i = 1; i < len; i++)
            cp = (cp << 6) | (static_cast<uint8_t>(p[i]) & 0x3F);
    } else if ((b & 0xF8) == 0xF0) {
        cp = b & 0x07;
        for (int i = 1; i < len; i++)
            cp = (cp << 6) | (static_cast<uint8_t>(p[i]) & 0x3F);
    } else {
        cp = 0xFFFD;
    }
    return cp;
}

ReaderContentView::PageLayout ReaderContentView::layoutPage(
    uint32_t startOffset) {
    PageLayout result;
    result.lineCount = 0;
    result.endOffset = startOffset;

    if (!font_ || !textSource_) return result;

    // 从 TextSource 获取文本
    ink::TextSpan span = textSource_->read(startOffset);
    if (!span.data || span.length == 0) return result;

    const char* textBuf = span.data;
    uint32_t textLen = span.length;

    // 使用缓存的 viewport 尺寸（后台 task 安全）
    int maxWidth = cachedViewportW_ > 0 ? cachedViewportW_ : bounds().w;
    int maxHeight = cachedViewportH_ > 0 ? cachedViewportH_ : bounds().h;
    int lh = lineHeight();
    if (lh <= 0) return result;

    int remainingHeight = maxHeight;
    uint32_t offset = startOffset;
    // localOff 是相对于 span.data 的局部偏移
    uint32_t localOff = 0;

    while (localOff < textLen && remainingHeight >= lh) {
        if (result.lineCount >= 64) break;  // 固定数组上限

        // 处理 \r\n 或 \n
        if (textBuf[localOff] == '\n') {
            result.lines[result.lineCount++] = {offset, offset, true};
            offset++;
            localOff++;
            remainingHeight -= lh + paragraphSpacing_;
            continue;
        }
        if (textBuf[localOff] == '\r') {
            uint32_t crOffset = offset;
            offset++;
            localOff++;
            if (localOff < textLen && textBuf[localOff] == '\n') {
                offset++;
                localOff++;
            }
            result.lines[result.lineCount++] = {crOffset, crOffset, true};
            remainingHeight -= lh + paragraphSpacing_;
            continue;
        }

        // 折行：逐字符测量宽度
        uint32_t lineStartOffset = offset;
        uint32_t lineStartLocal = localOff;
        int lineWidth = 0;

        while (localOff < textLen &&
               textBuf[localOff] != '\n' &&
               textBuf[localOff] != '\r') {
            int cLen = utf8CharLen(static_cast<uint8_t>(textBuf[localOff]));
            if (localOff + cLen > textLen) break;

            uint32_t cp = decodeCodepoint(textBuf + localOff, cLen);
            int cw = charWidth(cp);

            if (lineWidth + cw > maxWidth && localOff > lineStartLocal) {
                break;
            }

            lineWidth += cw;
            offset += cLen;
            localOff += cLen;
        }

        // 安全措施：至少前进一个字符
        if (localOff == lineStartLocal && localOff < textLen) {
            int cLen = utf8CharLen(static_cast<uint8_t>(textBuf[localOff]));
            offset += cLen;
            localOff += cLen;
        }

        uint32_t lineEndOffset = offset;

        // 判断是否段落结尾
        bool isParagraphEnd = false;
        if (localOff < textLen && textBuf[localOff] == '\n') {
            isParagraphEnd = true;
            offset++;
            localOff++;
        } else if (localOff < textLen && textBuf[localOff] == '\r') {
            isParagraphEnd = true;
            offset++;
            localOff++;
            if (localOff < textLen && textBuf[localOff] == '\n') {
                offset++;
                localOff++;
            }
        }

        result.lines[result.lineCount++] = {
            lineStartOffset, lineEndOffset, isParagraphEnd};

        remainingHeight -= lh;
        if (isParagraphEnd) {
            remainingHeight -= paragraphSpacing_;
        }
    }

    result.endOffset = offset;
    return result;
}

// ════════════════════════════════════════════════════════════════
//  后台分页
// ════════════════════════════════════════════════════════════════

void ReaderContentView::ensurePagination() {
    if (paginateStarted_) return;
    if (!textSource_ || !font_) return;

    paginateStarted_ = true;

    // 缓存 viewport 尺寸供后台 task 使用
    cachedViewportW_ = bounds().w;
    cachedViewportH_ = bounds().h;

    // 尝试从缓存加载 PageIndex
    if (cacheDirPath_[0] != '\0') {
        char idxPath[280];
        getPagesIdxPath(idxPath, sizeof(idxPath));
        uint32_t textSize = textSource_->totalSize();
        if (textSize > 0) {
            uint32_t hash = paramsHash();
            if (pageIndex_.load(idxPath, textSize, hash)) {
                ESP_LOGI(TAG, "PageIndex loaded from cache: %u pages",
                         (unsigned)pageIndex_.pageCount());
                // 恢复页码
                if (hasInitialByteOffset_) {
                    currentPage_ = static_cast<int>(
                        pageIndex_.findPage(initialByteOffset_));
                    hasInitialByteOffset_ = false;
                    ESP_LOGI(TAG, "Restored to page %d (offset %lu)",
                             currentPage_, (unsigned long)initialByteOffset_);
                }
                if (statusCallback_) statusCallback_();
                return;
            }
        }
    }

    // 缓存未命中，启动后台分页 task
    startPaginateTask();
}

void ReaderContentView::startPaginateTask() {
    if (paginateTask_) return;

    paginateStopRequested_ = false;
    paginateComplete_ = false;
    BaseType_t ret = xTaskCreatePinnedToCore(
        paginateTaskFunc, "paginate", 8192, this,
        tskIDLE_PRIORITY + 2, &paginateTask_, 1);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create paginate task");
    } else {
        ESP_LOGI(TAG, "Background pagination started");
    }
}

void ReaderContentView::stopPaginateTask() {
    if (!paginateTask_) return;

    paginateStopRequested_ = true;
    // 等待 task 退出
    for (int i = 0; i < 50 && !paginateComplete_; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!paginateComplete_) {
        ESP_LOGW(TAG, "Paginate task did not exit cleanly, forcing delete");
        vTaskDelete(paginateTask_);
    }
    paginateTask_ = nullptr;
}

void ReaderContentView::paginateTaskFunc(void* param) {
    auto* self = static_cast<ReaderContentView*>(param);
    self->doPaginate();
    self->paginateComplete_ = true;
    vTaskDelete(nullptr);
}

void ReaderContentView::doPaginate() {
    if (!textSource_ || !font_) return;

    ESP_LOGI(TAG, "BG paginate: starting");

    uint32_t offset = 0;
    uint32_t lastProgressNotify = 0;

    while (!paginateStopRequested_) {
        uint32_t avail = textSource_->availableSize();
        if (offset >= avail) {
            // 等待更多数据（GBK 转换中）
            if (textSource_->state() == ink::TextSourceState::Ready) {
                break;  // 全部数据已就绪，分页完成
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        pageIndex_.addPage(offset);

        PageLayout layout = layoutPage(offset);
        if (layout.endOffset <= offset) break;

        offset = layout.endOffset;

        // 进度通知（每 10% 或至少每 100 页）
        uint32_t total = textSource_->totalSize();
        if (total == 0) total = textSource_->availableSize();
        if (total > 0) {
            uint32_t progressPct = offset * 100 / total;
            uint32_t lastPct = lastProgressNotify;
            if (progressPct >= lastPct + 10) {
                lastProgressNotify = progressPct;
                if (statusCallback_) statusCallback_();
            }
        }

        vTaskDelay(1);  // 至少 1 tick，让 IDLE task 喂看门狗
    }

    if (!paginateStopRequested_) {
        pageIndex_.markComplete();

        // 恢复页码
        if (hasInitialByteOffset_) {
            currentPage_ = static_cast<int>(
                pageIndex_.findPage(initialByteOffset_));
            hasInitialByteOffset_ = false;
            ESP_LOGI(TAG, "BG: Restored to page %d (offset %lu)",
                     currentPage_, (unsigned long)initialByteOffset_);
        }

        // 保存到缓存
        if (cacheDirPath_[0] != '\0') {
            char idxPath[280];
            getPagesIdxPath(idxPath, sizeof(idxPath));
            uint32_t textSize = textSource_->totalSize();
            uint32_t hash = paramsHash();
            pageIndex_.save(idxPath, textSize, hash);
        }

        ESP_LOGI(TAG, "BG paginate: complete, %u pages",
                 (unsigned)pageIndex_.pageCount());

        if (statusCallback_) statusCallback_();
    }
}

uint32_t ReaderContentView::paramsHash() const {
    if (!font_) return 0;
    int w = cachedViewportW_ > 0 ? cachedViewportW_ : bounds().w;
    int h = cachedViewportH_ > 0 ? cachedViewportH_ : bounds().h;
    return PageIndex::computeParamsHash(
        static_cast<uint8_t>(font_->advance_y),
        lineSpacing10x_, paragraphSpacing_, 0, w, h);
}

void ReaderContentView::getPagesIdxPath(char* buf, int bufSize) const {
    snprintf(buf, bufSize, "%s/pages.idx", cacheDirPath_);
}

// ════════════════════════════════════════════════════════════════
//  渲染
// ════════════════════════════════════════════════════════════════

void ReaderContentView::onDraw(ink::Canvas& canvas) {
    if (!font_ || !textSource_) return;

    ink::TextSourceState srcState = textSource_->state();
    if (srcState == ink::TextSourceState::Error) return;

    // 懒分页：首次 onDraw 时启动
    if (!paginateStarted_) {
        ensurePagination();
    }

    // 获取当前页 offset
    uint32_t pageOffset;
    if (pageIndex_.pageCount() > 0) {
        pageOffset = pageIndex_.pageOffset(static_cast<uint32_t>(currentPage_));
    } else {
        pageOffset = 0;
    }

    // 从 TextSource 获取文本
    ink::TextSpan span = textSource_->read(pageOffset);
    if (!span.data || span.length == 0) {
        // 文本尚不可用，显示加载提示
        const char* hint = "\xE6\xAD\xA3\xE5\x9C\xA8\xE5\x8A\xA0\xE8\xBD\xBD...";  // "正在加载..."
        int hintLen = strlen(hint);
        int w = bounds().w;
        int h = bounds().h;
        // 居中显示
        int textW = font_->advance_y / 2 * hintLen / 3;  // 粗略估算
        int x = (w - textW) / 2;
        if (x < 0) x = 0;
        int y = h / 2;
        canvas.drawTextN(font_, hint, hintLen, x, y, textColor_);
        return;
    }

    // 布局当前页（layoutPage 内部调用了 read，窗口已对齐）
    PageLayout layout = layoutPage(pageOffset);

    // 再次 read 以获取渲染用指针（同一窗口内，无磁盘 I/O）
    span = textSource_->read(pageOffset);
    if (!span.data) return;

    int lh = lineHeight();
    int y = 0;

    for (int i = 0; i < layout.lineCount; i++) {
        const LineInfo& line = layout.lines[i];
        int len = static_cast<int>(line.end - line.start);
        if (len > 0) {
            uint32_t localOff = line.start - pageOffset;
            if (localOff + len <= span.length) {
                int baselineY = y + font_->ascender;
                canvas.drawTextN(font_, span.data + localOff, len,
                                 0, baselineY, textColor_);
            }
        }
        y += lh;
        if (line.isParagraphEnd) {
            y += paragraphSpacing_;
        }
    }
}
