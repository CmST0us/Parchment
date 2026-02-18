## Why

当前渲染管线中，`RenderEngine::drawView()` 对每个 View 无条件执行 `canvas.clear(backgroundColor)`，而 View 基类的默认背景色为白色 (0xFF)。这导致两个问题：

1. **背景色泄漏**：TextLabel、IconView 等叶子视图在非白色容器中会产生可见的白色矩形边界
2. **文字反走样错误**：`Canvas::drawChar()` 硬编码白色背景进行 alpha 混合，在非白色背景上文字边缘出现白色光晕

## What Changes

- 新增 `Color::Clear` 哨兵值 (0x01)，表示"透明，不绘制背景"
- View 基类 `backgroundColor_` 默认值改为 `Color::Clear`（所有 View 默认透明）
- `RenderEngine::drawView()` 和 `repairDrawView()` 在 `backgroundColor == Color::Clear` 时跳过 `canvas.clear()`
- `Canvas::drawChar()` 改为读取 framebuffer 实际像素值进行 alpha 混合（与 `drawBitmapFg` 行为一致）
- 移除所有 View 子类中冗余的 `setBackgroundColor(Color::White)` 调用
- 仅 `Application::buildWindowTree()` 中 `windowRoot_` 保留 `setBackgroundColor(Color::White)` 作为全屏底色

## Capabilities

### New Capabilities

_无新增 capability。_

### Modified Capabilities

- `ink-view`: View 基类默认 backgroundColor 从白色改为 Clear（透明）
- `ink-render-engine`: drawView/repairDrawView 支持透明背景（跳过 clear）
- `ink-canvas`: drawChar alpha 混合改为读取实际背景像素；新增 Color::Clear 常量
- `ink-header-view`: 移除构造函数和 rebuild 中的显式背景色设置
- `ink-text-label`: 无代码改动（继承 View 默认 Clear）
- `ink-icon-view`: 无代码改动（继承 View 默认 Clear）

## Impact

- **components/ink_ui/include/ink_ui/core/Canvas.h**: 新增 `Color::Clear` 常量
- **components/ink_ui/include/ink_ui/core/View.h**: 默认 `backgroundColor_` 改为 `Color::Clear`
- **components/ink_ui/src/core/RenderEngine.cpp**: drawView / repairDrawView 逻辑分支
- **components/ink_ui/src/core/Canvas.cpp**: drawChar 反走样修复
- **components/ink_ui/src/views/HeaderView.cpp**: 移除 setBackgroundColor 调用
- **components/ink_ui/src/views/StatusBarView.cpp**: 移除 setBackgroundColor 调用
- **components/ink_ui/src/core/Application.cpp**: 移除 contentArea_ 的 setBackgroundColor，保留 windowRoot_
- **向后兼容**: 已显式设置 backgroundColor 的外部代码不受影响；未设置的 View 行为从"白色背景"变为"透明" (**BREAKING**)
