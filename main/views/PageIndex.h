/**
 * @file PageIndex.h
 * @brief 页边界索引 — 存储每页起始 byte offset，支持 SD 卡持久化。
 *
 * 后台 task 通过 addPage() 增量构建，主线程通过 pageOffset()/findPage()
 * 查询。所有读写通过 mutex 保护线程安全。
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

/// 页边界索引
class PageIndex {
public:
    PageIndex();

    // ── 增量构建 ──

    /// 添加一页的起始 byte offset（后台 task 调用）
    void addPage(uint32_t offset);

    /// 标记索引构建完成
    void markComplete();

    /// 清空索引（配置变更时调用）
    void clear();

    // ── 查询 ──

    /// 当前已知的页数
    uint32_t pageCount() const;

    /// 第 page 页的起始 byte offset。page 越界时返回 0。
    uint32_t pageOffset(uint32_t page) const;

    /// 二分搜索找到包含该 byte offset 的页码
    uint32_t findPage(uint32_t offset) const;

    /// 索引是否已覆盖全文
    bool isComplete() const;

    // ── SD 卡持久化 ──

    /// 从文件加载索引（校验 magic + version + file_size + params_hash）
    /// @return true 如果缓存有效并成功加载
    bool load(const char* path, uint32_t textSize, uint32_t paramsHash);

    /// 保存索引到文件（仅 isComplete 时允许）
    bool save(const char* path, uint32_t textSize, uint32_t paramsHash) const;

    // ── 工具 ──

    /// 计算排版参数哈希值（FNV-1a 32-bit）
    static uint32_t computeParamsHash(uint8_t fontSize, uint8_t lineSpacing,
                                      uint8_t paragraphSpacing, uint8_t margin,
                                      int viewportW, int viewportH);

private:
    mutable std::mutex mutex_;
    std::vector<uint32_t> offsets_;  // PSRAM 分配
    bool complete_ = false;

    /// 持久化文件 header
    struct FileHeader {
        char magic[4];        // "PIDX"
        uint16_t version;     // 1
        uint32_t fileSize;    // 原始文本大小（用于校验）
        uint32_t paramsHash;  // 排版参数哈希
        uint32_t pageCount;   // 页数
    } __attribute__((packed));

    static constexpr uint16_t CURRENT_VERSION = 1;
};
