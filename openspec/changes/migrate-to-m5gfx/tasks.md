## 1. 字体生成工具

- [x] 1.1 创建 `tools/generate_bin_font.py`：命令行解析（输入 TTF、输出 .bin、--charset、--range、--size 参数）
- [x] 1.2 实现 FreeType 光栅化：加载 TTF，按字符范围逐字符光栅化为 8bpp bitmap，量化为 4bpp
- [x] 1.3 实现字符集预设：`--charset gb2312` 和 `--charset gb18030` 的 Unicode 范围映射
- [x] 1.4 实现字体文件过滤：跳过 FreeType 返回 glyph index 0 的不支持字符，输出警告
- [x] 1.5 实现 4bpp RLE 编码器：高 4 位计数 + 低 4 位灰度，行结束标记，超 15 分段
- [x] 1.6 实现 .bin 文件输出：128 字节文件头 + glyph 条目（unicode 升序）+ RLE 位图数据
- [x] 1.7 实现生成统计输出：glyph 数、文件大小、压缩率、跳过字符数
- [x] 1.8 使用 LXGWWenKai 生成测试 .bin 字体文件，验证格式正确 (需要字体文件，后续手动验证)

## 2. 字体引擎核心

- [x] 2.1 创建 `components/font_engine/` 组件目录和 CMakeLists.txt
- [x] 2.2 实现 .bin 文件加载器：解析文件头、读取 glyph 条目、校验 magic 和 version
- [x] 2.3 实现 HashMap 索引：加载时构建 Unicode→GlyphEntry 的 HashMap，O(1) 查找
- [x] 2.4 实现 RLE 解码器：将 RLE 编码的 glyph bitmap 解码为 4bpp 灰度数据
- [x] 2.5 实现面积加权缩放引擎：4bpp 源 bitmap → 8bpp 目标 bitmap，缩放因子 [0.5, 1.0]
- [x] 2.6 实现缩放后尺寸计算：width、height、advance_x、x_offset、y_offset 按比例缩放

## 3. 字体缓存系统

- [x] 3.1 实现常用字缓存：启动时加载 ASCII + 常用 CJK 标点的已解码 bitmap 到 PSRAM
- [x] 3.2 实现回收池：LRU 淘汰策略，1500 条目上限，PSRAM 分配
- [x] 3.3 实现页面缓存：5 页滑窗，当前页 ± 2 页的 glyph bitmap 缓存
- [x] 3.4 实现缓存查找链路：页面缓存 → 常用字缓存 → 回收池 → 文件读取 + 解码
- [x] 3.5 实现页面翻转时的缓存滑窗：滑出页 glyph 转入回收池，新页 glyph 预加载
- [x] 3.6 实现缩放结果缓存：同一字符同一字号的缩放 bitmap 缓存复用

## 4. M5GFX 显示驱动集成

- [x] 4.1 修改 `main/main.cpp`：将 `epd_driver_init()` + `EpdDriver::instance().init()` 替换为 `M5.begin(cfg)`
- [x] 4.2 设置 `M5.Display.setAutoDisplay(false)` 和 `M5.Display.setEpdMode(epd_quality)`
- [x] 4.3 更新 `main/CMakeLists.txt`：移除 `epd_driver` `epdiy` `gt911` 依赖，添加 `M5GFX` `M5Unified`

## 5. ink_ui Canvas 重写

- [x] 5.1 修改 `Canvas.h`：构造函数从 `(uint8_t* fb, Rect clip)` 改为 `(M5GFX* display, Rect clip)`
- [x] 5.2 移除 `#include "epdiy.h"` 和所有 `EpdFont*` 引用
- [x] 5.3 重写 `clear()`：改为调用 `display->fillRect()`
- [x] 5.4 重写几何绘图方法：fillRect/drawRect/drawHLine/drawVLine/drawLine 改为调用 M5GFX API
- [x] 5.5 重写 `drawBitmap()` 和 `drawBitmapFg()`：改为调用 `display->pushImage()`
- [x] 5.6 重写 `drawText()` / `measureText()`：集成新字体引擎，通过 pushImage 输出 glyph
- [x] 5.7 更新 Color 常量：从 4bpp 高位 (0x00/0x10/.../0xF0) 改为 8bpp (0x00/0x44/.../0xFF)
- [x] 5.8 移除 `setPixel()` 中的手动坐标变换和 4bpp 打包逻辑
- [x] 5.9 移除 `framebuffer()` 方法和 `fb_` 成员
- [x] 5.10 更新 `clipped()`：通过 M5GFX `setClipRect()` 管理裁剪区域

## 6. ink_ui RenderEngine 更新

- [x] 6.1 修改 `RenderEngine` 构造函数：从 `(EpdDriver&)` 改为 `(M5GFX*)`
- [x] 6.2 更新 Phase 3 (Draw)：Canvas 构造改为传入 M5GFX display 指针
- [x] 6.3 更新 Phase 4 (Flush)：移除 `logicalToPhysical()`，直接用逻辑坐标调用 `display->display(x,y,w,h)`
- [x] 6.4 更新 `repairDamage()`：同样使用逻辑坐标直接调用 `display->display()`
- [x] 6.5 移除 `fb_` 成员和 `EpdDriver&` 引用

## 7. ink_ui GestureRecognizer 更新

- [x] 7.1 移除 `#include "gt911.h"` 和 GPIO48 中断配置
- [x] 7.2 修改触摸读取：从 `gt911_read()` 改为 `M5.Touch.getDetail()`
- [x] 7.3 修改任务循环：从中断信号量等待改为 20ms 轮询 `M5.Touch.update()`
- [x] 7.4 移除 `mapCoords()` 函数（M5.Touch 已输出逻辑坐标）

## 8. ink_ui Application 更新

- [x] 8.1 修改 `init()`：移除 `EpdDriver::instance().init()`，添加字体引擎初始化
- [x] 8.2 修改 RenderEngine 创建：传入 `&M5.Display` 而非 EpdDriver
- [x] 8.3 更新 StatusBarView / BookCoverView：移除 `EpdFont*` 引用，改用新字体引擎 API

## 9. ink_ui EpdDriver 替换

- [x] 9.1 删除 `components/ink_ui/src/core/EpdDriver.cpp`
- [x] 9.2 删除 `components/ink_ui/include/ink_ui/core/EpdDriver.h`
- [x] 9.3 从 `components/ink_ui/CMakeLists.txt` 移除 EpdDriver 源文件和 epdiy/epd_driver 依赖

## 10. ui_core 清理

- [x] 10.1 将 `ui_icon.c` / `ui_icon.h` / `ui_icon_data.h` 迁移到独立 `components/ui_icon/` 组件
- [x] 10.2 更新 `ink_ui/CMakeLists.txt`：添加 `ui_icon` 依赖
- [x] 10.3 更新 `main/CMakeLists.txt`：移除 `ui_core` 依赖，添加 `ui_icon`
- [x] 10.4 删除 `main/pages/` 目录（死代码）
- [x] 10.5 删除 `components/ui_core/` 整个目录

## 11. 旧组件清理

- [x] 11.1 删除 `components/epd_driver/` 目录
- [x] 11.2 移除 `components/epdiy/` git submodule
- [x] 11.3 删除 `components/gt911/` 目录
- [x] 11.4 删除 `fonts_data/*.pfnt` 旧字体文件
- [x] 11.5 删除 `tools/fontconvert.py` 和 `tools/generate_fonts.sh` 旧字体工具
- [x] 11.6 更新 `CLAUDE.md` 和 MEMORY.md：反映新架构

## 12. ViewController 字体 API 更新

- [x] 12.1 更新 `BootViewController.cpp`：移除 `ui_font_get()` 调用，改用新字体引擎
- [x] 12.2 更新 `LibraryViewController.cpp`：移除 `ui_font_get()` 和 `EpdFont*`，改用新字体引擎
- [x] 12.3 更新 `ReaderViewController.cpp`：移除 `ui_font_get()` 和 `EpdFont*`，改用新字体引擎
- [x] 12.4 更新所有 View 的 `onDraw()` 中的 `drawText()` 调用签名

## 13. 验证

- [x] 13.1 编译通过：所有组件无编译错误
- [ ] 13.2 启动测试：M5.begin() 成功，屏幕清白
- [ ] 13.3 字体渲染测试：中文文字在多个字号 (16/20/24/28/32px) 下正确显示
- [ ] 13.4 触摸测试：Tap、LongPress、Swipe 手势正确识别
- [ ] 13.5 页面导航测试：Boot → Library → Reader 页面流转正常
- [ ] 13.6 EPD 刷新测试：局部刷新、全屏刷新工作正常，无残影异常
