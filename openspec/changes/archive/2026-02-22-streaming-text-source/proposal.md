## Why

当前 ReaderViewController 在打开书籍时将整个文件一次性加载到 PSRAM（`fread` 全文件 + 编码转换 + 全文 `paginate()`），导致大文件打开速度极慢（3MB 文件约 5 秒）。需要实现流式文本加载架构，达到**首次打开 < 1 秒、翻页 < 100ms** 的性能目标。

## What Changes

- **新增 `text_source` component**：独立的 UTF-8 文本访问层，封装文件 I/O、编码转换、PSRAM 滑动窗口缓冲，对外提供统一的按 offset 读取接口
- **新增 `PageIndex` 辅助类**：页边界索引数据结构 + SD 卡持久化（`pages.idx`），支持增量构建和缓存验证
- **新增 SD 卡缓存目录**：`/sdcard/.cache/<md5>/` 存放 UTF-8 转换缓存和页索引缓存
- **重构 `ReaderViewController`**：移除全文件 `textBuffer_` 加载，改用 TextSource 提供文本访问
- **重构 `ReaderContentView`**：移除 `textBuf_`/`textSize_` 和同步 `paginate()`，改用 TextSource + PageIndex + 后台 FreeRTOS task 分页
- **新增加载状态 UI**：GBK 转换中显示 "正在转换编码..."，页索引构建中显示 "正在索引页面..."，完成后显示完整页码

## Capabilities

### New Capabilities
- `text-source`: 流式文本访问层——文件打开、编码检测与 GBK 后台转换、UTF-8 滑动窗口缓冲、SD 卡缓存管理
- `page-index`: 页边界索引——存储/加载/增量构建页偏移表，SD 卡持久化与参数校验

### Modified Capabilities
- `reader-content-view`: 从全文加载改为流式 TextSource 读取，paginate 改为后台 FreeRTOS task，新增加载状态 UI 显示
- `reader-viewcontroller`: 移除全文件 `textBuffer_` 管理，改用 TextSource 所有权模型，新增缓存目录计算

## Impact

- **新增文件**: `components/text_source/`（TextSource.h/.cpp, TextWindow, EncodingConverter, CMakeLists.txt）、`main/views/PageIndex.h/.cpp`
- **修改文件**: `main/controllers/ReaderViewController.cpp/.h`、`main/views/ReaderContentView.cpp/.h`、`main/CMakeLists.txt`
- **依赖**: text_source 依赖 text_encoding + sd_storage；ReaderContentView 依赖 text_source
- **线程安全**: TextSource 内部 mutex 保护转换状态，PageIndex mutex 保护 offsets_ 向量
- **存储**: SD 卡新增 `/sdcard/.cache/` 目录，不做清理
