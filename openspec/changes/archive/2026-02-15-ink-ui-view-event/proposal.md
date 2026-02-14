## Why

Canvas 绘图引擎已就绪，下一步需要 View 树和事件系统作为 RenderEngine 局部重绘和触摸交互的基础。View 基类定义了组件的树形结构、脏标记冒泡机制和命中测试；Event 类型定义了触摸、滑动、定时器等事件的数据格式。这两者是整个 InkUI 框架的骨架——没有 View 树就无法做局部重绘，没有 Event 就无法做交互分发。

## What Changes

- 新增 `ink::View` 基类：View 树操作（addSubview/removeFromParent/clearSubviews）、frame/bounds/screenFrame 几何、setNeedsDisplay/setNeedsLayout 脏标记冒泡、hitTest 递归命中测试、onTouchEvent 事件冒泡
- 新增 FlexBox 布局属性（FlexStyle, flexGrow, flexBasis, alignSelf）作为 View 成员，但布局算法本身在下一个 change 实现
- 新增 `ink::Event` 统一事件类型：TouchEvent（Down/Move/Up/Tap/LongPress）、SwipeEvent、TimerEvent、自定义事件
- 新增 `ink::RefreshHint` 枚举（Fast/Quality/Full/Auto），View 携带刷新提示供 RenderEngine 使用
- 在 main.cpp 中用临时测试代码验证 View 树构建、脏标记冒泡和 hitTest

## Capabilities

### New Capabilities
- `ink-view`: View 基类，包含树操作、几何属性、脏标记冒泡、命中测试和事件冒泡
- `ink-event`: 统一事件类型定义，包含触摸、滑动、定时器和自定义事件

### Modified Capabilities
（无）

## Impact

- 新增文件: `View.h`, `View.cpp`, `Event.h`, `FlexLayout.h`（仅类型定义，算法后续实现）
- 修改文件: `CMakeLists.txt`（添加 View.cpp）、`InkUI.h`（添加 includes）
- 修改文件: `main/main.cpp`（临时验证代码）
- 依赖: 仅依赖 Geometry（已有）和 Canvas（已有，View::onDraw 签名引用）
