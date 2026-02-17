## 1. GBK 映射表生成工具

- [x] 1.1 创建 `tools/generate_gbk_table.py` 脚本，从 Python 内置 codecs 读取 GBK 全部双字节映射（首字节 0x81-0xFE，次字节 0x40-0xFE 跳过 0x7F），输出 C 数组 `const uint16_t gbk_to_unicode[126][190]`
- [x] 1.2 运行脚本生成 `components/text_encoding/gbk_table.c`，验证关键码点映射正确（如 B0A1→U+554A "啊"，C4E3→U+4F60 "你"）

## 2. text_encoding 组件骨架

- [x] 2.1 创建 `components/text_encoding/` 目录结构：`CMakeLists.txt`、`include/text_encoding.h`、`text_encoding.c`
- [x] 2.2 在 `text_encoding.h` 中定义 `text_encoding_t` 枚举（UTF8、UTF8_BOM、GBK、UNKNOWN）和两个公共 API 函数声明
- [x] 2.3 配置 `CMakeLists.txt`，注册组件并声明对 `esp_common` 的依赖（用于 `esp_err_t`）

## 3. 编码检测实现

- [x] 3.1 实现 `text_encoding_detect()`：BOM 检测（前 3 字节 EF BB BF）→ UTF-8 严格校验（遍历全部字节，检查多字节序列合法性，拒绝 C0-C1/F5-FF 首字节）→ fallback GBK
- [x] 3.2 处理边界情况：空缓冲区返回 UTF8，纯 ASCII 返回 UTF8

## 4. GBK→UTF-8 转码实现

- [x] 4.1 实现 `text_encoding_gbk_to_utf8()`：遍历源缓冲区，ASCII 直接输出，双字节通过 gbk_to_unicode 表查 Unicode codepoint，再编码为 UTF-8（1/2/3 字节）
- [x] 4.2 实现无效序列处理：映射值为 0 或次字节越界时输出 U+FFFD（EF BF BD）
- [x] 4.3 实现目标缓冲区容量检查：写入前检查剩余空间，不足时截断到最后一个完整字符

## 5. ReaderViewController 集成

- [x] 5.1 在 `ReaderViewController.cpp` 中 `#include "text_encoding.h"`，在 `loadFile()` 的 `fread()` 之后插入编码检测调用
- [x] 5.2 实现 GBK 分支：分配 1.5 倍 + 1 的 PSRAM 新缓冲区，调用转码，释放原缓冲区，更新 `textBuffer_` 和 `textSize_`；分配失败时清理并返回 false
- [x] 5.3 实现 UTF-8 BOM 分支：memmove 跳过前 3 字节，textSize_ 减 3
- [x] 5.4 在 `main/CMakeLists.txt` 中添加 `text_encoding` 组件依赖

## 6. 构建验证

- [x] 6.1 执行 `idf.py build` 确认编译通过，无 warning
- [x] 6.2 确认 gbk_table.c 的映射表被正确放入 flash（`.rodata` 段），不占 RAM
