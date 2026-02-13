# UI 实现路线图

基于 `docs/ui-wireframe-spec.md`，按细粒度拆解为独立 OpenSpec change。

## 依赖关系

```
Phase 1: 基础设施                    Phase 1.5
  Change 1           Change 2           Change 3         font-littlefs
  icon-resources ✅   widget-components ✅  data-layer ✅     ✅
     │                │                  │               │
     └────────┬───────┘                  │               │
              │                          │               │
Phase 2:   Change 4                      │               │
           boot-screen ✅                │               │
              │                          │               │
              └──────────┬───────────────┴───────────────┘
                         │
                      Change 5
                      library-screen ✅
                         │
Phase 3:                 │
           ┌─────────────┤
           │             │
        Change 6      Change 10
        reader-view   global-settings
           │
     ┌─────┼─────┬──────────┐
     │     │     │          │
  Change 7  Change 8  Change 9  Change 11
  toolbar   toc       rd-settings  epub
```

## Phase 1: 基础设施

### Change 1: `icon-resources` ✅ 完成 (2026-02-12)
- 图标资源 pipeline (SVG → 4bpp C array)
- Python 转换脚本 `tools/icons/convert_icons.py`
- Tabler Icons (MIT) 作为图标源
- ~8 个图标: arrow-left, menu-2, settings, bookmark, list, sort-descending, typography-abc, chevron-right
- `ui_icon.h/c` 图标结构体 + 绘制函数

### Change 2: `widget-components` ✅ 完成 (2026-02-12)
- 通用 UI widget 组件层
- 基础: widget_header, widget_button, widget_progress, widget_list, widget_separator
- 扩展: widget_slider, widget_selection_group, widget_modal_dialog
- 依赖 Change 1

### Change 3: `data-layer` ✅ 完成 (2026-02-12)
- `book_store` 组件: SD 卡 TXT 扫描, 书籍列表
- `settings_store` 组件: NVS 读写用户偏好 + 阅读进度
- 独立于 Change 1/2, 可并行

## Phase 1.5: 字体系统

### Change: `font-littlefs` ✅ 完成 (2026-02-12)
- **所有字体统一 .pfnt 格式**，全部存 LittleFS Flash 分区，无 ROM 嵌入
- UI 字体 (16/24px): LXGWWenKaiMono, GB2312 一级 + ASCII, boot 时常驻 PSRAM (~1.5MB)
- 阅读字体 (24/32px): LXGWWenKai, GB2312 + 日语, 按需加载到 PSRAM (一次一个, ~5-7MB)
- 自定义 .pfnt 格式: header 32B + intervals 12B/each + glyphs 20B/each + zlib 压缩位图
- `fontconvert.py` 支持 `--charset` (gb2312-1/gb2312+ja/gbk/gb18030) 和 `--output-format` (header/pfnt)
- `generate_fonts.sh` 一键生成 4 个 .pfnt 到 `fonts_data/`
- LittleFS 镜像通过 `littlefs_create_partition_image` 集成到 `idf.py build`
- Flash 分区: factory 1MB + fonts 15.6MB

### Bugfix: `font-glyph-offset` (2026-02-13)
- **问题**: fontconvert.py 中 `_COMMON_INTERVALS` 包含字体不支持的码点，光栅化时跳过但 interval 偏移仍按全量计算，导致后续所有 CJK glyph 偏移错位，显示乱码
- **修复**: 新增 `_filter_by_font()` 在创建 interval 前过滤字体不支持的码点
- **影响**: 需重新生成所有 .pfnt 字体文件

## Phase 2: 页面实现

### Change 4: `boot-screen` ✅ 完成 (2026-02-12)
- `main/pages/page_boot.c/h`
- 清理 main.c 临时代码 (text_demo_page, s_lyrics)
- 2 秒后跳转 Library
- 依赖 Change 1+2

### Change 5: `library-screen` ✅ 完成 (2026-02-13)
- `main/pages/page_library.c/h`
- 书目列表 + header + 排序 + 滚动
- 依赖 Change 1+2+3+4 + font-littlefs

## Phase 3: 阅读功能 ← 当前阶段

### Change 6: `reader-view`
- 核心阅读页面, TXT 文件分页显示
- 依赖 Change 5 + font-littlefs

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
- **Widget**: 通用组件层 (header/button/progress/list/separator/slider/selection/modal)
- **页面目录**: `main/pages/`
- **数据存储**: NVS 存进度和设置, SD 卡存书籍
- **EPUB**: 延后, 先只支持 TXT
- **字体存储**: 统一 .pfnt 格式, 全部存 LittleFS Flash 分区 (15.6MB)
- **字体覆盖**: UI 用 GB2312 一级 (常驻 PSRAM), 阅读用 GB2312+日语 (按需加载)
- **字体格式**: 自定义 .pfnt (4bpp zlib 压缩位图, Unicode codepoint 索引, 与 epdiy EpdFont 对齐)
