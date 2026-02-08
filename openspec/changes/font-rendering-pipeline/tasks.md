## 1. ui_canvas 扩展

- [x] 1.1 在 `ui_canvas.h` 中声明 `ui_canvas_draw_bitmap_1bit()` 函数
- [x] 1.2 在 `ui_canvas.c` 中实现 `ui_canvas_draw_bitmap_1bit()`：1-bit packed bitmap → 4bpp framebuffer 渲染，支持前景色参数和越界裁剪

## 2. 字体核心模块

- [x] 2.1 创建 `components/ui_core/ui_font.h`：定义 glyph 索引结构体、font context 结构体、所有公开 API 声明
- [x] 2.2 在 `ui_font.c` 中实现 UTF-8 decoder：逐字节解析 UTF-8 序列，返回 Unicode codepoint
- [x] 2.3 在 `ui_font.c` 中实现 `ui_font_load()`：打开 .bin 文件、解析 134 字节 header、读取 glyph table、在 PSRAM 中构建 hash map 索引
- [x] 2.4 在 `ui_font.c` 中实现 `ui_font_unload()`：释放 PSRAM 索引内存、关闭文件句柄
- [x] 2.5 在 `ui_font.c` 中实现 glyph 查找：通过 hash map O(1) 查找 codepoint 对应的 glyph 元数据
- [x] 2.6 在 `ui_font.c` 中实现 `ui_font_draw_char()`：查找 glyph → 从 SD 卡读取 bitmap 数据 → 调用 `ui_canvas_draw_bitmap_1bit()` 渲染
- [x] 2.7 在 `ui_font.c` 中实现 `ui_font_draw_text()`：UTF-8 逐字符解码 → 逐字符推进 x 坐标 → 自动换行（CJK 逐字断行 / ASCII 按词断行）
- [x] 2.8 在 `ui_font.c` 中实现 `ui_font_measure_text()`：计算文本像素宽度，不渲染
- [x] 2.9 在 `ui_font.c` 中实现 `ui_font_get_height()`：返回当前字体高度

## 3. Python 字体生成工具

- [x] 3.1 创建 `tools/generate_font_bin.py`：改造 M5ReadPaper 的脚本，保留 TTF→1-bit bitmap 核心流程
- [x] 3.2 实现默认字符集：ASCII (U+0020~U+007E) + CJK 基本区 (U+4E00~U+9FFF) + 中文标点
- [x] 3.3 实现命令行参数：`--size`、`--white`、`--no-smooth`、`--ascii-only`
- [x] 3.4 用选定的中文字体（Songti SC Regular）生成 4 档预设字号的 .bin 文件：24/28/32/36

## 4. 集成与验证

- [x] 4.1 在 `main/main.c` 中移除交互测试页面，替换为字体渲染验证页面
- [x] 4.2 验证页面：加载字体 → 渲染中英文混排测试文本 → 展示 4 档字号效果
- [x] 4.3 将 `components/ui_core/CMakeLists.txt` 更新，添加 `ui_font.c` 到编译列表
- [x] 4.4 编译通过，烧录到设备验证字体渲染效果
