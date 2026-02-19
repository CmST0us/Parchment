## Why

当前项目基于 epdiy 库驱动 ED047TC2 E-Ink 显示屏，需要手动管理 EPD 电源、坐标变换（portrait↔landscape）、帧缓冲区像素操作。M5GFX + M5Unified 作为 M5PaperS3 的官方图形库，提供自动电源管理、自动坐标旋转、丰富的绘图 API 和内置触摸支持。迁移到 M5GFX 可以大幅简化显示和触摸代码，同时获得更好的硬件兼容性保障。

同时，当前字体系统（.pfnt 格式 + epdiy 类型耦合）需要随 epdiy 一起替换。新字体系统采用自定义 .bin 格式（4bpp 灰度 + RLE 压缩），单一 32px 基础字号 + 运行时缩放，多级 PSRAM 缓存，参考 M5ReadPaper 的成熟架构。

## What Changes

- **BREAKING**: 移除 epdiy 库依赖和 `epd_driver` 组件，所有显示操作改用 M5GFX API
- **BREAKING**: 移除 gt911 触摸驱动组件，改用 M5Unified 内置的 `M5.Touch`
- **BREAKING**: 移除整个 `ui_core` 组件（旧 C 框架），`ui_icon` 迁入 `ink_ui`
- **BREAKING**: 废弃 .pfnt 字体格式及相关工具链，替换为自定义 .bin 格式
- 新增自研字体引擎：4bpp 灰度 + RLE 压缩 + HashMap 索引 + 多级 PSRAM 缓存 + 面积加权缩放
- 新增离线字体生成工具：Python + FreeType → .bin
- ink_ui Canvas 重写：从手动 4bpp framebuffer 操作改为 M5GFX `pushImage()` / 绘图 API
- ink_ui RenderEngine flush 简化：去除手动 logical→physical 坐标变换
- ink_ui GestureRecognizer 数据源切换：gt911 → `M5.Touch`
- ink_ui EpdDriver 替换为基于 M5GFX 的显示驱动

## Capabilities

### New Capabilities

- `m5gfx-display-driver`: 基于 M5GFX Panel_EPD 的显示驱动封装，替代 epdiy + epd_driver
- `bin-font-format`: 自定义 .bin 字体文件格式（4bpp 灰度、RLE 压缩、单一 32px 基础字号）
- `font-cache`: 多级 PSRAM 字体缓存系统（5 页滑窗 + 常用字 + 回收池）
- `font-scaler`: 运行时字号缩放引擎（面积加权灰度采样，32px → 任意目标字号）
- `font-generator`: 离线字体生成工具（Python + FreeType → .bin）

### Modified Capabilities

- `ink-canvas`: Canvas 重写为 M5GFX 绘图后端，去除手动 4bpp/坐标变换逻辑
- `ink-render-engine`: flush 阶段改用 M5GFX `display()` API，去除 logicalToPhysical 变换
- `ink-gesture-recognizer`: 数据源从 gt911 改为 `M5.Touch.getDetail()`
- `ink-application`: 初始化流程改为 `M5.begin()`，去除 EpdDriver 依赖

## Impact

### 删除的组件/文件
- `components/epdiy/` (git submodule)
- `components/epd_driver/` (C 封装层)
- `components/gt911/` (触摸驱动)
- `components/ui_core/` (旧 C 框架全部)
- `main/pages/` (死代码)
- `fonts_data/*.pfnt` (旧字体文件)
- `tools/fontconvert.py`, `tools/generate_fonts.sh` (旧字体工具)

### 修改的组件
- `components/ink_ui/`: Canvas、EpdDriver、RenderEngine、GestureRecognizer、StatusBarView、Application
- `main/main.cpp`: 初始化流程
- `main/views/BookCoverView.h`: 字体类型
- `main/controllers/*.cpp`: 字体 API 调用
- `CMakeLists.txt` (多处): 依赖关系更新

### 新增的组件/文件
- `components/font_engine/`: 新字体引擎（.bin 加载、缓存、缩放、渲染）
- `tools/generate_bin_font.py`: 新字体生成工具
