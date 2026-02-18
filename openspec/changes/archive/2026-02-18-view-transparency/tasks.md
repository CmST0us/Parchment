## 1. Canvas 层：新增 Color::Clear 常量

- [x] 1.1 在 `Canvas.h` 的 `Color` 命名空间新增 `constexpr uint8_t Clear = 0x01`

## 2. View 层：默认透明背景

- [x] 2.1 在 `View.h` 将 `backgroundColor_` 默认值从 `0xFF` 改为 `Color::Clear`

## 3. RenderEngine 层：透明背景跳过 clear

- [x] 3.1 修改 `RenderEngine::drawView()`: 当 `backgroundColor != Color::Clear` 时才执行 `canvas.clear(bg)`
- [x] 3.2 修改 `RenderEngine::repairDrawView()`: 同上逻辑

## 4. Canvas 层：修复 drawChar alpha 混合

- [x] 4.1 修改 `Canvas::drawChar()` 将硬编码 `bg4 = 0x0F` 改为 `bg4 = getPixel(absX, absY) >> 4`

## 5. 移除冗余 setBackgroundColor 调用

- [x] 5.1 移除 `HeaderView::HeaderView()` 中的 `setBackgroundColor(Color::White)`
- [x] 5.2 移除 `HeaderView::rebuild()` 中对 button 和 label 的 `setBackgroundColor(Color::White)` 调用
- [x] 5.3 移除 `StatusBarView::StatusBarView()` 中的 `setBackgroundColor(Color::White)`
- [x] 5.4 移除 `Application::buildWindowTree()` 中 `contentArea_` 的 `setBackgroundColor(Color::White)`（保留 `windowRoot_` 的设置）

## 6. 构建验证

- [x] 6.1 执行 `idf.py build` 确认编译通过
