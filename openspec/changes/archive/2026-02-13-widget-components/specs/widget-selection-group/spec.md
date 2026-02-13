## ADDED Requirements

### Requirement: Selection group 数据结构
系统 SHALL 提供 `ui_sel_group_t` 结构体，包含位置尺寸（x, y, w, h）、选项数组（最多 4 个 `const char *` 标签）、选项数量和当前选中索引。

#### Scenario: 结构体定义
- **WHEN** 包含 `ui_widget.h`
- **THEN** SHALL 可声明 `ui_sel_group_t` 类型变量，包含 `int x, y, w, h`、`const char *items[4]`、`int item_count`、`int selected` 字段

### Requirement: Selection group 绘制
系统 SHALL 提供 `ui_widget_draw_sel_group()` 函数，水平等分绘制多个选项按钮。选中项 SHALL 使用 BLACK 底 WHITE 字，未选中项 SHALL 使用 WHITE 底 BLACK 字 BLACK 边框。

#### Scenario: 正常绘制
- **WHEN** 调用 `ui_widget_draw_sel_group(fb, &group)` 且 group.item_count 为 3
- **THEN** SHALL 绘制 3 个等宽按钮并排排列，selected 索引对应的按钮使用选中样式

#### Scenario: 单选项高亮
- **WHEN** group.selected 为 1
- **THEN** 第 2 个按钮 SHALL 使用 BLACK 底 WHITE 字样式，其余使用 WHITE 底 BLACK 字

### Requirement: Selection group 命中检测
系统 SHALL 提供 `ui_widget_sel_group_hit_test()` 函数，返回触摸命中的选项索引。触摸区域在 y 方向 ±20px 扩展。

#### Scenario: 触摸命中选项
- **WHEN** 在 selection group 区域内触摸
- **THEN** SHALL 返回对应选项的索引（0-based）

#### Scenario: 触摸未命中
- **WHEN** 在 selection group 扩展区域外触摸
- **THEN** SHALL 返回 -1
