## ADDED Requirements

### Requirement: View 树操作
`ink::View` SHALL 支持以下树操作：
- `addSubview(unique_ptr<View>)`: 添加子 View，设置子 View 的 parent_ 指针
- `removeFromParent()`: 从父 View 中移除自身，返回 unique_ptr 归还所有权
- `clearSubviews()`: 移除所有子 View
- `parent()`: 返回父 View 的 raw pointer（非拥有）
- `subviews()`: 返回子 View 列表的 const 引用

子 View 的生命周期 SHALL 由父 View 的 `subviews_` vector（`unique_ptr`）管理。

#### Scenario: 添加子 View
- **WHEN** 调用 `parent.addSubview(std::move(child))`
- **THEN** child 出现在 `parent.subviews()` 末尾，`child.parent()` 返回 `&parent`

#### Scenario: 从父 View 移除
- **WHEN** 调用 `child.removeFromParent()`
- **THEN** child 从父 View 的 subviews 中移除，返回的 unique_ptr 持有 child 所有权，`child.parent()` 返回 nullptr

#### Scenario: 清除所有子 View
- **WHEN** 调用 `parent.clearSubviews()`
- **THEN** 所有子 View 被销毁，`parent.subviews()` 为空

### Requirement: View 几何属性
View SHALL 提供以下几何属性：
- `setFrame(Rect)` / `frame()`: View 在父 View 坐标系中的位置和尺寸
- `bounds()`: 返回 `{0, 0, frame_.w, frame_.h}`
- `screenFrame()`: 累加所有祖先 frame 的 origin，返回屏幕绝对坐标

#### Scenario: screenFrame 计算绝对坐标
- **WHEN** View A frame 为 `{10, 20, 500, 900}`，View B 是 A 的子 View，frame 为 `{5, 30, 100, 50}`
- **THEN** `B.screenFrame()` 返回 `{15, 50, 100, 50}`

#### Scenario: 根 View 的 screenFrame
- **WHEN** 根 View（无 parent）frame 为 `{0, 0, 540, 960}`
- **THEN** `screenFrame()` 返回 `{0, 0, 540, 960}`

### Requirement: 脏标记冒泡
`setNeedsDisplay()` SHALL 标记自身 `needsDisplay_=true`，并沿 parent 链向上设置每个祖先的 `subtreeNeedsDisplay_=true`，直到遇到已标记的节点（短路优化）。

`setNeedsLayout()` SHALL 标记自身 `needsLayout_=true`，并冒泡到根。

#### Scenario: setNeedsDisplay 冒泡
- **WHEN** 深度为 3 的子 View 调用 `setNeedsDisplay()`
- **THEN** 该 View 的 `needsDisplay_` 为 true，父 View 和祖父 View 的 `subtreeNeedsDisplay_` 为 true

#### Scenario: 短路优化
- **WHEN** 祖父 View 的 `subtreeNeedsDisplay_` 已为 true，子 View 调用 `setNeedsDisplay()`
- **THEN** 冒泡在祖父 View 处停止，不再向上传播

### Requirement: hitTest 命中测试
`hitTest(x, y)` SHALL 递归搜索子 View（逆序遍历），返回包含该点的最深层可见 View。坐标为父 View 坐标系。隐藏的 View（`hidden_==true`）SHALL 被跳过。

#### Scenario: 命中最深层 View
- **WHEN** 根 View 包含 Panel，Panel 包含 Button，点击坐标在 Button 范围内
- **THEN** `rootView.hitTest(x, y)` 返回 Button 的指针

#### Scenario: 命中后添加的 View（上层优先）
- **WHEN** 两个同级 View A 和 B 重叠，B 后添加，点击重叠区域
- **THEN** `hitTest` 返回 B（后添加的在上层）

#### Scenario: 隐藏 View 被跳过
- **WHEN** View 的 `hidden_` 为 true，点击其 frame 范围
- **THEN** `hitTest` 跳过该 View，继续搜索

### Requirement: 触摸事件冒泡
View SHALL 提供 `onTouchEvent(TouchEvent)` 虚函数。hitTest 找到目标 View 后，调用其 `onTouchEvent()`。返回 true 表示事件被消费；返回 false 则冒泡给 parent，直到某个 View 消费或到达根。

#### Scenario: 事件消费
- **WHEN** Button 的 `onTouchEvent` 返回 true
- **THEN** 事件不再冒泡，处理结束

#### Scenario: 事件冒泡
- **WHEN** TextLabel 的 `onTouchEvent` 返回 false
- **THEN** 事件传递给 TextLabel 的 parent 的 `onTouchEvent()`

### Requirement: View 属性
View SHALL 提供以下可配置属性：
- `backgroundColor_`: uint8_t 灰度值，默认 `Color::Clear`（透明，不绘制背景）
- `hidden_`: bool，默认 false。设置为 true 时 SHALL 调用 `setNeedsDisplay()` 通知重绘
- `opaque_`: bool，默认 true
- `refreshHint_`: RefreshHint 枚举，默认 Auto

当 `backgroundColor_` 为 `Color::Clear` 时，RenderEngine SHALL 跳过该 View 的背景清除操作，使其透明地叠加在父 View 的内容上。

需要不透明背景的 View（如根容器）MUST 显式调用 `setBackgroundColor()` 设置有效灰度值。

#### Scenario: 设置隐藏触发重绘
- **WHEN** 调用 `view.setHidden(true)`
- **THEN** `view.needsDisplay_` 为 true，父 View 链的 `subtreeNeedsDisplay_` 为 true

#### Scenario: 默认透明背景
- **WHEN** 创建一个新的 View 实例，不设置 backgroundColor
- **THEN** `view.backgroundColor()` 返回 `Color::Clear`，RenderEngine 重绘时不清除其区域

#### Scenario: 显式设置背景色
- **WHEN** 调用 `view.setBackgroundColor(Color::White)`
- **THEN** `view.backgroundColor()` 返回 `Color::White`，RenderEngine 重绘时先以白色清除其区域

### Requirement: RefreshHint 枚举
`ink::RefreshHint` SHALL 定义以下值：
- `Fast`: MODE_DU，快速单色刷新
- `Quality`: MODE_GL16，灰度准确不闪
- `Full`: MODE_GC16，闪黑消残影
- `Auto`: 由 RenderEngine 根据内容自动决定

#### Scenario: View 携带刷新提示
- **WHEN** ButtonView 设置 `refreshHint_ = RefreshHint::Fast`
- **THEN** RenderEngine 收集脏区域时可读取该提示选择对应 EPD 刷新模式

### Requirement: FlexBox 布局属性
View SHALL 携带 FlexBox 布局相关成员字段：
- `flexStyle_`: FlexStyle 结构体（direction, alignItems, justifyContent, gap, padding）
- `flexGrow_`: int，默认 0（固定尺寸）
- `flexBasis_`: int，默认 -1（auto）
- `alignSelf_`: Align 枚举，默认 Auto

View SHALL 提供 `onLayout()` 虚函数和 `intrinsicSize()` 虚函数。`onLayout()` 的默认实现 SHALL 调用 `ink::flexLayout(this)` 执行 FlexBox 布局算法。子类可 override 此方法实现自定义布局。`intrinsicSize()` 默认返回 `{-1, -1}`。

#### Scenario: flexGrow 默认不伸展
- **WHEN** View 的 `flexGrow_` 为默认值 0
- **THEN** 该 View 在 FlexBox 布局中不参与剩余空间分配

#### Scenario: 默认 onLayout 执行 FlexBox
- **WHEN** 未 override onLayout() 的 View 被调用 onLayout()
- **THEN** 执行 FlexBox 算法对其子 View 进行排列

### Requirement: View 不可拷贝
View SHALL 删除拷贝构造和拷贝赋值运算符，防止意外拷贝 View 树。

#### Scenario: 编译期阻止拷贝
- **WHEN** 代码尝试 `View copy = someView;`
- **THEN** 编译错误
