## Why

中文 TXT 文件常见 GBK/GB2312 编码，当前阅读器整条渲染管线（加载 → 分页 → 渲染）硬性假设 UTF-8，导致 GBK 编码的书籍显示乱码。这是阅读器的核心功能缺陷，必须在正式使用前解决。

## What Changes

- 新增文件编码检测：在 `ReaderViewController::loadFile()` 读取文件后，自动识别 UTF-8 BOM、GBK/GB2312 等常见中文编码
- 新增 GBK → UTF-8 转码：检测到非 UTF-8 编码时，将整个文本缓冲区原地转换为 UTF-8，后续管线无需修改
- 新增 UTF-8 BOM 剥离：如果文件以 UTF-8 BOM (EF BB BF) 开头，跳过这 3 个字节
- 转码方案：内嵌精简 GBK→Unicode 映射表（~44KB），不依赖外部库（ESP-IDF 无 iconv）

## Capabilities

### New Capabilities
- `text-encoding`: 文本文件编码检测与转换，覆盖 UTF-8（含 BOM）、GBK、GB2312 的自动识别和统一转为 UTF-8

### Modified Capabilities
- `reader-viewcontroller`: loadFile 流程新增编码检测与转码步骤

## Impact

- **新增代码**: `components/text_encoding/` 组件（编码检测 + GBK→UTF-8 转换）
- **修改代码**: `main/controllers/ReaderViewController.cpp` 的 `loadFile()` 方法，在 fread 后插入编码检测与转码调用
- **内存影响**: GBK→Unicode 映射表 ~44KB（存 flash, 按需读取或常驻），转码时临时缓冲区最大为原文件 1.5 倍（GBK 双字节 → UTF-8 三字节）
- **无 breaking change**: 已有的 UTF-8 文件不受影响，检测到 UTF-8 时直接跳过转码
