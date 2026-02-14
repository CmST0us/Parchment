## 1. Canvas 头文件与构建配置

- [x] 1.1 创建 `Canvas.h`：Canvas 类声明，包含构造函数、clipped()、所有绘图方法签名、private 成员
- [x] 1.2 更新 `CMakeLists.txt`：添加 Canvas.cpp 到源文件列表，确保 epdiy 依赖可用（miniz）
- [x] 1.3 更新 `InkUI.h`：添加 `#include "ink_ui/core/Canvas.h"`

## 2. 核心像素操作与裁剪

- [x] 2.1 实现 `setPixel` / `getPixel` private 方法：逻辑坐标到物理 framebuffer 的 4bpp 变换（从 ui_canvas.c 迁移）
- [x] 2.2 实现构造函数 `Canvas(uint8_t* fb, Rect clip)` 和 `clipped(const Rect& subRect)`
- [x] 2.3 实现 `clear(uint8_t gray)` 和 `framebuffer()` 访问器

## 3. 几何图形绘制

- [x] 3.1 实现 `fillRect`：局部坐标 → 绝对坐标 → clip 裁剪 → setPixel
- [x] 3.2 实现 `drawRect`：边框绘制（调用 fillRect 画四条边）
- [x] 3.3 实现 `drawHLine` / `drawVLine`：委托给 fillRect
- [x] 3.4 实现 `drawLine`：Bresenham 算法 + clip 裁剪（从 ui_canvas.c 迁移）

## 4. 位图绘制

- [x] 4.1 实现 `drawBitmap`：4bpp 灰度位图绘制 + clip 裁剪
- [x] 4.2 实现 `drawBitmapFg`：4bpp alpha 位图前景色混合 + clip 裁剪

## 5. 文字渲染

- [x] 5.1 迁移 UTF-8 解码函数（`utf8_byte_len`, `next_codepoint`）到 Canvas.cpp 内部
- [x] 5.2 迁移 glyph 解压函数（`decompress_glyph`）到 Canvas.cpp 内部
- [x] 5.3 实现 `drawText`：单行文字渲染，y 为基线，支持 clip 裁剪
- [x] 5.4 实现 `drawTextN`：限制最大字节数的文字渲染
- [x] 5.5 实现 `measureText`：文字宽度度量（不写入 framebuffer）

## 6. 构建验证

- [x] 6.1 确认 `idf.py build` 编译通过，无 warning

## 7. 屏幕验证（main.cpp 临时测试）

- [x] 7.1 测试几何图形：创建全屏 Canvas，画 fillRect + drawRect + hline，updateScreen 确认显示
- [x] 7.2 测试裁剪：创建带 clip 的 Canvas，画超出边界的图形，确认裁剪正确
- [x] 7.3 测试文字：用 `ui_font_get()` 获取字体，Canvas::drawText 绘制中英文，确认渲染正确
