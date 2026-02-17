/**
 * @file text_encoding.c
 * @brief 编码检测与 GBK→UTF-8 转码实现。
 */

#include "text_encoding.h"
#include <stdint.h>
#include <string.h>

/* GBK→Unicode 映射表（由 generate_gbk_table.py 生成） */
extern const uint16_t gbk_to_unicode[126][190];

/**
 * @brief 计算 GBK 次字节的列索引。
 *
 * 0x40-0x7E → 0-62, 0x80-0xFE → 63-189 (跳过 0x7F)
 */
static inline int gbk_col(uint8_t lo) {
    return (lo < 0x80) ? (lo - 0x40) : (lo - 0x41);
}

// ════════════════════════════════════════════════════════════════
//  编码检测
// ════════════════════════════════════════════════════════════════

text_encoding_t text_encoding_detect(const char* buf, size_t len) {
    if (!buf || len == 0) {
        return TEXT_ENCODING_UTF8;
    }

    const uint8_t* p = (const uint8_t*)buf;

    /* 1. BOM 检测 */
    if (len >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
        return TEXT_ENCODING_UTF8_BOM;
    }

    /* 2. UTF-8 严格校验 */
    size_t i = 0;
    while (i < len) {
        uint8_t b = p[i];

        if (b < 0x80) {
            /* ASCII */
            i++;
            continue;
        }

        /* 确定多字节序列长度 */
        int seq_len;
        if (b >= 0xC2 && b <= 0xDF) {
            seq_len = 2;
        } else if (b >= 0xE0 && b <= 0xEF) {
            seq_len = 3;
        } else if (b >= 0xF0 && b <= 0xF4) {
            seq_len = 4;
        } else {
            /* C0-C1, F5-FF 或 80-BF 开头 → 非法 UTF-8 */
            return TEXT_ENCODING_GBK;
        }

        /* 检查是否有足够的 continuation bytes */
        if (i + seq_len > len) {
            return TEXT_ENCODING_GBK;
        }

        /* 验证 continuation bytes (0x80-0xBF) */
        for (int j = 1; j < seq_len; j++) {
            if ((p[i + j] & 0xC0) != 0x80) {
                return TEXT_ENCODING_GBK;
            }
        }

        /* 检查 overlong 编码 */
        if (seq_len == 3 && b == 0xE0 && p[i + 1] < 0xA0) {
            return TEXT_ENCODING_GBK;
        }
        if (seq_len == 4 && b == 0xF0 && p[i + 1] < 0x90) {
            return TEXT_ENCODING_GBK;
        }
        if (seq_len == 4 && b == 0xF4 && p[i + 1] > 0x8F) {
            return TEXT_ENCODING_GBK;
        }

        i += seq_len;
    }

    return TEXT_ENCODING_UTF8;
}

// ════════════════════════════════════════════════════════════════
//  GBK → UTF-8 转码
// ════════════════════════════════════════════════════════════════

/**
 * @brief 将 Unicode codepoint 编码为 UTF-8，写入 dst。
 * @return 写入的字节数 (1-3)，0 表示空间不足。
 */
static size_t encode_utf8(uint16_t cp, char* dst, size_t remaining) {
    if (cp < 0x80) {
        if (remaining < 1) return 0;
        dst[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        if (remaining < 2) return 0;
        dst[0] = (char)(0xC0 | (cp >> 6));
        dst[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else {
        if (remaining < 3) return 0;
        dst[0] = (char)(0xE0 | (cp >> 12));
        dst[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        dst[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
}

esp_err_t text_encoding_gbk_to_utf8(const char* src, size_t src_len,
                                     char* dst, size_t* dst_len) {
    if (!src || !dst || !dst_len) {
        return ESP_FAIL;
    }

    size_t capacity = *dst_len;
    size_t di = 0;  /* 目标偏移 */
    size_t si = 0;  /* 源偏移 */

    while (si < src_len) {
        uint8_t b = (uint8_t)src[si];

        if (b < 0x80) {
            /* ASCII: 直接输出 */
            if (di >= capacity) break;
            dst[di++] = (char)b;
            si++;
        } else if (b >= 0x81 && b <= 0xFE && si + 1 < src_len) {
            /* GBK 双字节 */
            uint8_t lo = (uint8_t)src[si + 1];

            if (lo >= 0x40 && lo <= 0xFE && lo != 0x7F) {
                int row = b - 0x81;
                int col = gbk_col(lo);
                uint16_t cp = gbk_to_unicode[row][col];

                if (cp != 0) {
                    size_t n = encode_utf8(cp, dst + di, capacity - di);
                    if (n == 0) break;  /* 空间不足，截断 */
                    di += n;
                } else {
                    /* 映射值为 0：无效 → U+FFFD */
                    size_t n = encode_utf8(0xFFFD, dst + di, capacity - di);
                    if (n == 0) break;
                    di += n;
                }
                si += 2;
            } else {
                /* 次字节越界 → U+FFFD */
                size_t n = encode_utf8(0xFFFD, dst + di, capacity - di);
                if (n == 0) break;
                di += n;
                si += 2;
            }
        } else {
            /* 孤立高字节 → U+FFFD */
            size_t n = encode_utf8(0xFFFD, dst + di, capacity - di);
            if (n == 0) break;
            di += n;
            si++;
        }
    }

    *dst_len = di;
    return ESP_OK;
}
