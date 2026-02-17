/**
 * @file text_encoding.h
 * @brief 文本文件编码检测与 GBK→UTF-8 转换。
 */

#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 文本编码类型
typedef enum {
    TEXT_ENCODING_UTF8,      ///< 合法 UTF-8（含纯 ASCII）
    TEXT_ENCODING_UTF8_BOM,  ///< 带 BOM 的 UTF-8
    TEXT_ENCODING_GBK,       ///< GBK/GB2312 编码
    TEXT_ENCODING_UNKNOWN,   ///< 无法识别的编码
} text_encoding_t;

/**
 * @brief 检测缓冲区的文本编码。
 *
 * 检测优先级: BOM → UTF-8 严格校验 → fallback GBK。
 * 空缓冲区或纯 ASCII 返回 TEXT_ENCODING_UTF8。
 *
 * @param buf  字节缓冲区
 * @param len  缓冲区长度
 * @return 检测到的编码类型
 */
text_encoding_t text_encoding_detect(const char* buf, size_t len);

/**
 * @brief 将 GBK 编码的缓冲区转换为 UTF-8。
 *
 * ASCII 字节直接输出，GBK 双字节通过映射表转为 UTF-8。
 * 无法映射的序列输出 U+FFFD (EF BF BD)。
 * 目标缓冲区不足时截断到最后一个完整 UTF-8 字符。
 *
 * @param src      GBK 源缓冲区
 * @param src_len  源缓冲区长度
 * @param dst      UTF-8 目标缓冲区（需预分配）
 * @param dst_len  输入: 目标缓冲区容量; 输出: 实际写入长度
 * @return ESP_OK 成功, ESP_FAIL 参数错误
 */
esp_err_t text_encoding_gbk_to_utf8(const char* src, size_t src_len,
                                     char* dst, size_t* dst_len);

#ifdef __cplusplus
}
#endif
