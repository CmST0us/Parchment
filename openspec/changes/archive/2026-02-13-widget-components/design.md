## Context

现有 widget 层（ui_widget.h/c）已实现 header、button、progress、list、separator 五种基础组件，采用纯函数式接口（struct 描述 + draw + hit_test）。设置页面需要 slider、selection group 和 modal dialog 三种新组件。

屏幕尺寸 540×960 portrait，4bpp 灰度，e-ink 局部刷新。触摸通过 GT911 直通坐标。

## Goals / Non-Goals

**Goals:**
- 新增 slider、selection group、modal dialog 三种 widget
- 完全遵循现有纯函数式 API 模式（struct + draw + hit_test/touch）
- 所有新 widget 均添加到现有 ui_widget.h/c 中，不创建新文件

**Non-Goals:**
- 不实现动画效果（e-ink 不适合）
- 不实现键盘/物理按键导航（仅触摸）
- 不实现页面本身（page_reading_settings 等是后续 change）

## Decisions

### Decision 1: 在 ui_widget.h/c 中就地扩展

**选择**: 将 3 种新 widget 直接添加到 ui_widget.h 和 ui_widget.c 中。

**替代方案**: 每种 widget 独立文件（ui_slider.h/c 等）。

**理由**: 现有 5 种 widget 已在单文件中，3 种新增后共 8 种仍可管理。保持一致性，降低 CMakeLists.txt 变更和 include 路径复杂度。

### Decision 2: Slider 使用整数值域

**选择**: Slider 使用 `int min_val, max_val, value` 整数三元组。

**替代方案**: 使用 float 浮点值域。

**理由**: 嵌入式环境避免浮点运算。对于字号（16-40）、行距（10-25 代表 1.0-2.5）、边距（0-48）都可用整数表示。调用方在显示时自行格式化（如 value/10 → "1.5"）。

### Decision 3: Slider touch 返回计算后的值

**选择**: `ui_widget_slider_touch()` 返回触摸对应的 `int` 值（min_val..max_val），未命中返回 `INT_MIN`。

**替代方案**: 返回 0-100 标准化值或 bool + out param。

**理由**: 与 progress_touch 返回 0-100 不同，slider 的值域由调用方定义，直接返回映射后的值最方便。使用 `INT_MIN` 作为哨兵值避免与合法值域冲突。

### Decision 4: Selection group 固定最大选项数

**选择**: `ui_sel_group_t` 固定 `items[4]` 数组，最多 4 个选项。

**替代方案**: 动态分配或指针数组。

**理由**: 设置页面选项数固定（边距 3 选项，字体 2-3 选项），4 足够。栈上固定大小避免堆分配，符合嵌入式实践。

### Decision 5: Modal dialog 使用 list widget 作为内容

**选择**: `ui_dialog_t` 内嵌一个 `ui_list_t` 用于内容区滚动列表。

**替代方案**: 自定义内容绘制回调。

**理由**: 当前唯一需要弹窗的场景是字体选择器（滚动字体列表）。内嵌 list 复用已有虚拟化绘制逻辑。如果未来需要非列表内容，可以再扩展。

## Risks / Trade-offs

- **[Slider 精度]** 窄滑块（如 200px 宽）上细粒度值（0-48）可能难以精确触摸 → 可通过 step 参数量化到离散值（如 step=4）
- **[Dialog 全屏刷新]** 弹窗打开/关闭涉及大面积变化，MODE_DU 可能残影明显 → 打开时用 MODE_GL16 全屏刷新
- **[触摸区扩展]** Slider 和 selection group 需要 y 方向 ±20px 的扩展触摸区以提高易用性 → 与 progress 保持一致
