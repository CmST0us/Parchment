# UI 实现路线图

基于 `docs/ui-wireframe-spec.md`，按细粒度拆解为独立 OpenSpec change。

## 依赖关系

```
Change 1         Change 2           Change 3
icon-resources   widget-components  data-layer
   │                │                  │
   └────────┬───────┘                  │
            │                          │
         Change 4                      │
         boot-screen                   │
            │                          │
            └──────────┬───────────────┘
                       │
                    Change 5
                    library-screen
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
