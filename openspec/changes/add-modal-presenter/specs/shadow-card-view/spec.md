## ADDED Requirements

### Requirement: ShadowCardView 基础
`ink::ShadowCardView` SHALL 继承自 `ink::View`，作为带投影阴影的卡片容器。ShadowCardView 的 frame 包含阴影扩散区域和白色卡片区域。

ShadowCardView SHALL 在 `onDraw()` 中依次绘制：
1. 白色背景填充整个 frame（清除底层残留像素）
2. 灰度渐变投影阴影（右偏 + 下偏）
3. 白色卡片矩形覆盖阴影内部区域

子 View SHALL 在卡片内部区域（去除阴影 padding 后的矩形）通过 FlexBox 布局排列。

#### Scenario: ShadowCardView 绘制
- **WHEN** ShadowCardView 被 RenderEngine 调用 onDraw()
- **THEN** 画布上先出现白色填充，然后灰度渐变阴影，最后白色卡片矩形

#### Scenario: 子 View 在卡片内部布局
- **WHEN** ShadowCardView frame 为 {50, 300, 440, 240}，阴影 padding 为 8px
- **THEN** 子 View 的布局区域为 {8, 8, 424, 224}（卡片内部）

### Requirement: 阴影渐变参数
ShadowCardView 的阴影 SHALL 使用以下默认参数：
- **阴影偏移**: 右偏 2px，下偏 2px（模拟左上方光源）
- **阴影扩散**: 8px（从卡片边缘向外扩散的距离）
- **灰度渐变**: 从卡片边缘向外，灰度值从 11 渐变到 15（白色），共 4 级过渡
- **方向差异**: 右侧和底部阴影比左侧和顶部深 1 级灰度

阴影区域占用 ShadowCardView frame 的外围 padding 区域。

#### Scenario: 阴影灰度渐变
- **WHEN** 检查卡片右侧阴影区域的像素灰度值（从卡片边缘向右）
- **THEN** 灰度值依次为约 11, 12, 13, 14, 渐变到 15（白色）

#### Scenario: 光源方向阴影不对称
- **WHEN** 比较卡片右下角和左上角的阴影灰度
- **THEN** 右下角阴影比左上角深约 1 级灰度

### Requirement: ShadowCardView 布局
ShadowCardView SHALL override `onLayout()` 方法。布局逻辑：
1. 计算卡片内部区域 = bounds 去除阴影 padding（各边 shadowSpread 像素）
2. 对子 View 在卡片内部区域执行 FlexBox 布局

ShadowCardView 的 FlexBox 属性（direction, alignItems, gap, padding 等）SHALL 作用于卡片内部区域。

#### Scenario: Column 布局子 View
- **WHEN** ShadowCardView direction=Column，有两个子 View
- **THEN** 子 View 在卡片内部区域（去除阴影 padding 后）按 Column 排列

### Requirement: ShadowCardView intrinsicSize
`intrinsicSize()` SHALL 返回卡片内容的固有尺寸加上阴影 padding。如果子 View 定义了 intrinsicSize，卡片的 intrinsicSize 应在子 View 布局尺寸基础上加上 2 * shadowSpread。

#### Scenario: 查询 intrinsicSize
- **WHEN** ShadowCardView 的内容需要 300x200 区域，shadowSpread=8
- **THEN** `intrinsicSize()` 返回 {316, 216}
