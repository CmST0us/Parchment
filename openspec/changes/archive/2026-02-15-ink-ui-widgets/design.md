## Context

InkUI 框架核心层已完成（View 树、Canvas、FlexLayout、RenderEngine、GestureRecognizer、Application）。`views/` 和 `text/` 目录为空，无法组合实际页面。

当前 Canvas 已提供:
- `drawText()` / `measureText()` — 基于 epdiy EpdFont 的文字绘制
- `fillRect()` / `drawRect()` — 几何图形
- `drawBitmap()` / `drawBitmapFg()` — 位图渲染（4bpp alpha）
- 裁剪区域自动管理

View 基类已提供:
- `onDraw(Canvas&)` — 子类覆写绘制
- `onTouchEvent(TouchEvent&)` — 触摸响应（冒泡机制）
- `intrinsicSize()` — FlexBox 固有尺寸
- FlexBox 布局属性（flexGrow_, flexBasis_, alignSelf_）

**约束**: C++17, `-fno-exceptions -fno-rtti`, ESP32-S3 PSRAM, 4bpp 灰度墨水屏。

## Goals / Non-Goals

**Goals:**
- 实现 7 个基础 View 子类，覆盖文字、图标、按钮、标题栏、进度条、分隔线、翻页指示器
- 每个 widget 支持 FlexBox 布局（intrinsicSize + flexGrow）
- 每个 widget 携带合理的 RefreshHint 默认值
- 能用这些 widget 组合出 Boot 页面验证

**Non-Goals:**
- PagedListView、OverlayView 等复合 View（阶段 4）
- FontManager、TextLayout 文字排版引擎（阶段 4）
- 实际的 ViewController 页面（阶段 5）
- 图标资源的 Python 转换脚本（已有方案，不在本 change 范围）
- 多行文字自动换行（TextLabel 只做简单截断，完整排版由 TextContentView 在阶段 4 实现）

## Decisions

### 1. TextLabel 文字对齐方式

**选择**: TextLabel 接受 `Align` 枚举控制水平对齐（Start/Center/End），不支持竖直对齐。

**理由**: 线框规格中所有文字场景只需要水平对齐（标题居中、标签左对齐、数值右对齐）。竖直居中通过 FlexBox alignItems 实现。

### 2. ButtonView 回调机制

**选择**: 使用 `std::function<void()>` 存储 onTap 回调。

**理由**: C++17 标准设施，比 C 风格函数指针+void\*上下文更安全。`std::function` 在 `-fno-exceptions` 下可用（只要不调用空 function，我们检查 `if (onTap_)` 保护）。每个 ButtonView 实例仅持有一个 ~32 bytes 的 function 对象。

**替代方案**: 虚函数覆写 — 需要为每种按钮行为创建子类，太重。C 函数指针 — 无法捕获上下文。

### 3. HeaderView 组合方式

**选择**: HeaderView 继承 View，内部创建 IconView + TextLabel 子 View，用 FlexBox Row 布局。

**理由**: 符合 UIKit 组合模式，leftIcon/rightIcon 可选（不传则不创建）。Header 背景色由自身 View 的 backgroundColor 控制。

### 4. 图标数据格式

**选择**: IconView 接受 `const uint8_t*` 指向 32×32 4bpp 位图数据（512 bytes），使用 Canvas::drawBitmapFg() 以 tintColor 渲染。

**理由**: 与架构文档中的图标方案一致（Tabler Icons 32×32 4bpp）。icon 数据由外部提供（可以是 ROM 常量或 PSRAM 加载），IconView 不管理资源生命周期。

### 5. PageIndicatorView 交互模式

**选择**: PageIndicatorView 包含两个 ButtonView（上页/下页）和一个 TextLabel（"2/5"），通过 `std::function<void(int)>` 回调通知页码变化。

**理由**: 与 PagedListView（阶段 4）解耦 — PageIndicatorView 只负责 UI 展示和点击，具体翻页逻辑由外部控制。

### 6. 字体引用方式

**选择**: TextLabel 和 ButtonView 的字体参数使用 `const EpdFont*`，由调用方提供（通过 `ui_font_get()` 获取）。

**理由**: FontManager 是阶段 4 的内容。当前阶段直接使用已有的 `ui_font.h` C 接口获取字体指针，零额外成本。

## Risks / Trade-offs

**[Risk] std::function 在 -fno-exceptions 下的行为**
→ 调用空 std::function 是 UB（在有异常时抛 bad_function_call）。所有调用点检查 `if (callback_)` 保护。

**[Risk] 图标数据指针生命周期**
→ IconView 持有 raw pointer，不管理所有权。调用方需确保 icon 数据在 IconView 存活期间有效。典型场景下 icon 数据是 ROM 常量（永久有效），无风险。

**[Trade-off] TextLabel 不支持多行自动换行**
→ 简化实现，多行只能通过设置 `maxLines_` 和提供已分行的文本。完整排版引擎（CJK 断行、禁则处理）在 TextLayout（阶段 4）中实现。

**[Trade-off] ButtonView 的文字宽度依赖 font**
→ 如果未设置 font，ButtonView 的 intrinsicSize 无法计算文字宽度。调用方需确保设置 font。
