## Why

ReaderViewController 的 Header 当前始终显示，占据了宝贵的阅读区域。阅读器应最大化内容展示面积，仅在用户需要导航时才显示 Header。同时 `headerLbl` 和 `headerContainer` 与 `HeaderView` 功能重复，属于冗余 UI 元素。

## What Changes

- 进入 ReaderViewController 时默认隐藏 HeaderView
- 点击屏幕中间 1/3 区域切换 HeaderView 的显示/隐藏
- **BREAKING**: 移除 `headerLbl`（`headerLabel_`）和 `headerContainer`，这两个元素不再使用
- Header 隐藏时其 48px 空间回收给阅读内容区域

## Capabilities

### New Capabilities

- `reader-header-toggle`: 阅读器 Header 的点击切换显示/隐藏行为

### Modified Capabilities

- `reader-viewcontroller`: 移除 headerLbl/headerContainer，Header 默认隐藏

## Impact

- `main/controllers/ReaderViewController.cpp` — 主要修改文件，移除冗余 UI 元素，添加 toggle 逻辑
- `main/controllers/ReaderViewController.h` — 移除 `headerLabel_` 成员，添加 `headerView_` 指针
- InkUI FlexLayout 自动回收隐藏 View 的空间，无需修改框架层
