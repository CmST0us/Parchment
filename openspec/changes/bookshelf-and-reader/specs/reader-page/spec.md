## ADDED Requirements

### Requirement: 分页文本渲染
阅读页面 SHALL 使用 text_paginator 和 text_layout 渲染当前页的 TXT 内容。

#### Scenario: 进入阅读
- **WHEN** 从书架页面进入阅读页面（传入文件路径）
- **THEN** 加载或生成分页表，渲染第一页内容到屏幕

#### Scenario: 使用缓存分页表
- **WHEN** 该书籍存在有效的 `.idx` 缓存
- **THEN** 直接加载缓存分页表，不重新扫描

#### Scenario: 首次打开书籍
- **WHEN** 该书籍无缓存分页表
- **THEN** 执行预扫描生成分页表并缓存

### Requirement: 翻页控制
阅读页面 SHALL 支持通过点击屏幕左右半区翻页。

#### Scenario: 下一页
- **WHEN** TAP 屏幕右半区（x >= 270）且当前不是最后一页
- **THEN** 页码加 1，渲染下一页内容，标记全屏脏区域

#### Scenario: 上一页
- **WHEN** TAP 屏幕左半区（x < 270）且当前不是第一页
- **THEN** 页码减 1，渲染上一页内容，标记全屏脏区域

#### Scenario: 首页左翻
- **WHEN** 当前页为第 0 页且 TAP 左半区
- **THEN** 无响应

#### Scenario: 末页右翻
- **WHEN** 当前页为最后一页且 TAP 右半区
- **THEN** 无响应

### Requirement: 返回书架
阅读页面 SHALL 支持长按返回书架页面。

#### Scenario: 长按返回
- **WHEN** LONG_PRESS 屏幕任意位置
- **THEN** 调用 `ui_page_pop()` 返回书架页面

### Requirement: 页面信息显示
阅读页面 SHALL 在页面底部显示当前页码和总页数。

#### Scenario: 页码显示
- **WHEN** 渲染阅读页面
- **THEN** 页面底部显示 "页码/总页数" 格式的信息（如 "3/156"）
