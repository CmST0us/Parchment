## ADDED Requirements

### Requirement: 缓存层次结构
字体缓存系统 SHALL 实现三级缓存，全部分配在 PSRAM 中：

1. **页面缓存 (Page Cache)**: 5 页滑窗，缓存当前页 ± 2 页的所有已解码 glyph bitmap
2. **常用字缓存 (Common Cache)**: 常驻缓存，包含 ASCII 可打印字符、数字、常用 CJK 标点
3. **回收池 (Recycle Pool)**: 被淘汰页面的 glyph bitmap 回收再利用，最多 1500 条目

#### Scenario: 三级缓存全部在 PSRAM
- **WHEN** 字体缓存系统初始化
- **THEN** 三级缓存的内存均分配在 PSRAM 中

### Requirement: 缓存查找顺序
Glyph 查找 SHALL 按以下顺序搜索：页面缓存 → 常用字缓存 → 回收池 → 文件读取 + RLE 解码。命中任意一级后 SHALL 立即返回，不继续搜索。

#### Scenario: 页面缓存命中
- **WHEN** 请求字符 '好' 且当前页包含 '好'
- **THEN** 从页面缓存直接返回已解码 bitmap，无文件 I/O

#### Scenario: 常用字缓存命中
- **WHEN** 请求字符 'A'
- **THEN** 从常用字缓存直接返回，无文件 I/O

#### Scenario: 回收池命中
- **WHEN** 请求字符 '的' 且该字符在 3 页前使用过、已被回收
- **THEN** 从回收池返回并提升到当前页面缓存

#### Scenario: 全部未命中
- **WHEN** 请求一个从未出现过的字符
- **THEN** 从文件读取 RLE 数据、解码为 4bpp bitmap、存入当前页面缓存后返回

### Requirement: 页面缓存滑窗
页面缓存 SHALL 维护一个 5 页大小的滑动窗口。当翻页导致窗口滑动时：
- 滑出窗口的页面的 glyph SHALL 转入回收池
- 新页面的 glyph SHALL 预加载到页面缓存
- 窗口 SHALL 以当前阅读页为中心

#### Scenario: 向后翻页
- **WHEN** 从第 5 页翻到第 6 页
- **THEN** 窗口变为 [4,5,6,7,8]，第 3 页的 glyph 转入回收池，第 7 和第 8 页的 glyph 被预加载

#### Scenario: 向前翻页
- **WHEN** 从第 6 页翻回第 5 页
- **THEN** 窗口变为 [3,4,5,6,7]，第 8 页的 glyph 转入回收池，第 3 页的 glyph 被预加载

### Requirement: 常用字缓存初始化
常用字缓存 SHALL 在字体加载时填充以下字符的 glyph：
- ASCII 可打印字符 (0x20–0x7E, 共 95 个)
- 常用 CJK 标点：，。、；：！？（）【】《》""''—…
- 阿拉伯数字和常见数学符号

常用字缓存 SHALL 在字体加载期间一次性填充完成，之后不再变化。

#### Scenario: 启动时加载常用字
- **WHEN** 字体文件成功加载
- **THEN** 常用字缓存包含 ASCII 和常用标点的已解码 bitmap

#### Scenario: 常用字缓存不被淘汰
- **WHEN** 页面缓存滑窗滚动或回收池溢出
- **THEN** 常用字缓存内容不受影响

### Requirement: 回收池管理
回收池 SHALL 使用 LRU 淘汰策略，容量上限为 1500 个 glyph。当回收池满时，SHALL 淘汰最久未使用的 glyph 条目并释放其 bitmap 内存。

#### Scenario: 回收池满时淘汰
- **WHEN** 回收池已有 1500 个条目，新的 glyph 需要回收
- **THEN** 淘汰最久未使用的条目，新 glyph 加入回收池

#### Scenario: 回收池命中更新 LRU
- **WHEN** 回收池中的 glyph 被查找命中
- **THEN** 该 glyph 的 LRU 时间戳更新为最新

### Requirement: 缓存的 Glyph 数据格式
缓存中存储的 glyph bitmap SHALL 为已解码的 4bpp 灰度数据（每像素 4 bit，每字节 2 像素），可直接用于缩放和 pushImage 输出，无需再次解码。

#### Scenario: 缓存命中返回已解码数据
- **WHEN** 从任意缓存层命中 glyph
- **THEN** 返回已解码的 4bpp bitmap 指针和尺寸信息，可直接使用

### Requirement: HashMap 索引
字体加载时 SHALL 构建 HashMap 索引，以 Unicode 码点为 key，值为 glyph 条目在文件中的偏移和元数据。查找时间复杂度 SHALL 为 O(1)。

#### Scenario: O(1) 查找
- **WHEN** 查找 Unicode 码点 U+4F60（'你'）
- **THEN** 通过 HashMap 直接获得 glyph 元数据，无需二分查找

#### Scenario: HashMap 内存占用
- **WHEN** 字体包含 7000 个 glyph
- **THEN** HashMap 占用约 7000 × (4+16) = 140KB PSRAM（key + glyph 元数据）
