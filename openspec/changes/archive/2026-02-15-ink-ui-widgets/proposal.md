## Why

InkUI 框架的核心层（View 树、Canvas、FlexLayout、RenderEngine、GestureRecognizer、Application）已经实现。但目前没有任何可复用的 UI 组件 — `views/` 和 `text/` 目录完全为空。无法组合出实际的页面界面。

下一步需要实现基础 View 子类（TextLabel、ButtonView、HeaderView 等），这些是线框规格中所有 8 个页面的构建基石。

## What Changes

- 新增 `TextLabel`: 单行/多行文字显示，支持对齐、截断、颜色
- 新增 `IconView`: 32×32 4bpp 图标渲染，支持 tint 着色
- 新增 `ButtonView`: 可点击按钮，支持 icon + text、tap 回调、多种样式
- 新增 `HeaderView`: 组合 View（leftIcon + title + rightIcon），FlexBox Row 布局
- 新增 `ProgressBarView`: 灰度进度条，可配置填充/轨道颜色
- 新增 `SeparatorView`: 水平/垂直分隔线
- 新增 `PageIndicatorView`: 翻页导航指示器（"< 2/5 >"）
- 更新 `CMakeLists.txt` 和 `InkUI.h` 注册新组件
- 更新 `main.cpp` 用基础 View 组合出 Boot 页面验证

## Capabilities

### New Capabilities
- `ink-text-label`: TextLabel 文字显示 View，支持单行/多行、对齐、截断
- `ink-icon-view`: IconView 图标 View，渲染 32×32 4bpp 位图图标
- `ink-button-view`: ButtonView 按钮 View，支持 tap 回调、主/次/图标三种样式
- `ink-header-view`: HeaderView 页面头部栏，组合 leftIcon + title + rightIcon
- `ink-progress-bar`: ProgressBarView 进度条 View，灰度填充
- `ink-separator`: SeparatorView 分隔线 View，水平/垂直
- `ink-page-indicator`: PageIndicatorView 翻页指示器，含上/下页按钮和页码标签

### Modified Capabilities
（无需修改现有 spec — 基础 View 类已提供足够的虚函数接口）

## Impact

- **新增文件**: 7 个 `.h` + 7 个 `.cpp` 在 `components/ink_ui/include/ink_ui/views/` 和 `src/views/`
- **修改文件**: `CMakeLists.txt`（添加源文件）、`InkUI.h`（添加 includes）、`main.cpp`（Boot 页面验证）
- **依赖**: Canvas（文字/图形绘制）、FlexLayout（HeaderView 内部布局）、Event（ButtonView 触摸响应）
- **不影响**: 核心层代码无需修改，ui_core 旧框架保持不动
