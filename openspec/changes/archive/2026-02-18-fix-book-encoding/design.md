## Context

当前 `ReaderViewController::loadFile()` 将 TXT 文件以 raw bytes 读入 PSRAM 缓冲区，后续分页（`paginate()`）和渲染（`renderPage()`）全链路假定 UTF-8 编码。中文互联网上大量 TXT 小说使用 GBK/GB2312 编码，直接当 UTF-8 解析会导致字节边界错位、codepoint 无效，最终显示乱码。

插入点明确：在 `loadFile()` 的 `fread()` 之后、`return true` 之前完成编码检测和转码，后续管线完全不需要改动。

## Goals / Non-Goals

**Goals:**
- 自动检测 UTF-8（含 BOM）、GBK/GB2312 编码并统一转为 UTF-8
- 转码对用户透明，无需手动选择编码
- 独立组件（`components/text_encoding/`），可被其他模块复用
- 内存开销可控：映射表常驻 flash，转码临时缓冲区用完即释放

**Non-Goals:**
- 不支持 UTF-16、Big5、Shift-JIS 等其他编码（后续可扩展）
- 不提供编码手动选择 UI（第一版自动检测足够）
- 不做流式转码（当前整文件加载模式下无需）

## Decisions

### 1. 编码检测策略：BOM 优先 + UTF-8 校验 + GBK fallback

**方案**: 三步检测法
1. **BOM 检测**: 检查文件头 3 字节是否为 UTF-8 BOM (`EF BB BF`)，是则剥离 BOM 并标记为 UTF-8
2. **UTF-8 有效性校验**: 遍历全部字节，严格校验 UTF-8 多字节序列（前导字节 + continuation bytes）。全部合法则判定 UTF-8
3. **Fallback GBK**: 上述均不满足，默认当作 GBK 处理

**为什么不用统计启发式（如 chardet）**: ESP32 内存有限，启发式需要频率表和统计模型，复杂度高收益低。中文 TXT 实际上只有 UTF-8 和 GBK 两种主流编码，三步法覆盖 99% 场景。

**为什么 fallback 是 GBK 而非报错**: 用户体验优先。GBK 是中文 TXT 最常见的非 UTF-8 编码，即使偶尔误判，GBK 解码器遇到无效字节可以用替换字符（U+FFFD）优雅降级。

### 2. GBK→UTF-8 转换：内嵌映射表

**方案**: 编译期生成 GBK→Unicode 映射表，存储为 `const` 数组（存 flash，不占 RAM）

- GBK 双字节范围：首字节 `0x81-0xFE`，次字节 `0x40-0xFE`（跳过 `0x7F`）
- 映射表结构：`uint16_t gbk_to_unicode[126][190]`，约 47KB
- 用 Python 脚本从标准 GBK 码表生成 C 数组

**为什么不用 iconv / ICU**: ESP-IDF 不自带这些库，交叉编译和移植成本高。一张静态映射表简单可靠。

**为什么不压缩映射表**: 47KB 在 16MB flash 中微不足道，压缩增加运行时 CPU 开销，得不偿失。

### 3. 转码时机：loadFile() 内原地替换缓冲区

**方案**: 在 `loadFile()` 中 `fread()` 完成后：
1. 调用 `text_encoding_detect(buf, size)` 检测编码
2. 若为 GBK，分配新 PSRAM 缓冲区（最大 1.5 倍），调用 `text_encoding_gbk_to_utf8(src, src_len, dst, &dst_len)` 转码
3. 释放原缓冲区，将 `textBuffer_` 指向新缓冲区，更新 `textSize_`
4. 若为 UTF-8 BOM，memmove 去掉前 3 字节，更新 `textSize_`
5. 若为纯 UTF-8，不做任何操作

**为什么分配新缓冲区而非 in-place**: GBK 双字节 → UTF-8 三字节，转码后数据可能比原始数据大 50%，无法原地覆盖。

### 4. 组件结构

```
components/text_encoding/
├── CMakeLists.txt
├── include/
│   └── text_encoding.h      // 公共 API
├── text_encoding.c           // 检测 + 转码逻辑
└── gbk_table.c               // GBK→Unicode 映射表（生成）
tools/
└── generate_gbk_table.py     // 从标准码表生成 gbk_table.c
```

公共 API：
```c
typedef enum {
    TEXT_ENCODING_UTF8,
    TEXT_ENCODING_UTF8_BOM,
    TEXT_ENCODING_GBK,
    TEXT_ENCODING_UNKNOWN,
} text_encoding_t;

// 检测缓冲区编码
text_encoding_t text_encoding_detect(const char* buf, size_t len);

// GBK → UTF-8 转码，返回 ESP_OK/ESP_FAIL
// dst 需预分配，dst_len 传入容量、传出实际长度
esp_err_t text_encoding_gbk_to_utf8(const char* src, size_t src_len,
                                     char* dst, size_t* dst_len);
```

## Risks / Trade-offs

- **误判风险**: 纯 ASCII 文件会被判为 UTF-8（正确，ASCII 是 UTF-8 子集）。某些二进制文件或极少见编码可能被误判为 GBK，但阅读器只处理 `.txt`，风险极低。→ 缓解：GBK 解码遇到无效序列输出替换字符，不会崩溃。
- **内存峰值**: 转码时同时持有原始缓冲区和新缓冲区，峰值为文件大小的 2.5 倍（原 1x + 新 1.5x）。8MB 文件峰值 20MB，而 PSRAM 共 8MB。→ 缓解：当前 `loadFile()` 已限制文件最大 8MB，实际中文小说 TXT 一般 1-3MB，峰值 2.5-7.5MB 在 PSRAM 容量内。如有需要可降低文件大小上限。
- **映射表维护**: GBK 是固定标准，映射表不会变化，无需维护。生成脚本保留以备重新生成。
