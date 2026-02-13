## 1. Slider Widget

- [x] 1.1 在 `ui_widget.h` 中添加 `ui_slider_t` 结构体定义（x, y, w, h, min_val, max_val, value, step）
- [x] 1.2 在 `ui_widget.h` 中声明 `ui_widget_draw_slider()` 和 `ui_widget_slider_touch()` 函数
- [x] 1.3 在 `ui_widget.c` 中实现 `ui_widget_draw_slider()`：轨道 LIGHT 色、已填充 BLACK 色、把手 BLACK 色
- [x] 1.4 在 `ui_widget.c` 中实现 `ui_widget_slider_touch()`：y±20px 扩展触摸区、值域映射、step 量化、未命中返回 INT_MIN

## 2. Selection Group Widget

- [x] 2.1 在 `ui_widget.h` 中添加 `ui_sel_group_t` 结构体定义（x, y, w, h, items[4], item_count, selected）
- [x] 2.2 在 `ui_widget.h` 中声明 `ui_widget_draw_sel_group()` 和 `ui_widget_sel_group_hit_test()` 函数
- [x] 2.3 在 `ui_widget.c` 中实现 `ui_widget_draw_sel_group()`：等宽分割、选中 BLACK 底 WHITE 字、未选中 WHITE 底 BLACK 字边框
- [x] 2.4 在 `ui_widget.c` 中实现 `ui_widget_sel_group_hit_test()`：y±20px 扩展触摸区、返回选项索引或 -1

## 3. Modal Dialog Widget

- [x] 3.1 在 `ui_widget.h` 中添加 `ui_dialog_t` 结构体定义（title, list, visible）
- [x] 3.2 在 `ui_widget.h` 中声明 `ui_widget_draw_dialog()` 和 `ui_widget_dialog_hit_test()` 函数
- [x] 3.3 在 `ui_widget.c` 中实现 `ui_widget_draw_dialog()`：全屏 MEDIUM 遮罩、居中 WHITE 面板、标题栏、内嵌 list 绘制
- [x] 3.4 在 `ui_widget.c` 中实现 `ui_widget_dialog_hit_test()`：返回 list item index（≥0）、标题栏（-1）或遮罩区域（-2）

## 4. 构建验证

- [x] 4.1 `idf.py build` 编译通过，无错误无警告
