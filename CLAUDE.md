# Parchment E-Reader

M5Stack PaperS3 (ESP32-S3) 墨水屏阅读器项目。

## 技术栈

- **芯片**: ESP32-S3, 8MB PSRAM, 16MB Flash
- **框架**: ESP-IDF v5.5.1
- **显示**: ED047TC2 E-Ink (960x540), 通过 M5GFX/M5Unified 驱动
- **触摸**: GT911 电容触摸 (通过 M5Unified Touch_Class)
- **存储**: SPI SD 卡, 挂载于 `/sdcard`; LittleFS 字体分区
- **语言**: C++17 (ink_ui), C (font_engine, 基础组件)

## 项目结构

```
main/
  main.cpp                - 应用入口 (NVS + SD + InkUI 启动)
  controllers/            - ViewController 实现 (Boot/Library/Reader)
  views/                  - 自定义 View (BootLogoView, BookCoverView)
components/
  ink_ui/                 - UIKit 风格 UI 框架 (C++17, M5GFX 后端)
    core/                 - View, Canvas, RenderEngine, GestureRecognizer, Application
    views/                - TextLabel, ButtonView, HeaderView, StatusBarView 等
  font_engine/            - 自定义 .bin 字体引擎 (4bpp RLE + 多级 PSRAM 缓存)
  ui_icon/                - 32×32 4bpp 图标资源 (Tabler Icons)
  M5GFX/                  - M5GFX 显示库 (git submodule)
  M5Unified/              - M5Unified 硬件抽象 (git submodule)
  esp_littlefs/           - LittleFS 文件系统 (git submodule)
  sd_storage/             - SD 卡挂载
  settings_store/         - NVS 设置存储
  book_store/             - 书籍索引管理
  text_encoding/          - GBK/UTF-8 编码转换
tools/
  generate_bin_font.py    - .bin 字体文件生成工具
  icons/                  - 图标转换工具
fonts_data/               - 字体文件 (LittleFS 镜像, 需要先生成 .bin)
docs/
  ui-wireframe-spec.md    - UI 线框规格
openspec/                 - OpenSpec 变更管理
```

## 编码规范

- C++17, `-fno-exceptions -fno-rtti`
- 使用 Google 代码风格
- 类和函数声明需要注释
- 文档使用简体中文，技术术语保留英文
- 源文件命名使用英文

## 构建

```bash
# ESP-IDF 环境
export IDF_PATH=/Users/eki/esp/esp-idf-5.5.1
source $IDF_PATH/export.sh

# 生成字体文件 (首次或字体更新时)
python tools/generate_bin_font.py <font.ttf> -o fonts_data/reading_font.bin --charset gb2312 --size 32

# 构建烧录
idf.py build
idf.py flash monitor
```

## 关键架构知识

### 显示驱动 (M5GFX)
- `M5.begin()` 自动初始化硬件 (EPD + 触摸 + 电源)
- `M5.Display.setAutoDisplay(false)` — 手动控制刷新时机
- `M5.Display.setEpdMode(epd_quality)` — 默认高质量灰度刷新
- `display->display(x, y, w, h)` — 局部刷新，逻辑坐标，无需手动变换
- 8bpp 灰度 API (0x00=黑, 0xFF=白), Panel_EPD 内部处理 dithering

### EPD 刷新模式
- `epd_quality`: 高质量灰度 (默认)
- `epd_text`: 文字优化
- `epd_fast`: 快速刷新
- `epd_fastest`: 最快 (二值)

### 显示坐标系
- 逻辑坐标: 540x960 portrait (x: 0-539 左→右, y: 0-959 上→下)
- M5GFX `setRotation()` 自动处理坐标变换，代码中只使用逻辑坐标
- 触摸坐标: M5.Touch 输出逻辑坐标

### InkUI 框架
- View 树: UIKit 风格嵌套, `ink::View` 基类, `unique_ptr` 所有权
- 布局: FlexBox (Column/Row + flexGrow + gap + padding)
- 渲染: 脏区域追踪 + `display->display()` 局部刷新
- Canvas: RAII — 构造设 clipRect, 析构恢复
- 页面: ViewController + NavigationController 页面栈
- 手势: M5.Touch 20ms 轮询, Tap/LongPress/Swipe 状态机
- 颜色: 8bpp 常量 Black(0x00)/Dark(0x44)/Medium(0x88)/Light(0xCC)/White(0xFF)/Clear(0x01 哨兵)
- 命名空间: `ink::`

### 字体系统
- **格式**: 自定义 .bin (128B header + 16B glyph entries + RLE bitmap)
- **基础字号**: 32px, 支持缩放到 16-32px (面积加权算法)
- **缓存**: 常用字 (ASCII + CJK 标点) → 页面缓存 (5 页滑窗) → LRU 回收池 (1500 条目)
- **API**: `font_engine_t*` + `uint8_t fontSize` 替代旧 `EpdFont*`
- **存储**: LittleFS 分区, `fonts_data/reading_font.bin`
- **生成**: `tools/generate_bin_font.py` (FreeType → 4bpp RLE → .bin)
