## 1. Event 类型定义

- [x] 1.1 创建 `Event.h`：TouchType, TouchEvent, SwipeDirection, SwipeEvent, TimerEvent, EventType, Event 定义
- [x] 1.2 创建 `FlexLayout.h`：FlexDirection, Align, Justify, FlexStyle 类型定义（仅类型，无算法）

## 2. View 头文件

- [x] 2.1 创建 `View.h`：View 类声明，包含树操作、几何属性、脏标记、hitTest、onTouchEvent、FlexBox 成员、RefreshHint

## 3. View 实现 — 树操作

- [x] 3.1 实现构造函数和析构函数
- [x] 3.2 实现 `addSubview`：添加子 View，设置 parent_
- [x] 3.3 实现 `removeFromParent`：从父 View 中移除，返回 unique_ptr
- [x] 3.4 实现 `clearSubviews`：移除所有子 View

## 4. View 实现 — 几何与脏标记

- [x] 4.1 实现 `setFrame` / `frame` / `bounds`
- [x] 4.2 实现 `screenFrame`：递归累加祖先 origin
- [x] 4.3 实现 `setNeedsDisplay` + `propagateDirtyUp`：脏标记冒泡（含短路优化）
- [x] 4.4 实现 `setNeedsLayout`：布局脏标记冒泡
- [x] 4.5 实现 `setHidden`：设置隐藏并触发 setNeedsDisplay
- [x] 4.6 实现 `setBackgroundColor` 和其他属性 setter

## 5. View 实现 — 命中测试与事件

- [x] 5.1 实现 `hitTest`：递归子 View（逆序），返回最深命中的可见 View
- [x] 5.2 实现 `onLayout` / `intrinsicSize` 默认空实现

## 6. 构建配置

- [x] 6.1 更新 `CMakeLists.txt`：添加 View.cpp
- [x] 6.2 更新 `InkUI.h`：添加 Event.h, FlexLayout.h, View.h includes
- [x] 6.3 确认 `idf.py build` 编译通过

## 7. 验证（main.cpp 临时测试）

- [x] 7.1 测试树操作：创建 View 树（根 + 3 个子 View），验证 parent/subviews 关系
- [x] 7.2 测试脏标记冒泡：子 View setNeedsDisplay，检查祖先的 subtreeNeedsDisplay
- [x] 7.3 测试 hitTest：构建嵌套 View，模拟坐标命中测试，验证返回最深层 View
- [x] 7.4 测试 screenFrame：嵌套 View 的绝对坐标计算
