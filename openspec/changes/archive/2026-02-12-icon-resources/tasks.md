## 1. SVG 源文件获取

- [x] 1.1 创建 `tools/icons/src/` 目录
- [x] 1.2 从 Tabler Icons 下载 8 个 SVG 文件 (arrow-left, menu-2, settings, bookmark, list, sort-descending, typography, chevron-right)

## 2. 转换脚本

- [x] 2.1 创建 `tools/icons/convert_icons.py`，实现 SVG → 32×32 灰度 → 4bpp 量化 → C 数组输出
- [x] 2.2 运行脚本生成 `components/ui_core/ui_icon_data.h`

## 3. Canvas 着色绘制

- [x] 3.1 在 `ui_canvas.h` 中声明 `ui_canvas_draw_bitmap_fg()` 函数
- [x] 3.2 在 `ui_canvas.c` 中实现 `ui_canvas_draw_bitmap_fg()`：4bpp alpha 混合、边界裁剪、NULL 保护

## 4. 图标模块

- [x] 4.1 创建 `ui_icon.h`：`ui_icon_t` 结构体定义 + 8 个图标 extern 常量声明 + `ui_icon_draw()` 函数声明
- [x] 4.2 创建 `ui_icon.c`：include 生成的 `ui_icon_data.h`，定义图标常量实例 + `ui_icon_draw()` 实现
- [x] 4.3 更新 `components/ui_core/CMakeLists.txt` 添加 `ui_icon.c` 到源文件列表

## 5. 验证

- [x] 5.1 确认项目编译通过 (`idf.py build`)
