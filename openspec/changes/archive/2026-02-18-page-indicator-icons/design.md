## Context

PageIndicatorView 当前实现使用 `ButtonStyle::Secondary`（白底黑边框 + 中文文字「上一页」「下一页」），与 spec 描述的 Icon 样式不一致。现有图标资源缺少 `arrow-right`，需要补充后才能完成按钮的图标化。

现有基础设施：
- `tools/icons/convert_icons.py` 可从 Tabler Icons SVG 生成 32×32 4bpp 位图数据
- `ButtonView` 已完整支持 `ButtonStyle::Icon` 模式（只渲染图标，禁用态自动变灰）
- `ui_icon_data.h` 已有 `icon_arrow_left_data[]`，缺少 `arrow-right`

## Goals / Non-Goals

**Goals:**
- 新增 `arrow-right` 图标资源，补全箭头图标对
- 将 PageIndicatorView 的翻页按钮从文字改为图标，使用 `UI_ICON_ARROW_LEFT` 和 `UI_ICON_ARROW_RIGHT`
- 保持与现有 HeaderView 等组件的图标按钮视觉风格一致

**Non-Goals:**
- 不修改 ButtonView 的 Icon 模式渲染逻辑（已完善）
- 不重构图标资源的存储方式
- 不修改 pageLabel 的文字显示逻辑

## Decisions

### 1. 使用 `arrow-right` 而非 `chevron-right`

**选择**: 使用完整箭头 (`←` `→`) 而非 chevron (`<` `>`)。

**理由**: 完整箭头在小尺寸下更醒目，且与已有的 `arrow-left`（用于 Header 返回按钮）形成统一的视觉语言。`chevron-right` 保留用于列表项「进入」指示。

### 2. 图标生成走标准 pipeline

**选择**: 下载 `arrow-right.svg` 到 `tools/icons/src/`，运行 `convert_icons.py` 重新生成 `ui_icon_data.h`。

**替代方案**: 手动镜像 `arrow-left` 的位图数据。虽然不需要额外 SVG，但破坏了「所有图标由脚本统一生成」的约定。

### 3. PageIndicatorView 的 `setFont()` 保持不变

**选择**: `setFont()` 仍然同时设置 prev/next 按钮的字体（即使 Icon 模式不需要字体），保持 API 稳定性。

**理由**: 设置无用字体的开销为零，而改变 API 签名可能影响调用方。

## Risks / Trade-offs

- [重新生成 ui_icon_data.h 可能影响其他图标] → 生成脚本是幂等的，相同输入产生相同输出，风险极低
- [Icon 模式按钮没有文字 fallback] → 箭头符号含义直观普遍，不需要文字辅助
