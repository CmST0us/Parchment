## Why

当前字体以 C header 嵌入固件，仅包含 312 个手选字符。作为阅读器，需要完整的 CJK 字符覆盖（GB18030）才能正常显示中日英书籍内容。将字体迁移到 LittleFS 分区存储，可支撑 ~27,564 个字符的完整字体集，同时允许用户替换预生成的 `.pfnt` 字体文件。

## What Changes

- 新建 `fonts` LittleFS 分区（10MB），替换现有未使用的 `storage` FAT 分区
- 设计 `.pfnt` 二进制字体格式（自包含：一个文件 = 一个字体 + 一个字号）
- 扩展 `tools/fontconvert.py`，新增 `--output-format=pfnt` 输出模式
- 更新 `tools/generate_fonts.sh`，分别生成 UI 嵌入字体（.h）和阅读字体（.pfnt）
- 新建字体加载器：mount LittleFS、从文件读取 `.pfnt` 到 PSRAM、构建 `EpdFont` 结构体
- 重构 `ui_font.c`：UI 字号（20/28px）返回 ROM 嵌入字体，阅读字号从 LittleFS 动态加载
- 精简 UI 嵌入字体字符集为 ASCII + GB2312 一级常用字（~3,850 字符），仅保留 20px 和 28px 两个字号
- 删除旧的 6 个全量字体 `.h` 文件（20/24/28/32/36/40px × 312 字符）
- CMake 集成 `littlefs_create_partition_image`，`idf.py build` 自动打包 `.pfnt` 到 LittleFS 镜像
- 预留 SD 卡自定义字体接口（`/sdcard/fonts/*.pfnt`），本次不实现加载逻辑

## Capabilities

### New Capabilities
- `font-littlefs-storage`: LittleFS 分区挂载、`.pfnt` 二进制格式定义、字体文件运行时加载到 PSRAM、字体生命周期管理（load/unload）
- `font-build-pipeline`: 字体构建工具链——fontconvert.py 的 pfnt 输出模式、generate_fonts.sh 的 UI/阅读双轨生成、CMake LittleFS 镜像自动打包

### Modified Capabilities
- `font-assets`: 字符集从 312 个手选字符扩展为 ASCII + GB2312 一级（UI 嵌入）和 GB18030（阅读字体）；字号从 6 个缩减为 2 个 UI 嵌入（20/28px）+ 2 个 LittleFS 阅读字体（默认 24/32px）

## Impact

- **`partitions.csv`**: `storage` FAT 分区替换为 `fonts` LittleFS 分区（10MB）
- **`main/main.c`**: 启动时调用 `ui_font_init()` 挂载 LittleFS
- **`components/ui_core/ui_font.c/h`**: 重构——新增 `ui_font_init()`、`ui_font_list_available()`，`ui_font_get()` 内部逻辑分 UI/阅读两路
- **`components/ui_core/fonts/`**: 删除 6 个旧 .h，替换为 2 个新 UI .h（字符集更大但字号更少）
- **`tools/fontconvert.py`**: 新增 `--output-format pfnt` 和 `--charset` 参数
- **`tools/generate_fonts.sh`**: 重写为双轨生成（UI .h + 阅读 .pfnt）
- **`main/CMakeLists.txt`**: 新增 `littlefs_create_partition_image` 调用
- **新目录**: `fonts_data/` 存放预生成的 `.pfnt` 文件
- **依赖**: esp_littlefs 组件（已有 submodule）、miniz（已有，用于解压）
- **无 API 破坏性变更**: `ui_font_get()` 签名不变，调用方代码无需修改
