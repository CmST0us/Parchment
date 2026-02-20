## Context

ReaderViewController 当前的 View 层级：
- `view_` (root, Column)
  - `headerContainer` (24px, 白底) → 包含 `headerLabel_` (TextLabel, 书名)
  - `HeaderView` (48px, 含返回按钮和标题)
  - `ReaderTouchView` (flexGrow=1, 内容区)
  - footer 区域

其中 `headerContainer`/`headerLabel_` 与 `HeaderView` 的标题功能重复。Header 始终可见，占据 72px (24+48) 阅读空间。

InkUI FlexLayout 已支持隐藏 View 自动回收空间（`flexLayout()` 跳过 `isHidden()` 的 View）。

## Goals / Non-Goals

**Goals:**
- 移除冗余的 `headerContainer` 和 `headerLabel_`
- Header 默认隐藏，最大化阅读区域
- 点击屏幕中间 1/3 区域切换 Header 显示/隐藏

**Non-Goals:**
- 不修改 StatusBar 的显示逻辑（保持 `prefersStatusBarHidden()` 行为）
- 不修改 InkUI 框架层代码
- 不添加动画效果

## Decisions

### 1. 使用 `View::setHidden()` 控制 Header 显隐

**选择**: 调用 `headerView_->setHidden(true/false)` 切换。

**理由**: FlexLayout 已在 `flexLayout()` 中跳过隐藏 View，隐藏时 48px 自动回归 `ReaderTouchView`（flexGrow=1）。无需修改框架层。

**替代方案**: 动态 add/remove HeaderView — 复杂度更高，需要管理所有权转移，无实际收益。

### 2. 通过 ReaderTouchView 的中间 1/3 区域回调触发 toggle

**选择**: ReaderTouchView 的 `onTouchEvent` 已有三区域划分（上 1/3 翻上页、下 1/3 翻下页、中间 1/3 空闲）。在中间区域的 tap 事件中调用 `onTapMiddle_` 回调。

**理由**: 复用已有的触摸区域划分逻辑，中间区域目前未使用，改动最小。

### 3. 保存 HeaderView raw pointer 用于 toggle

**选择**: 在 `viewDidLoad` 中 `std::move` HeaderView 到 view 树之前保存 raw pointer 到 `headerView_` 成员变量。

**理由**: 与现有 `contentView_`、`footerLeft_` 等成员的模式一致。

### 4. toggle 后调用 `view_->setNeedsLayout()`

**选择**: Header 显隐切换后调用 `view_->setNeedsLayout()` 触发整棵 View 树重新布局和重绘。

**理由**: 隐藏 Header 后其屏幕区域不会自动清除，需要父 View 重绘来填充空白区域。

## Risks / Trade-offs

- **[屏幕残影]** → 隐藏 Header 后如果仅做局部刷新（GL16），可能残留旧内容。通过 `view_->setNeedsLayout()` 触发完整重绘来缓解。
- **[错误路径变更]** → 移除 `headerLabel_` 后，文件加载失败时的错误提示路径（原通过 `headerLabel_->setText("加载失败")`）需要替代方案。改为使用 `ESP_LOGE` 日志记录即可，用户可通过文件不显示内容感知错误。
