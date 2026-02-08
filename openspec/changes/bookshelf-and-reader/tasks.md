## 1. 基础设施：分区与 LittleFS

- [x] 1.1 修改 `partitions.csv`：`storage` 分区改为 littlefs 类型，8MB
- [x] 1.2 通过 ESP-IDF 组件管理器添加 `joltwallet/littlefs` 依赖
- [x] 1.3 创建 `components/littlefs_storage/` 组件：初始化、挂载 `/littlefs`、卸载 API
- [x] 1.4 创建项目根目录 `fonts/` 文件夹，下载文泉驿微米黑 `wqy-microhei.ttc`
- [x] 1.5 在 `main/CMakeLists.txt` 配置 `littlefs_create_partition_image` 打包字体目录
- [x] 1.6 在 `main.c` 中集成 LittleFS 挂载，验证 `/littlefs/wqy-microhei.ttc` 可读

## 2. 字体管理器：stb_truetype 集成

- [x] 2.1 将 `stb_truetype.h` 引入项目（`components/font_manager/` 下）
- [x] 2.2 实现 `font_manager_init()` / `font_manager_deinit()`：加载默认字体
- [x] 2.3 实现 `font_manager_load(path)`：从文件读取 TTF/TTC 到 PSRAM，初始化 stbtt_fontinfo
- [x] 2.4 实现 `font_manager_scan()`：扫描 LittleFS + SD 卡字体目录，返回字体路径列表
- [x] 2.5 实现字形渲染：`font_manager_get_glyph(font, codepoint, size)` 返回 4bpp 位图
- [x] 2.6 实现 PSRAM LRU 字形缓存，key = (codepoint, size)，可配置容量
- [x] 2.7 编译验证：加载内置字体并渲染单个字符到 framebuffer

## 3. 文本排版引擎

- [x] 3.1 创建 `components/text_layout/` 组件，定义排版参数结构体（字号、行距、边距）
- [x] 3.2 实现 UTF-8 码点迭代器（从 byte buffer 提取下一个 codepoint）
- [x] 3.3 实现自动换行算法：中文逐字断行、英文按词断行、混排处理
- [x] 3.4 实现 `text_layout_render_page(fb, text, len, params)`：排版并渲染一页文本到 framebuffer
- [x] 3.5 实现 `text_layout_calc_page_end(text, len, params)`：计算一页容纳的文本字节数
- [x] 3.6 验证：渲染中英文混排段落到屏幕，检查换行和布局正确性

## 4. 文本分页器

- [x] 4.1 创建 `components/text_paginator/` 组件（或放在 text_layout 中），定义 `page_index_t` 结构
- [x] 4.2 实现 `paginator_build()`：逐页扫描文件生成 offsets 数组
- [x] 4.3 实现 `.idx` 缓存文件写入（header + offsets 二进制格式）
- [x] 4.4 实现 `.idx` 缓存文件读取 + 有效性校验（magic / 参数 hash 比对）
- [x] 4.5 实现 `paginator_read_page()`：根据 offsets 从文件读取指定页文本
- [x] 4.6 验证：对测试 TXT 文件生成分页表，翻到指定页读取内容

## 5. 书架页面

- [x] 5.1 创建 `main/pages/bookshelf_page.c/.h`，实现页面结构体和回调
- [x] 5.2 实现 `on_enter`：扫描 `/sdcard/books/*.txt`，构建文件名列表
- [x] 5.3 实现 `on_render`：渲染书名列表（使用 text_layout 渲染每行书名）
- [x] 5.4 实现书架列表分页：SWIPE_UP/DOWN 翻页
- [x] 5.5 实现选书命中测试：TAP 某行 → `ui_page_push(&reader_page, file_path)`
- [x] 5.6 处理空书架状态：无书籍时显示提示图形

## 6. 阅读页面

- [x] 6.1 创建 `main/pages/reader_page.c/.h`，实现页面结构体和回调
- [x] 6.2 实现 `on_enter`：接收文件路径，加载/生成分页表
- [x] 6.3 实现 `on_render`：读取当前页文本，调用 text_layout 渲染内容 + 底部页码
- [x] 6.4 实现翻页逻辑：左半屏 TAP 上一页、右半屏 TAP 下一页
- [x] 6.5 实现长按返回书架：LONG_PRESS → `ui_page_pop()`
- [x] 6.6 处理边界情况：首页/末页翻页无响应

## 7. 集成与入口

- [x] 7.1 修改 `main/main.c`：添加 LittleFS 挂载步骤，入口页面改为 bookshelf_page
- [x] 7.2 更新 `main/CMakeLists.txt`：添加 pages 源文件和新 component 依赖
- [x] 7.3 全量编译通过
- [ ] 7.4 端到端验证：书架显示书籍 → 点击进入阅读 → 翻页 → 长按返回
