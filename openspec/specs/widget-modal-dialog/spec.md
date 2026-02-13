## Requirements

### Requirement: Modal dialog 数据结构
系统 SHALL 提供 `ui_dialog_t` 结构体，包含标题文字、内容区域的 `ui_list_t` 和可见状态标志。

#### Scenario: 结构体定义
- **WHEN** 包含 `ui_widget.h`
- **THEN** SHALL 可声明 `ui_dialog_t` 类型变量，包含 `const char *title`、`ui_list_t list`、`bool visible` 字段

### Requirement: Modal dialog 绘制
系统 SHALL 提供 `ui_widget_draw_dialog()` 函数。当 visible 为 true 时，SHALL 绘制半透明 MEDIUM 色遮罩覆盖全屏，居中绘制 WHITE 底内容面板（含标题栏和列表区域）。

#### Scenario: 可见时绘制
- **WHEN** 调用 `ui_widget_draw_dialog(fb, &dialog)` 且 dialog.visible 为 true
- **THEN** SHALL 绘制全屏遮罩和居中内容面板，标题栏显示 dialog.title，列表区域由内嵌 list 绘制

#### Scenario: 不可见时跳过
- **WHEN** dialog.visible 为 false
- **THEN** SHALL 不绘制任何内容（函数立即返回）

#### Scenario: 内容面板布局
- **WHEN** 弹窗可见
- **THEN** 内容面板 SHALL 水平居中，宽度为屏幕宽度减去左右各 24px 边距，高度自适应列表内容但不超过屏幕高度的 70%

### Requirement: Modal dialog 命中检测
系统 SHALL 提供 `ui_widget_dialog_hit_test()` 函数，区分触摸在内容区内部（返回列表 item index）和遮罩区域（用于关闭弹窗）。

#### Scenario: 触摸列表项
- **WHEN** 触摸点在内容面板的列表区域内
- **THEN** SHALL 返回对应列表项的 index（≥0）

#### Scenario: 触摸遮罩区域
- **WHEN** 触摸点在内容面板外部（遮罩区域）
- **THEN** SHALL 返回 -2（表示应关闭弹窗）

#### Scenario: 触摸标题栏
- **WHEN** 触摸点在内容面板的标题栏区域
- **THEN** SHALL 返回 -1（命中面板但非列表项）
