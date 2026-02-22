## ADDED Requirements

### Requirement: PageIndex 类定义
`PageIndex` SHALL 定义在 `main/views/PageIndex.h`，提供页边界索引的存储、查询和 SD 卡持久化功能。使用 `std::vector<uint32_t>` 存储页偏移表，分配在 PSRAM 中。

#### Scenario: 创建空索引
- **WHEN** 默认构造 `PageIndex`
- **THEN** `pageCount()` 返回 0，`isComplete()` 返回 false

### Requirement: PageIndex 增量构建
PageIndex SHALL 支持通过 `addPage(uint32_t offset)` 逐页添加偏移量。调用方（后台 task）按顺序添加每一页的起始 byte offset。`markComplete()` 标记索引构建完成。

#### Scenario: 逐页添加
- **WHEN** 依次调用 `addPage(0)`, `addPage(1836)`, `addPage(3671)`
- **THEN** `pageCount()` 返回 3，`pageOffset(0)` 返回 0，`pageOffset(2)` 返回 3671

#### Scenario: 标记完成
- **WHEN** 所有页面添加完毕后调用 `markComplete()`
- **THEN** `isComplete()` 返回 true

### Requirement: PageIndex 查询 API
PageIndex SHALL 提供以下查询方法：
- `uint32_t pageCount()` — 当前已知的页数（构建中可能仍在增长）
- `uint32_t pageOffset(uint32_t page)` — 返回第 page 页的起始 byte offset。page 越界时返回 0。
- `bool isComplete()` — 索引是否已覆盖全文
- `uint32_t findPage(uint32_t offset)` — 二分搜索找到包含该 byte offset 的页码

#### Scenario: 二分搜索定位页码
- **WHEN** 索引包含页 0(offset=0), 1(offset=1836), 2(offset=3671)，调用 `findPage(2000)`
- **THEN** 返回 1（offset 2000 落在第 1 页范围内）

#### Scenario: 查询越界页码
- **WHEN** 索引有 100 页，调用 `pageOffset(200)`
- **THEN** 返回 0

### Requirement: PageIndex SD 卡持久化
PageIndex SHALL 支持保存和加载到 SD 卡文件（`pages.idx`）。

文件格式：
- Header: magic "PIDX"(4 bytes) + version(uint16) + file_size(uint32) + params_hash(uint32) + page_count(uint32) = 18 bytes
- Body: uint32_t[page_count] 页偏移数组

`load(path, textSize, paramsHash)` SHALL：
1. 读取 header 并校验 magic、version、file_size、params_hash
2. 任一不匹配则返回 false（缓存失效）
3. 匹配则加载全部页偏移，标记为 complete

`save(path, textSize, paramsHash)` SHALL：
1. 写入 header + 全部页偏移
2. 仅在 `isComplete()` 为 true 时允许保存

`paramsHash` SHALL 由调用方计算，包含所有影响分页的参数：font_size、line_spacing、paragraph_spacing、margin、viewport_w、viewport_h。

#### Scenario: 保存和加载往返
- **WHEN** 构建完 1000 页索引后 `save("pages.idx", 2000000, 0xABCD)`，然后新建 PageIndex 调用 `load("pages.idx", 2000000, 0xABCD)`
- **THEN** 加载成功，`pageCount()` 返回 1000，所有 offset 与保存前一致

#### Scenario: 参数变化导致缓存失效
- **WHEN** 保存时 paramsHash=0xABCD，加载时 paramsHash=0x1234
- **THEN** `load()` 返回 false，索引保持为空

#### Scenario: 文件大小变化导致缓存失效
- **WHEN** 保存时 textSize=2000000，加载时 textSize=2500000（文件被修改）
- **THEN** `load()` 返回 false

### Requirement: PageIndex 线程安全
PageIndex SHALL 使用 mutex 保护 `offsets_` 向量和 `complete_` 标志，因为后台 task 写入的同时主线程可能读取。

`addPage()` 和 `markComplete()` SHALL 加锁写入。
`pageCount()`、`pageOffset()`、`findPage()`、`isComplete()` SHALL 加锁读取。

#### Scenario: 主线程读取同时后台添加
- **WHEN** 后台 task 调用 `addPage()` 同时主线程调用 `pageCount()`
- **THEN** 不产生数据竞争，两者均正确完成
