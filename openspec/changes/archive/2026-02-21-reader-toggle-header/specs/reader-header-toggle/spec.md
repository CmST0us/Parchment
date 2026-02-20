## ADDED Requirements

### Requirement: Header 默认隐藏
ReaderViewController 进入阅读页面时 SHALL 默认隐藏 HeaderView（`headerView_->setHidden(true)`）。隐藏时 HeaderView 的 48px 空间 SHALL 由 FlexLayout 自动回收给内容区域。

#### Scenario: 进入阅读页面
- **WHEN** 用户从书库进入 ReaderViewController
- **THEN** HeaderView 不可见，内容区域占据 HeaderView 原有的 48px 空间

### Requirement: 点击中间区域切换 Header
用户 tap 屏幕中间 1/3 区域时 SHALL 切换 HeaderView 的显示/隐藏状态。切换后 SHALL 调用 `view_->setNeedsLayout()` 触发重新布局和重绘。

#### Scenario: 点击中间区域显示 Header
- **WHEN** HeaderView 处于隐藏状态，用户 tap 屏幕纵向中间 1/3 区域
- **THEN** HeaderView 变为可见，内容区域缩小 48px，页面重新布局

#### Scenario: 点击中间区域隐藏 Header
- **WHEN** HeaderView 处于显示状态，用户 tap 屏幕纵向中间 1/3 区域
- **THEN** HeaderView 变为隐藏，内容区域扩展 48px，页面重新布局

#### Scenario: 点击左右区域不影响 Header
- **WHEN** 用户 tap 屏幕左侧 1/3 或右侧 1/3 区域
- **THEN** Header 显隐状态不变，仅执行翻页操作
