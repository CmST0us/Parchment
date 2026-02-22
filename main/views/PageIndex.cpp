/**
 * @file PageIndex.cpp
 * @brief PageIndex 实现 — 页边界索引 + SD 卡持久化。
 */

#include "views/PageIndex.h"

#include <cstring>
#include <cstdio>
#include <algorithm>

extern "C" {
#include "esp_log.h"
}

static const char* TAG = "PageIndex";

PageIndex::PageIndex() = default;

// ════════════════════════════════════════════════════════════════
//  增量构建
// ════════════════════════════════════════════════════════════════

void PageIndex::addPage(uint32_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    offsets_.push_back(offset);
}

void PageIndex::markComplete() {
    std::lock_guard<std::mutex> lock(mutex_);
    complete_ = true;
    ESP_LOGI(TAG, "Index complete: %u pages", (unsigned)offsets_.size());
}

void PageIndex::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    offsets_.clear();
    complete_ = false;
}

// ════════════════════════════════════════════════════════════════
//  查询
// ════════════════════════════════════════════════════════════════

uint32_t PageIndex::pageCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(offsets_.size());
}

uint32_t PageIndex::pageOffset(uint32_t page) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (page >= offsets_.size()) return 0;
    return offsets_[page];
}

uint32_t PageIndex::findPage(uint32_t offset) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (offsets_.empty()) return 0;

    // 二分搜索：找到最后一个 offsets_[i] <= offset 的 i
    auto it = std::upper_bound(offsets_.begin(), offsets_.end(), offset);
    if (it == offsets_.begin()) return 0;
    --it;
    return static_cast<uint32_t>(it - offsets_.begin());
}

bool PageIndex::isComplete() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return complete_;
}

// ════════════════════════════════════════════════════════════════
//  SD 卡持久化
// ════════════════════════════════════════════════════════════════

bool PageIndex::load(const char* path, uint32_t textSize, uint32_t paramsHash) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    FileHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return false;
    }

    // 校验 magic
    if (memcmp(header.magic, "PIDX", 4) != 0) {
        ESP_LOGW(TAG, "Invalid magic in %s", path);
        fclose(f);
        return false;
    }

    // 校验版本
    if (header.version != CURRENT_VERSION) {
        ESP_LOGW(TAG, "Version mismatch: %u vs %u", header.version, CURRENT_VERSION);
        fclose(f);
        return false;
    }

    // 校验文件大小
    if (header.fileSize != textSize) {
        ESP_LOGW(TAG, "File size mismatch: %lu vs %lu",
                 (unsigned long)header.fileSize, (unsigned long)textSize);
        fclose(f);
        return false;
    }

    // 校验参数哈希
    if (header.paramsHash != paramsHash) {
        ESP_LOGW(TAG, "Params hash mismatch: 0x%08lx vs 0x%08lx",
                 (unsigned long)header.paramsHash, (unsigned long)paramsHash);
        fclose(f);
        return false;
    }

    if (header.pageCount == 0) {
        fclose(f);
        return false;
    }

    // 读取页偏移数组
    std::lock_guard<std::mutex> lock(mutex_);
    offsets_.resize(header.pageCount);
    size_t readCount = fread(offsets_.data(), sizeof(uint32_t),
                              header.pageCount, f);
    fclose(f);

    if (readCount != header.pageCount) {
        ESP_LOGE(TAG, "Incomplete read: %zu / %u", readCount, header.pageCount);
        offsets_.clear();
        return false;
    }

    complete_ = true;
    ESP_LOGI(TAG, "Loaded %u pages from %s", header.pageCount, path);
    return true;
}

bool PageIndex::save(const char* path, uint32_t textSize,
                     uint32_t paramsHash) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!complete_) {
        ESP_LOGW(TAG, "Cannot save incomplete index");
        return false;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create %s", path);
        return false;
    }

    FileHeader header;
    memcpy(header.magic, "PIDX", 4);
    header.version = CURRENT_VERSION;
    header.fileSize = textSize;
    header.paramsHash = paramsHash;
    header.pageCount = static_cast<uint32_t>(offsets_.size());

    bool ok = fwrite(&header, sizeof(header), 1, f) == 1;
    if (ok) {
        ok = fwrite(offsets_.data(), sizeof(uint32_t),
                     offsets_.size(), f) == offsets_.size();
    }

    fclose(f);

    if (ok) {
        ESP_LOGI(TAG, "Saved %u pages to %s",
                 (unsigned)offsets_.size(), path);
    } else {
        ESP_LOGE(TAG, "Write error saving %s", path);
        remove(path);
    }

    return ok;
}

// ════════════════════════════════════════════════════════════════
//  工具
// ════════════════════════════════════════════════════════════════

uint32_t PageIndex::computeParamsHash(uint8_t fontSize, uint8_t lineSpacing,
                                       uint8_t paragraphSpacing, uint8_t margin,
                                       int viewportW, int viewportH) {
    // FNV-1a 32-bit hash
    uint32_t hash = 0x811C9DC5;
    auto mix = [&hash](uint8_t byte) {
        hash ^= byte;
        hash *= 0x01000193;
    };

    mix(fontSize);
    mix(lineSpacing);
    mix(paragraphSpacing);
    mix(margin);

    // 混入 viewport 尺寸
    mix(static_cast<uint8_t>(viewportW & 0xFF));
    mix(static_cast<uint8_t>((viewportW >> 8) & 0xFF));
    mix(static_cast<uint8_t>(viewportH & 0xFF));
    mix(static_cast<uint8_t>((viewportH >> 8) & 0xFF));

    return hash;
}
