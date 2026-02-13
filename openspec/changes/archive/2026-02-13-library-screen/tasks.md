## 1. 页面框架

- [x] 1.1 创建 `main/pages/page_library.h`，声明 `extern ui_page_t page_library`
- [x] 1.2 创建 `main/pages/page_library.c`，实现页面骨架（on_enter/on_exit/on_event/on_render 四个回调 + static 状态变量）
- [x] 1.3 在 `on_enter` 中调用 `book_store_scan()` 扫描 SD 卡，调用 `settings_store_load_sort_order()` 恢复排序偏好，批量缓存所有书籍的阅读进度
- [x] 1.4 在 `on_enter` 中调用 `ui_render_mark_full_dirty()` 触发首次全屏渲染

## 2. 页面渲染

- [x] 2.1 实现 Header 渲染：使用 `ui_widget_draw_header()` 绘制黑底白字 Header（左侧 menu-2 图标、标题 "Parchment"、右侧 settings 图标）
- [x] 2.2 实现 Subheader 渲染：在 y=48 绘制 36px 高 LIGHT 底色区域，显示 "N 本书" 和排序方式文字
- [x] 2.3 实现书目项绘制回调函数 `draw_book_item()`：格式标签区域（60×80, "TXT"）、书名（20px BLACK）、作者 "Unknown"（16px DARK）、进度条（ui_progress_t 200×8）、分隔线
- [x] 2.4 配置 `ui_list_t` Widget（x=0, y=84, w=540, h=876, item_height=96），设置 draw_item 回调和 item_count
- [x] 2.5 实现空状态显示：书籍数量为 0 时在列表区域居中显示 "未找到书籍" 提示文字

## 3. 事件处理

- [x] 3.1 实现列表滑动滚动：`SWIPE_UP` / `SWIPE_DOWN` 事件调用 `ui_widget_list_scroll()`，步长 96px，标记列表区域脏区域
- [x] 3.2 实现书目项点击：`TOUCH_TAP` 事件中通过 `ui_widget_list_hit_test()` 检测点击项，当前输出日志（预留 `ui_page_push` 接口）
- [x] 3.3 实现 Header 按钮点击：`ui_widget_header_hit_test()` 检测左/右图标点击，当前输出日志
- [x] 3.4 实现排序按钮点击：检测 Subheader 排序区域点击，弹出排序 modal dialog

## 4. 排序功能

- [x] 4.1 配置排序 `ui_dialog_t`，包含 "按名称排序" 和 "按大小排序" 两个选项
- [x] 4.2 实现排序选择处理：选择后调用 `book_store_sort()`、重新缓存阅读进度、重置滚动偏移、关闭 dialog、调用 `settings_store_save_sort_order()` 持久化
- [x] 4.3 排序 dialog 可见时绘制 dialog 覆盖层，处理 dialog 内的触摸事件

## 5. Boot 页面集成

- [x] 5.1 修改 `main/pages/page_boot.c`，在定时器回调中将跳转目标改为 `ui_page_replace(&page_library, NULL)`
- [x] 5.2 在 `page_boot.c` 中添加 `#include "page_library.h"`

## 6. 验证

- [x] 6.1 确认编译通过（`idf.py build`）
- [x] 6.2 确认页面生命周期正常：boot → library 跳转、列表渲染、滚动、排序切换
