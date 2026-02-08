## ADDED Requirements

### Requirement: 字体加载
`font_manager` SHALL 使用 stb_truetype 从文件路径加载 TTF/TTC 字体，返回可用于字形渲染的字体句柄。

#### Scenario: 加载 TTF 文件
- **WHEN** 调用 `font_manager_load(path)` 且 `path` 指向有效 TTF 文件
- **THEN** 返回非 NULL 的字体句柄，可用于后续渲染调用

#### Scenario: 加载 TTC 文件
- **WHEN** 调用 `font_manager_load(path)` 且 `path` 指向 TTC 文件（如文泉驿微米黑）
- **THEN** 默认加载第一个字面（index 0），返回有效句柄

#### Scenario: 文件不存在
- **WHEN** 调用 `font_manager_load(path)` 且文件不存在
- **THEN** 返回 NULL 并记录 ESP_LOGE 日志

### Requirement: 字体扫描
`font_manager` SHALL 扫描 LittleFS `/littlefs/` 和 SD 卡 `/sdcard/fonts/` 目录下的 `.ttf` 和 `.ttc` 文件，生成可用字体列表。

#### Scenario: 双源扫描
- **WHEN** 调用 `font_manager_scan()`
- **THEN** 返回包含 LittleFS 和 SD 卡中所有 TTF/TTC 文件路径的列表

#### Scenario: SD 卡不可用
- **WHEN** SD 卡未挂载时调用 `font_manager_scan()`
- **THEN** 仅返回 LittleFS 中的字体文件，不报错

#### Scenario: 无字体文件
- **WHEN** LittleFS 和 SD 卡中均无 TTF/TTC 文件
- **THEN** 返回空列表

### Requirement: 字形渲染
`font_manager` SHALL 将指定 Unicode 码点渲染为 4bpp 灰度位图。

#### Scenario: 渲染 ASCII 字符
- **WHEN** 请求渲染码点 'A'（U+0041），字号 24px
- **THEN** 返回包含字形位图数据、宽度、高度、水平偏移和垂直偏移的结构体

#### Scenario: 渲染中文字符
- **WHEN** 请求渲染码点 '中'（U+4E2D），字号 24px，且当前字体包含该字形
- **THEN** 返回有效的字形位图数据

#### Scenario: 字形不存在
- **WHEN** 请求渲染字体中不包含的码点
- **THEN** 返回 fallback 字形（空矩形或 tofu 块）

### Requirement: 字形缓存
`font_manager` SHALL 在 PSRAM 中维护 LRU 字形缓存，避免重复光栅化。

#### Scenario: 缓存命中
- **WHEN** 同一字号下再次请求已渲染过的码点
- **THEN** 直接从缓存返回位图数据，不重新光栅化

#### Scenario: 缓存淘汰
- **WHEN** 缓存已满且需要渲染新字形
- **THEN** 淘汰最久未使用的缓存条目，为新字形腾出空间

#### Scenario: 字号变化
- **WHEN** 渲染字号与缓存中已有字形的字号不同
- **THEN** 视为不同缓存条目，独立存储

### Requirement: 默认字体
`font_manager` SHALL 在初始化时自动加载 `/littlefs/wqy-microhei.ttc` 作为默认字体。

#### Scenario: 默认字体可用
- **WHEN** 调用 `font_manager_init()` 且内置字体文件存在
- **THEN** 默认字体自动加载，后续渲染调用无需显式指定字体

#### Scenario: 默认字体缺失
- **WHEN** LittleFS 中不存在 `wqy-microhei.ttc`
- **THEN** 记录 ESP_LOGW 日志，`font_manager_get_default()` 返回 NULL
