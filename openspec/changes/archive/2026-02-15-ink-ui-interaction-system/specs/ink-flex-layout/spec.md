## ADDED Requirements

### Requirement: FlexBox 布局算法
`ink::flexLayout(View* container)` 自由函数 SHALL 对 container 的可见子 View 执行 FlexBox 布局，按 `container->flexStyle_` 的 direction、alignItems、justifyContent、gap、padding 计算每个子 View 的 frame。

算法 SHALL 按以下步骤执行：
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

### Requirement: gap 间距
FlexBox 算法 SHALL 在相邻可见子 View 之间插入 `flexStyle_.gap` 像素的间距。gap 空间 SHALL 从可分配空间中扣除。

#### Scenario: Column 带 gap
- **WHEN** 容器高度 300，gap=10，3 个子 View 各 flexBasis=80
- **THEN** 子 View 位于 y=0、y=90、y=180，总占用 80+10+80+10+80=260

### Requirement: padding 内边距
FlexBox 算法 SHALL 从容器 bounds 中扣除 `flexStyle_.padding`（top/right/bottom/left），子 View 只能在 padding 内区域排列。

#### Scenario: Column 带 padding
- **WHEN** 容器 frame {0,0,540,960}，padding={20,16,20,16}，direction=Column，alignItems=Stretch
- **THEN** 子 View 的 x 起始于 16，宽度为 540-16-16=508，y 起始于 20

### Requirement: alignItems 交叉轴对齐
FlexBox 算法 SHALL 支持以下 alignItems 值控制子 View 在交叉轴上的定位：
- `Stretch`：子 View 交叉轴尺寸 = 容器可用交叉轴尺寸（默认）
- `Start`：子 View 靠交叉轴起始边
- `Center`：子 View 在交叉轴居中
- `End`：子 View 靠交叉轴结束边

子 View 可通过 `alignSelf_` 覆盖容器的 alignItems。`alignSelf_==Auto` 时沿用容器设置。

#### Scenario: alignItems Center
- **WHEN** 容器 direction=Column，宽度 540，alignItems=Center，子 View intrinsicSize 宽度 200
- **THEN** 子 View x = (540-200)/2 = 170

#### Scenario: alignSelf 覆盖
- **WHEN** 容器 alignItems=Stretch，子 View alignSelf_=End，intrinsicSize 宽度 100
- **THEN** 该子 View x = 540-100 = 440（靠右），其他子 View 仍 Stretch

### Requirement: justifyContent 主轴对齐
FlexBox 算法 SHALL 支持以下 justifyContent 值控制子 View 组在主轴上的分布：
- `Start`：从主轴起始位置排列（默认）
- `Center`：整组居中
- `End`：靠主轴结束位置
- `SpaceBetween`：首尾贴边，中间等距
- `SpaceAround`：每个子 View 两侧等距

justifyContent 仅在固定尺寸子 View 未占满容器时有效。

#### Scenario: justifyContent Center
- **WHEN** 容器高度 960，2 个子 View 各高 100，justifyContent=Center
- **THEN** 第一个子 View y = (960-200)/2 = 380

#### Scenario: justifyContent SpaceBetween
- **WHEN** 容器高度 960，3 个子 View 各高 100，justifyContent=SpaceBetween
- **THEN** 子 View 分别位于 y=0、y=330、y=860

### Requirement: intrinsicSize 查询
当子 View 的 flexBasis_==-1（auto）且 flexGrow==0 时，FlexBox 算法 SHALL 调用 `child.intrinsicSize()` 获取子 View 的固有尺寸。返回值中 -1 表示该维度无固有尺寸。

#### Scenario: 使用 intrinsicSize 作为基础尺寸
- **WHEN** 子 View flexBasis=-1，flexGrow=0，intrinsicSize() 返回 {200, 50}
- **THEN** Column 布局中该子 View 主轴尺寸为 50，交叉轴 intrinsic 宽度 200（当 alignItems!=Stretch 时）

### Requirement: View::onLayout 默认实现
`View::onLayout()` 的默认实现 SHALL 调用 `ink::flexLayout(this)` 执行 FlexBox 布局。子类可 override 此方法实现自定义布局。

#### Scenario: 默认布局行为
- **WHEN** 未 override onLayout() 的 View 有子 View 且 needsLayout_==true
- **THEN** onLayout() 执行 FlexBox 算法对子 View 进行排列
