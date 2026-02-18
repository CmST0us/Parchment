## MODIFIED Requirements

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
