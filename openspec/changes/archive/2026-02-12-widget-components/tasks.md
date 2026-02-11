## 1. 文件结构

- [x] 1.1 创建 `components/ui_core/include/ui_widget.h`：所有 widget 类型定义、枚举和函数声明
- [x] 1.2 创建 `components/ui_core/ui_widget.c`：空文件框架，include 必要头文件
- [x] 1.3 更新 `components/ui_core/CMakeLists.txt` 添加 `ui_widget.c`

## 2. Header 组件

- [x] 2.1 实现 `ui_widget_draw_header()`：黑底 48px 标题栏、居中标题文字、左右图标
- [x] 2.2 实现 `ui_widget_header_hit_test()`：左图标区 / 右图标区 / 无命中

## 3. Button 组件

- [x] 3.1 实现 `ui_widget_draw_button()`：4 种样式 (PRIMARY/SECONDARY/ICON/SELECTED)，文字/图标/组合布局
- [x] 3.2 实现 `ui_widget_button_hit_test()`：矩形命中检测

## 4. Progress 组件

- [x] 4.1 实现 `ui_widget_draw_progress()`：双色进度条 + 可选百分比标签
- [x] 4.2 实现 `ui_widget_progress_touch()`：触摸映射到 0-100 值

## 5. List 组件

- [x] 5.1 实现 `ui_widget_draw_list()`：虚拟化绘制，仅调用可见项的 draw_item 回调
- [x] 5.2 实现 `ui_widget_list_hit_test()`：触摸坐标 → item index
- [x] 5.3 实现 `ui_widget_list_scroll()`：scroll_offset 更新 + clamp

## 6. Separator 组件

- [x] 6.1 实现 `ui_widget_draw_separator()`：1px MEDIUM 水平线

## 7. 验证

- [x] 7.1 确认项目编译通过 (`idf.py build`)
