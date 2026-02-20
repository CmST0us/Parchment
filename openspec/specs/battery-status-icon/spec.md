### Requirement: 电池图标绘制
StatusBarView SHALL 在右侧绘制电池图标，使用 canvas 原语（drawRect + fillRect）。图标由以下部分组成：
1. 电池外框矩形
2. 右侧正极凸起小矩形
3. 内部填充矩形，宽度按电量百分比线性缩放

图标 SHALL 垂直居中于 StatusBar（20px 高）。右边距 SHALL 为 12px（与左侧时间文字的左边距对称）。

#### Scenario: 满电显示
- **WHEN** 电量百分比为 100%
- **THEN** 电池图标内部完全填充

#### Scenario: 半电显示
- **WHEN** 电量百分比为 50%
- **THEN** 电池图标内部填充约一半宽度

#### Scenario: 空电显示
- **WHEN** 电量百分比为 0%
- **THEN** 电池图标内部无填充，仅显示空壳

### Requirement: 电池电量更新接口
StatusBarView SHALL 提供 `updateBattery(int percent)` 方法。调用时 SHALL 保存百分比值并调用 `setNeedsDisplay()` 触发重绘。百分比值 SHALL 被 clamp 到 0–100 范围。

#### Scenario: 电量变化触发重绘
- **WHEN** 调用 `updateBattery(75)`
- **THEN** 保存百分比为 75，标记需要重绘

#### Scenario: 电量未变化不触发重绘
- **WHEN** 调用 `updateBattery()` 且百分比与上次相同
- **THEN** 不调用 `setNeedsDisplay()`
