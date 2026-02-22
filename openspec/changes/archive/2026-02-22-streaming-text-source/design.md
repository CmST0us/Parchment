## Context

当前 `ReaderViewController` 在 `viewDidLoad()` 中将整个 TXT 文件 `fread` 到 PSRAM，检测编码并转为 UTF-8，然后将裸指针交给 `ReaderContentView`。`ReaderContentView` 在首次 `onDraw()` 时同步执行 `paginate()`，遍历全文构建页偏移表。

这种"全加载 + 全分页"策略在文件增大时不可接受：3MB 文件需要 ~5 秒才能显示第一页。

**约束**：
- ESP32-S3 240MHz，8MB PSRAM，16MB Flash
- SD 卡 SPI 读速 ~2 MB/s
- 编译选项 `-fno-exceptions -fno-rtti`
- FreeRTOS 多任务环境

## Goals / Non-Goals

**Goals:**
- 首次打开任意大小文件 < 1 秒显示第一页
- 再次打开有缓存的文件 < 200ms 恢复阅读
- 翻页计算 < 100ms
- 支持大于 PSRAM 容量的文件（理论无上限）
- GBK 文件透明处理，用户无感知差异

**Non-Goals:**
- 缓存清理策略（SD 卡空间充裕，不做清理）
- 支持 TXT 以外的格式（EPUB 等）
- 字形宽度缓存（后续优化，不在本次范围）
- 多文件同时打开

## Decisions

### Decision 1: 文本窗口（TextWindow）替代全文件加载

**选择**: 使用 512KB PSRAM 滑动窗口，按需从 SD 卡加载文本块

**替代方案**:
- A) 保持全文件加载 + 只优化分页 — 不可行，`fread` 3MB = 1.5 秒已超标
- B) mmap — ESP-IDF 不支持 SD 卡 mmap
- C) 分块全部加载到 PSRAM — 仍受 PSRAM 限制，且首块可用前等待时间更长

**理由**: 512KB 窗口覆盖 ~300 页中文文本，远超用户连续阅读需求。加载 512KB 只需 ~250ms，128KB 只需 ~60ms。首页只需加载首块（128KB），其余文本按需加载。

### Decision 2: GBK 预转换到 SD 卡缓存

**选择**: 首次打开 GBK 文件时，后台 FreeRTOS task 流式转换为 UTF-8，写入 `/sdcard/.cache/<hash>/text.utf8`

**替代方案**:
- A) 每次加载 chunk 时实时转换 — 页索引 offset 在 GBK 和 UTF-8 空间不一致，需额外映射表，复杂且易错
- B) 不缓存，每次打开都转换 — 重复浪费时间

**理由**: 预转换后所有操作统一在 UTF-8 空间。页索引 offset 直接对应 text.utf8 文件位置。首页通过内存中快速转换首块实现立即显示。后台转换完成后无缝切换到缓存文件。SD 卡空间占用可忽略（~1.5x 原文件）。

### Decision 3: PageIndex 与 TextSource 分离

**选择**: PageIndex 作为独立辅助类放在 `main/views/`，TextSource 作为独立 component

**替代方案**:
- A) PageIndex 放入 text_source component — 但 PageIndex 依赖排版参数（font_size、line_spacing 等），会导致 text_source 反向依赖渲染层
- B) PageIndex 嵌入 ReaderContentView — 降低可测试性和关注点分离度

**理由**: TextSource 是纯数据 I/O 层（"给我 offset 处的文本"），PageIndex 是排版数据层（"第 N 页从哪个 offset 开始"）。分离使各自职责清晰，且 TextSource 可复用于其他场景。

### Decision 4: 后台任务使用 FreeRTOS task

**选择**: GBK 转换和页索引构建各使用独立的 FreeRTOS task

**替代方案**:
- A) 主循环 tick() 驱动 — 分页速度受帧率（~30fps）限制，3MB 文件可能需要 10+ 秒
- B) 单个后台 task 串行处理 — 可行但代码复杂度相当，不如各自独立

**理由**: ESP32-S3 双核，后台 task 可在 Core 0 运行不阻塞主循环（Core 1）。FreeRTOS task 可连续执行不受帧率限制，分页速度最优。需要 mutex 保护共享数据，但交互点少（PageIndex 的 offsets_ 向量 + TextSource 的状态）。

### Decision 5: 缓存目录结构

**选择**: `/sdcard/.cache/<md5_of_filepath>/` 下存放 `text.utf8` 和 `pages.idx`

**理由**: MD5 哈希与 settings_store 的进度存储键一致（已有先例）。每本书一个目录，结构清晰。`pages.idx` 带 header 校验排版参数变化时自动失效。

### Decision 6: TextWindow UTF-8 字符边界对齐

**选择**: 窗口加载时，起始位置向后扫描找到 UTF-8 字符边界（非 continuation byte, 即非 `10xxxxxx`）

**理由**: UTF-8 是自同步编码，最多向前扫描 3 字节即可找到字符起始。这确保窗口内任何位置的 UTF-8 解码都是正确的。

### Decision 7: 页索引未完成时的翻页策略

**选择**: 顺序翻页始终可用（依赖 layoutPage 返回的 nextOffset），跳转功能在索引完成后才可用

**理由**: 用户 99% 的操作是顺序翻页。layoutPage() 本身就返回下一页 offset，不需要页索引。总页数和进度条跳转是低频操作，等索引完成（几秒）后自然可用。

## Risks / Trade-offs

**[Risk] GBK 转换期间用户快速翻页追上转换进度**
→ Mitigation: 128KB 首块 ≈ 80 页，后台转换速度远快于阅读速度。极端情况下 TextSource::read() 返回空 TextSpan，UI 显示"正在加载..."占位视图。

**[Risk] 后台 task 内存竞争**
→ Mitigation: TextSource 内部 mutex 保护状态和文件切换；PageIndex 使用独立 mutex 保护 offsets_ 向量。交互点少，锁持有时间短（微秒级）。

**[Risk] SD 卡写入 text.utf8 时掉电导致不完整文件**
→ Mitigation: 转换完成后写入一个标记（文件末尾或单独的 `.done` 文件）。下次打开时检查标记，不完整则重新转换。

**[Risk] 页索引缓存参数哈希碰撞**
→ Mitigation: paramsHash 使用 32-bit FNV-1a，包含所有影响分页的参数。碰撞概率极低，且最坏情况只是分页错位（可被用户发现并通过改设置触发重建）。

**[Trade-off] 滑动窗口 vs 全文件加载**
→ 窗口方案在极端随机跳转场景（如快速拖拽进度条）可能触发窗口重新加载（~250ms）。但这比全文件加载的启动延迟好得多。可通过加大窗口到 1MB 缓解。
