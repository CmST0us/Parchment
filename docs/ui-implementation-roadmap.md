# UI 实现路线图

基于 `docs/ui-wireframe-spec.md`，按细粒度拆解为独立 OpenSpec change。

## 依赖关系

```
Change 1         Change 2           Change 3
icon-resources   widget-components  data-layer   font-littlefs
   │                │                  │          (Phase 1.5)
   └────────┬───────┘                  │
            │                          │
         Change 4                      │
         boot-screen                   │
            │                          │
            └──────────┬───────────────┘
                       │
                    Change 5
                    library-screen ──── font-littlefs
```

## Phase 1: 基础设施

### Change 1: `icon-resources` ✅ 完成 (2026-02-12)
- 图标资源 pipeline (SVG → 4bpp C array)
- Python 转换脚本 `tools/icons/convert_icons.py`
- Tabler Icons (MIT) 作为图标源
- ~8 个图标: arrow-left, menu-2, settings, bookmark, list, sort-descending, typography-abc, chevron-right
- `ui_icon.h/c` 图标结构体 + 绘制函数

### Change 2: `widget-components`
- 通用 UI widget 组件层
- widget_header, widget_button, widget_progress, widget_list, widget_separator
- 依赖 Change 1

### Change 3: `data-layer` ✅ 完成 (2026-02-12)
- `book_store` 组件: SD 卡 TXT 扫描, 书籍列表
- `settings_store` 组件: NVS 读写用户偏好 + 阅读进度
- 独立于 Change 1/2, 可并行

## Phase 1.5: 字体系统

### Change: `font-littlefs` ✅ 完成 (2026-02-12)
- 字体存储从编译内嵌迁移到 LittleFS Flash 分区
- UI 字体 (20/28px): GB2312 一级 + ASCII (~4667 glyphs), 编译为 ROM .h 文件
- 阅读字体 (24/32px): GB18030 (~22380 glyphs), 预生成 .pfnt 二进制文件存入 LittleFS
- 自定义 .pfnt 格式, 运行时加载到 PSRAM
- `fontconvert.py` 支持 `--charset` (gb2312-1/gbk/gb18030) 和 `--output-format` (header/pfnt)
- `generate_fonts.sh` 双轨生成: UI .h + 阅读 .pfnt
- LittleFS 镜像自动集成到 `idf.py build` 流程
- Flash 分区: factory 3MB + fonts 12.9MB
- 独立于 Change 1-3, 但阅读功能 (Phase 3) 依赖此变更

## Phase 2: 页面实现

### Change 4: `boot-screen`
- `main/pages/page_boot.c/h`
- 清理 main.c 临时代码 (text_demo_page, s_lyrics)
- 2 秒后跳转 Library
- 依赖 Change 1+2

### Change 5: `library-screen`
- `main/pages/page_library.c/h`
- 书目列表 + header + 排序 + 滚动
- 依赖 Change 1+2+3+4

## Phase 3: 阅读功能 (后续规划)

### Change 6: `reader-view`
- 核心阅读页面, TXT 文件分页显示

### Change 7: `reader-toolbar`
- 阅读工具栏覆盖层 (返回/进度/目录/设置/书签)

### Change 8: `table-of-contents`
- 目录页面

### Change 9: `reading-settings`
- 字体/行距/边距设置页面

### Change 10: `global-settings`
- 全局设置页面

### Change 11: `epub-support`
- EPUB 文件解析 (zip + XML 元数据)

## 设计决策记录

- **图标**: Tabler Icons (MIT), 32×32 4bpp 灰度位图
- **Widget**: 提前抽象通用组件层
- **页面目录**: `main/pages/`
- **数据存储**: NVS 存进度和设置, SD 卡存书籍
- **EPUB**: 延后, 先只支持 TXT
- **字体存储**: LittleFS Flash 分区 (12.9MB), 编译期预生成 .pfnt 二进制
- **字体覆盖**: UI 用 GB2312 一级 (ROM), 阅读用 GB18030 (PSRAM)
- **字体格式**: 自定义 .pfnt (4bpp zlib 压缩位图, 与 epdiy EpdFont 对齐)
