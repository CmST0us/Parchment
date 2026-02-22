## 1. TextSource component 基础结构

- [x] 1.1 创建 `components/text_source/` 目录结构（CMakeLists.txt, include/text_source/TextSource.h, src/）
- [x] 1.2 实现 TextWindow 类：PSRAM 512KB 滑动缓冲区，`load(offset)` 从文件加载并对齐 UTF-8 边界，`spanAt(offset)` 返回 TextSpan
- [x] 1.3 实现 TextSource 核心：`open()`/`close()` 生命周期，UTF-8 文件直接打开路径（状态 Closed → Preparing → Ready），`read(offset)` 通过 TextWindow 返回 TextSpan，`state()`/`totalSize()`/`availableSize()` 查询

## 2. GBK 后台转换

- [x] 2.1 实现 EncodingConverter 类：检测 GBK 编码，内存转换首块 128KB，创建缓存目录
- [x] 2.2 实现 FreeRTOS 后台转换 task：流式读 64KB GBK → 转 UTF-8 → 写 `text.utf8`，写入完成标记 "DONE"，更新 `availableSize_`
- [x] 2.3 TextSource 集成 EncodingConverter：GBK 文件状态流转（Converting → Ready），缓存命中检测（text.utf8 + 完成标记），转换完成后切换读取源，mutex 保护共享状态
- [x] 2.4 处理不完整缓存：打开时检测 text.utf8 无完成标记则删除并重新转换

## 3. PageIndex

- [x] 3.1 实现 PageIndex 类（main/views/PageIndex.h/.cpp）：`addPage()`/`markComplete()`/`pageCount()`/`pageOffset()`/`findPage()` 基础 API，mutex 线程安全
- [x] 3.2 实现 PageIndex SD 卡持久化：`save()`/`load()` 带 header 校验（magic "PIDX" + version + file_size + params_hash），二进制读写

## 4. ReaderContentView 重构

- [x] 4.1 替换 `textBuf_`/`textSize_` 为 `TextSource*`：新增 `setTextSource()`，移除 `setTextBuffer()`
- [x] 4.2 重构 `layoutPage()` 从 TextSource::read() 获取文本，处理 TextSpan 为空的情况返回空 PageLayout
- [x] 4.3 集成 PageIndex 替代 `std::vector<uint32_t> pages_`：首次 onDraw 尝试加载缓存，缓存未命中启动后台分页 task
- [x] 4.4 实现后台分页 FreeRTOS task：循环 layoutPage() + addPage()，完成后 markComplete() + save()，task 自动退出
- [x] 4.5 实现顺序翻页不依赖完整索引：翻到超出 PageIndex 范围时通过 layoutPage() 的 nextOffset 即时扩展
- [x] 4.6 重构 `onDraw()` 渲染逻辑：从 TextSource::read() 获取文本，TextSpan 为空时显示 "正在加载..." 居中提示
- [x] 4.7 实现分页 API 更新：`totalPages()` 索引未完成返回 -1，`pageIndexProgress()` 返回构建进度，`isPageIndexComplete()`
- [x] 4.8 实现 `setStatusCallback()` 状态回调：后台 task 每 10% 进度或完成时触发回调

## 5. ReaderViewController 重构

- [x] 5.1 替换 `textBuffer_` 为 `TextSource textSource_` 成员变量：移除 loadFile() 的 fread/encoding 逻辑
- [x] 5.2 重构 `viewDidLoad()`：计算 cacheDirPath（md5 of path），创建缓存目录，调用 `textSource_.open()`，调用 `contentView_->setTextSource()`
- [x] 5.3 实现页脚动态状态显示：Converting 时显示 "正在转换编码... XX%"，Indexing 时显示 "正在索引... XX%"，完成后显示 "page/total percent%"
- [x] 5.4 更新进度保存/恢复逻辑：`viewWillDisappear()` 保存 UTF-8 byte_offset，`viewDidLoad()` 恢复 initialByteOffset
- [x] 5.5 清理：移除 `textBuffer_` 相关的 `heap_caps_free`，移除旧的编码检测/转换代码，更新 main/CMakeLists.txt 依赖

## 6. 构建集成

- [x] 6.1 更新 `main/CMakeLists.txt` 添加 PageIndex 源文件和 text_source 依赖
- [ ] 6.2 确保编译通过：`idf.py build` 无错误
