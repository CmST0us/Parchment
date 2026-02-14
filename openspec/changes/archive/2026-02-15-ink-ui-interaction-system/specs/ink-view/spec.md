## MODIFIED Requirements

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
