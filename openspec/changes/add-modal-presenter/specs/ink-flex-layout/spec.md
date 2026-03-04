## MODIFIED Requirements

### Requirement: FlexBox 布局算法
`ink::flexLayout(View* container)` 自由函数 SHALL 对 container 的可见子 View 执行 FlexBox 布局，按 `container->flexStyle_` 的 direction、alignItems、justifyContent、gap、padding 计算每个子 View 的 frame。

当 `container->flexStyle_.direction` 为 `FlexDirection::None` 时，flexLayout() SHALL **跳过子节点的定位计算**（不改变子节点的 frame），仅递归调用每个可见子 View 的 `onLayout()` 方法。这允许子节点保留手动设置的 frame，用于自由叠放布局。

当 direction 为 Column 或 Row 时，算法 SHALL 按以下步骤执行：
1. **测量阶段**：遍历可见子 View，区分固定尺寸（flexGrow==0）和弹性尺寸（flexGrow>0）
2. **分配阶段**：可用空间 = 容器主轴尺寸 - padding - 固定尺寸总和 - (N-1)*gap，按 flexGrow 比例分配
3. **定位阶段**：按 cursor 逐个设置子 View 的 frame，递归调用 child.onLayout()

隐藏的子 View（hidden_==true）SHALL 被跳过，不占用空间。

#### Scenario: Column 方向基本布局
- **WHEN** 容器 frame 为 {0,0,540,960}，direction=Column，有 3 个子 View 各 flexBasis=100
- **THEN** 子 View frame 分别为 {0,0,540,100}、{0,100,540,100}、{0,200,540,100}

#### Scenario: Row 方向布局
- **WHEN** 容器 direction=Row，有 2 个子 View 各 flexBasis=200
- **THEN** 子 View 沿水平方向排列

#### Scenario: flexGrow 弹性分配
- **WHEN** 容器高度 960，padding=0，gap=0，子 View A flexBasis=100 flexGrow=0，子 View B flexGrow=1
- **THEN** A 高度为 100，B 高度为 860（960-100）

#### Scenario: FlexDirection::None 跳过定位
- **WHEN** 容器 direction=None，有两个子 View，childA frame={0,0,540,960}，childB frame={50,300,440,240}
- **THEN** flexLayout() 不修改 childA 和 childB 的 frame，仅调用各自的 onLayout()

#### Scenario: FlexDirection::None 隐藏子 View
- **WHEN** 容器 direction=None，childA hidden=false，childB hidden=true
- **THEN** childA 的 onLayout() 被调用，childB 的 onLayout() 不被调用
