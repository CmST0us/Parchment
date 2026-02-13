## Context

Parchment 是一个基于 ESP32-S3 的墨水屏阅读器，当前已完成：
- 图标资源 pipeline、通用 Widget 组件层（header、button、list、progress、separator、slider、selection group、modal dialog）
- 数据层（`book_store` SD 卡扫描、`settings_store` NVS 持久化）
- 字体系统（LittleFS + PSRAM 加载）
- 启动画面（`page_boot`）
- UI 框架（ui_core：页面栈、事件队列、画布绘图、脏区域刷新、手势识别）

书库界面是启动后的首个交互页面。用户在此浏览 SD 卡中的书籍、切换排序方式、点击进入阅读。

约束：
- 540×960 逻辑坐标，4bpp 灰度 framebuffer
- E-Ink 刷新慢：全屏 ~1s，局部 ~0.3s，须最小化刷新区域
- PSRAM 有限（8MB 总量，UI 字体 ~1.5MB + fb ~1MB + 阅读字体 ~5-7MB）
- `book_store` 最多支持 64 本书
- 当前仅支持 TXT 格式

## Goals / Non-Goals

**Goals:**
- 实现符合 `ui-wireframe-spec.md` 第 2 节线框的完整书库页面
- 集成 `book_store` 和 `settings_store`，展示真实书籍数据
- 支持触摸滚动浏览书籍列表（利用 `ui_widget_draw_list` 虚拟滚动）
- 支持排序切换（按名称 / 按大小），排序偏好持久化
- 点击书目项为后续 reader-view 页面预留导航接口
- 启动画面完成后自动跳转至书库

**Non-Goals:**
- EPUB 格式支持（Phase 3: epub-support）
- 阅读界面实现（Phase 3: reader-view）
- 全局设置页面（Phase 3: global-settings）
- 书籍封面图片显示（无此需求，用格式标签代替）
- 书籍搜索/过滤功能

## Decisions

### 1. 页面状态管理：static 局部变量

**选择**: 使用文件级 static 变量存储页面状态（书籍列表引用、排序方式、滚动偏移等）。

**理由**: 与 `page_boot.c` 保持一致的实现模式。页面同时只有一个实例，无需动态分配。`on_enter` 初始化状态，`on_exit` 清理。

**备选方案**: 动态分配 page context 结构体 — 过度设计，增加内存管理复杂度。

### 2. 列表渲染：复用 `ui_list_t` Widget

**选择**: 直接使用 `ui_widget_draw_list` + 自定义 `draw_item` 回调渲染每个书目项。

**理由**: `ui_list_t` 已实现虚拟滚动（只渲染可见项）、滚动边界钳位、hit test。书目项绘制逻辑放在回调中，保持页面代码简洁。

**备选方案**: 手动管理可见范围和绘制 — 重复造轮子，容易出 bug。

### 3. 排序切换：Modal Dialog

**选择**: 点击 subheader 排序按钮弹出 `ui_dialog_t` 选择排序方式。

**理由**: 线框规格中排序有多个选项（名称/日期/进度），modal dialog 是自然的选择 UI。已有 `widget_modal_dialog` 组件可直接复用。当前 `book_store` 仅支持 `BOOK_SORT_NAME` 和 `BOOK_SORT_SIZE` 两种排序，dialog 中展示这两个选项。

**备选方案**: 直接点击切换（toggle）— 仅适用于两个选项，不够灵活。

### 4. 阅读进度显示

**选择**: 在书目项中调用 `settings_store_load_progress()` 获取每本书的阅读进度百分比，通过 `ui_progress_t` 绘制进度条。

**理由**: 线框规格要求每本书显示进度条和百分比。进度数据来自 `settings_store`（NVS）。

**注意**: `settings_store_load_progress` 使用文件路径 MD5 作为 NVS key，每次调用有 NVS 读取开销。应在 `on_enter` 或排序变更时批量加载一次并缓存，而非每次渲染时读取。

### 5. 滚动策略：像素级滚动 + 局部刷新

**选择**: 滑动手势 (`SWIPE_UP` / `SWIPE_DOWN`) 触发 `ui_widget_list_scroll()`，按固定步长（一个 item 高度 96px）滚动，标记列表区域为脏区域触发局部刷新。

**理由**: E-Ink 局部刷新（MODE_DU）速度快，逐项滚动避免过多残影累积。`ui_list_t` 的 `scroll_offset` 支持像素级控制。

### 6. 点击书目项的导航

**选择**: 点击书目项时调用 `ui_page_push()` 推入 reader-view 页面，将 `book_info_t` 指针作为参数传递。当前 reader-view 尚未实现，暂时仅打印日志。

**理由**: 预留标准的页面导航接口，reader-view 实现后可无缝对接。使用 push 而非 replace，允许从阅读界面返回书库。

## Risks / Trade-offs

- **[NVS 读取开销]** 64 本书各读一次进度 → 批量缓存到数组中，仅在进入页面和排序变更时刷新
- **[书籍数量为 0]** SD 卡无书或未插入 → 显示空状态提示文字（"未找到书籍，请将 TXT 文件放入 SD 卡"）
- **[长文件名截断]** 中文书名可能超出显示宽度 → 使用 `ui_canvas_draw_text_n` 截断到可用宽度，不做省略号处理（简化实现）
- **[E-Ink 残影]** 多次局部刷新后灰度累积 → 可在后续版本添加定期全刷策略，本次不处理
