## MODIFIED Requirements

### Requirement: ReaderViewController 页面布局
ReaderViewController SHALL 显示以下元素：
1. 顶部 HeaderView：返回按钮 + 书名（默认隐藏，通过 tap 中间区域切换）
2. ReaderTouchView 容器（处理三分区触摸），内嵌 ReaderContentView（文本渲染）
3. 底部页脚：书名 左侧，"currentPage/totalPages percent%" 右侧，小字体 DARK

页脚的页码和百分比 SHALL 从 `ReaderContentView` 的 `totalPages()` 和 `currentPage()` 获取。

**变更说明**: 移除了"顶部文件名标示"（即 `headerContainer` + `headerLabel_`），HeaderView 改为默认隐藏。

#### Scenario: 阅读页面布局
- **WHEN** 文本加载完成
- **THEN** 屏幕显示文本内容（ReaderContentView 渲染）和底部页码信息，HeaderView 默认隐藏

## REMOVED Requirements

### Requirement: 顶部文件名标示
**Reason**: `headerContainer` 和 `headerLabel_` 与 HeaderView 的标题功能重复，属于冗余 UI 元素
**Migration**: 书名信息已在 HeaderView title 和底部页脚中显示，无需替代方案
