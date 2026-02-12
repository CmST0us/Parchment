## 1. 分区表与 LittleFS 挂载

- [x] 1.1 修改 `partitions.csv`：将 `storage` FAT 分区替换为 `fonts, data, spiffs, 0x410000, 0xA00000,`
- [x] 1.2 在 `components/ui_core/ui_font.c` 中实现 `ui_font_init()`：使用 `esp_vfs_littlefs_register()` 挂载 `fonts` 分区到 `/fonts`，挂载失败时格式化重试
- [x] 1.3 在 `main/main.c` 的 `app_main()` 中，于 `ui_core_start()` 之前调用 `ui_font_init()`

## 2. .pfnt 二进制格式定义

- [x] 2.1 新建 `components/ui_core/include/ui_font_pfnt.h`：定义 `pfnt_header_t`、文件 magic "PFNT"、版本号等结构体
- [x] 2.2 实现 .pfnt 加载函数 `pfnt_load(const char *path)`：打开文件、校验 magic/version、分配 PSRAM、读取 intervals/glyphs/bitmap 数据、构建 `EpdFont` 结构体并返回指针
- [x] 2.3 实现 .pfnt 卸载函数 `pfnt_unload(EpdFont *font)`：释放 PSRAM 中的 bitmap、glyph、intervals 内存

## 3. ui_font 重构

- [x] 3.1 更新 `ui_font.h`：新增 `ui_font_init()` 声明、`ui_font_list_available(int *sizes_out, int max_count)` 声明；更新 extern 声明为 `ui_font_20` 和 `ui_font_28`；移除旧的 6 个字体 extern
- [x] 3.2 重写 `ui_font.c`：include 新的 `ui_font_20.h` 和 `ui_font_28.h`；重建 `ui_font_get()` 路由逻辑——20/28px 返回 ROM，其他字号自动加载/返回 PSRAM 阅读字体
- [x] 3.3 实现 `ui_font_list_available()`：扫描 `/fonts/*.pfnt` 目录，解析每个文件 header 中的 font_size_px，返回可用字号列表
- [x] 3.4 实现字体切换逻辑：当请求新的阅读字号时，先卸载当前阅读字体，再加载新字号的 .pfnt

## 4. fontconvert.py 扩展

- [x] 4.1 新增 `--output-format` 参数（`header`/`pfnt`，默认 `header`）
- [x] 4.2 新增 `--charset` 参数（`custom`/`gb2312-1`/`gbk`/`gb18030`，默认 `custom`），内置各字符集的 Unicode 码点范围定义
- [x] 4.3 实现 pfnt 二进制输出模式：按 .pfnt 格式规范写入 header、intervals、glyph table、bitmap data 到 stdout
- [x] 4.4 验证 `--output-format header --charset custom` 默认行为与修改前一致

## 5. generate_fonts.sh 重写

- [x] 5.1 重写脚本为双轨生成：UI 字体（`--charset gb2312-1 --output-format header`，20/28px → `components/ui_core/fonts/ui_font_{20,28}.h`）和阅读字体（`--charset gb18030 --output-format pfnt`，24/32px → `fonts_data/noto_cjk_{24,32}.pfnt`）
- [x] 5.2 自动创建 `fonts_data/` 和 `components/ui_core/fonts/` 输出目录

## 6. CMake 构建集成

- [x] 6.1 在 `main/CMakeLists.txt`（或 `components/ui_core/CMakeLists.txt`）中添加 `littlefs_create_partition_image(fonts ../fonts_data FLASH_IN_PROJECT)` 调用
- [x] 6.2 在 `components/ui_core/CMakeLists.txt` 中添加对 `esp_littlefs` 的 REQUIRES 依赖
- [x] 6.3 确认 `idf.py build` 生成 LittleFS 镜像，`idf.py flash` 正确烧录到 fonts 分区

## 7. 旧字体清理

- [x] 7.1 删除 `components/ui_core/fonts/` 下的 6 个旧文件：`noto_sans_cjk_{20,24,28,32,36,40}.h`
- [x] 7.2 更新 `components/ui_core/CMakeLists.txt` 的 SRCS，确认不再引用旧字体头文件

## 8. 字体生成与构建验证

- [x] 8.1 使用 NotoSansCJKsc-Regular.otf 运行 `generate_fonts.sh` 生成 UI 字体 .h 和阅读字体 .pfnt
- [x] 8.2 执行 `idf.py build` 确认编译通过，无错误无警告
- [x] 8.3 验证 build 输出中包含 LittleFS 镜像且大小合理（~10.6MB for 2 个 GB18030 字号）

## 9. Roadmap 文档更新

- [x] 9.1 更新 `docs/ui-implementation-roadmap.md`：在 Phase 1 和 Phase 2 之间插入 Phase 1.5 `font-littlefs`，更新依赖关系图
