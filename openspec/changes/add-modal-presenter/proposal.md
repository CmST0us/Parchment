## Why

InkUI 框架当前只有单一的 `contentArea_` 内容槽位，无法在页面内容之上叠加显示模态视图（如 Loading、Toast、Alert、ActionSheet）。阅读器在加载书籍、操作确认、错误提示等场景中需要模态交互能力，这是 UI 框架的基础缺失。

## What Changes

- 新增 `screenRoot_` 顶层包裹 View，将 `windowRoot_` 和新的 `overlayRoot_` 并列叠放，实现 Z 轴分层
- 给 `FlexDirection` 新增 `None` 值，使 `screenRoot_` 和 `overlayRoot_` 的子节点不参与 flex 排列，而是自由定位重叠
- 新增 `ModalPresenter` 类，独立管理模态视图的呈现、队列调度、事件拦截和关闭修复
- 新增 `ShadowCardView` 通用阴影卡片组件，为模态窗提供投影悬浮效果
- 修改 Application 的事件分发逻辑，支持模态阻塞触摸和 swipe 拦截
- 修改 Application 的 renderCycle 入口从 `windowRoot_` 变更为 `screenRoot_`

## Capabilities

### New Capabilities
- `modal-presenter`: 模态视图呈现与队列管理系统（ModalPresenter 类、通道调度、事件拦截、自动消失）
- `shadow-card-view`: 带投影阴影的卡片容器 View 组件（灰度渐变阴影绘制）

### Modified Capabilities
- `ink-flex-layout`: FlexDirection 新增 None 值，flexLayout() 遇到 None 时跳过子节点定位，仅递归布局
- `ink-application`: 新增 screenRoot_ 包裹层；renderCycle/hitTest 入口变更；事件分发新增模态拦截逻辑；暴露 modalPresenter() 访问器
- `window-root`: Window View 树结构从 windowRoot_ 为根变更为 screenRoot_ 为根，windowRoot_ 成为 screenRoot_ 的子节点

## Impact

- **核心框架改动**: Application.h/.cpp、FlexLayout.h/.cpp
- **新增文件**: ModalPresenter.h/.cpp、ShadowCardView.h/.cpp
- **渲染管线**: renderCycle 根节点变更，repairDamage 调用路径变更
- **事件系统**: Timer 事件新增 ModalPresenter 拦截路径
- **现有 VC 无感知**: windowRoot_ 以下的树结构完全不变，现有 ViewController 不受影响
