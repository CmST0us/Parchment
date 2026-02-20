# Parchment E-Reader

M5Stack PaperS3 (ESP32-S3) 墨水屏阅读器项目。

## 技术栈

- **芯片**: ESP32-S3, 8MB PSRAM, 16MB Flash
- **框架**: ESP-IDF v5.5.1
- **显示**: ED047TC2 E-Ink (960x540), 通过 epdiy 库驱动
- **触摸**: GT911 电容触摸 (I2C)
- **存储**: SPI SD 卡挂载于 `/sdcard`; LittleFS `fonts` 分区存字体
- **语言**: C++ (主体), C (驱动层)

## 项目结构

```
main/
  main.cpp            - 应用入口 (C++), 硬件初始化 + InkUI Application 启动
  board.c/h           - M5PaperS3 板级定义（电源保持引脚等）
  controllers/        - InkUI ViewController 实现
    BootViewController     - 启动画面
    LibraryViewController  - 书架页面
    ReaderViewController   - 阅读页面
  views/              - 自定义 View 组件
    BookCoverView          - 书籍封面卡片
    BootLogoView           - 启动 Logo
  pages/              - 旧 ui_core 页面 (C, 遗留代码)
    page_boot, page_library

components/
  ink_ui/             - InkUI C++17 UI 框架 (主力)
    core/   - Application, Canvas, View, ViewController, NavigationController,
              FlexLayout, RenderEngine, GestureRecognizer, EpdDriver, Event, Geometry
    views/  - TextLabel, ButtonView, IconView, HeaderView, StatusBarView,
              PageIndicatorView, ProgressBarView, SeparatorView
  ui_core/            - 旧 C UI 框架 (遗留, 含字体子系统)
    ui_font.c/h       - 字体资源管理 (LittleFS .pfnt 加载)
    ui_font_pfnt.c/h  - .pfnt 格式解析
    ui_canvas, ui_core, ui_event, ui_page, ui_render, ui_touch, ui_text, ui_widget, ui_icon
  epd_driver/         - E-Ink 显示驱动封装（基于 epdiy, 含电源管理）
  epdiy/              - epdiy 库 (CmST0us fork, 含 M5PaperS3 板级)
  gt911/              - GT911 触摸驱动
  book_store/         - 书籍扫描与管理 (SD 卡 /sdcard/book/)
  sd_storage/         - SD 卡 SPI 挂载
  settings_store/     - NVS 设置存储
  text_encoding/      - GBK/GB2312 编码检测与 UTF-8 转换
  esp_littlefs/       - LittleFS 文件系统组件

tools/
  fontconvert.py      - TTF → .pfnt 转换工具
  generate_fonts.sh   - 批量生成 UI + 阅读字体
  generate_gbk_table.py - GBK 编码表生成
  icons/              - 图标资源

docs/
  ui-wireframe-spec.md        - UI 线框规格 (8 个页面)
  ink-ui-architecture.md      - InkUI 架构文档
  ui-implementation-roadmap.md - UI 实现路线图

openspec/             - OpenSpec 变更管理
```

## 编码规范

- C++ 使用 C++17, 编译选项 `-fno-exceptions -fno-rtti`
- 使用 Google C/C++ 代码风格
- 类和函数声明需要注释
- 文档使用简体中文，技术术语（API、REST 等）保留英文
- 源文件命名使用英文

## 构建

```bash
# ESP-IDF 环境
export IDF_PATH=/Users/eki/esp/esp-idf-5.5.1
source $IDF_PATH/export.sh
idf.py build
idf.py flash monitor
```

**注意**: 不要主动执行 `idf.py build`，除非用户明确要求。

## 关键架构知识

### InkUI 框架 (主力 UI)
- 语言: C++17, 命名空间 `ink::`
- View 树: UIKit 风格嵌套, `ink::View` 基类
- 布局: FlexBox (Column/Row + flexGrow + gap + padding)
- 渲染: 脏区域追踪 + 统一 GL16 局部刷新
- 手势: GestureRecognizer 触摸手势识别
- 所有权: unique_ptr 为主, parent 用 raw pointer
- **VC View 生命周期**: iOS 风格两步创建 — `loadView()` 创建 View 树，`viewDidLoad()` 做加载后配置。`view()` 访问器在加载后始终返回有效指针（即使 `view_` 已被 `takeView()` 移走）。`loadView()` 中用 `view_` 赋值，其他场景用 `view()` 访问
- 页面导航: ViewController + NavigationController (页面栈)
- 窗口: Application 管理 Window, 含持久 StatusBar
- 入口: `app_main()` → `ink::Application::init()` + `app.run()` 事件循环

### 旧 ui_core 框架 (遗留)
- C 语言页面栈模型, `ui_page_push()` 推入页面
- 帧缓冲区: 4bpp 灰度 (每字节 2 像素), 位于 PSRAM
- ui_canvas 绘图原语: pixel, line, hline, vline, rect, fill_rect, fill, bitmap
- **字体子系统仍在 ui_core 中**: `ui_font_init()` / `ui_font_get(size_px)`

### 字体系统
- 格式: `.pfnt` 二进制, 存储在 LittleFS `fonts` 分区 (~15MB)
- UI 字体: 16px / 24px (ASCII + GB2312 一级, boot 时常驻 PSRAM)
- 阅读字体: 24px / 32px (GB2312 + 日语, 按需加载, 同时只保留一个)
- API: `ui_font_init()` 挂载 LittleFS + 加载 UI 字体; `ui_font_get(size_px)` 获取字体
- 生成: `tools/fontconvert.py` (TTF → .pfnt) + `tools/generate_fonts.sh` (批量)
- 字体使用 epdiy 的 `EpdFont` 结构

### Flash 分区
| 名称      | 类型   | 偏移       | 大小     |
|-----------|--------|------------|----------|
| nvs       | data   | 0x9000     | 0x6000   |
| phy_init  | data   | 0xF000     | 0x1000   |
| factory   | app    | 0x10000    | 0x100000 |
| fonts     | spiffs | 0x110000   | 0xEF0000 |

### EPD 电源管理
epdiy 的 `epd_fullclear()`、`epd_hl_update_screen()`、`epd_clear()` 等函数**不会自动管理电源**。调用方必须在这些函数前后显式调用 `epd_poweron()` / `epd_poweroff()`。`epd_driver.c` 封装层已处理此逻辑。

### EPD 刷新模式
- 全屏刷新: `MODE_GL16`（不闪黑，灰度准确）
- 局部刷新: `MODE_DU`（不闪黑，单色直接更新，最快）
- `MODE_GC16` 会闪黑，仅在需要彻底消除残影时使用（如 `epd_fullclear`）

### 显示坐标系
- 逻辑坐标: 540x960 portrait (x: 0-539 左→右, y: 0-959 上→下)
- 物理 framebuffer: 960x540 landscape
- 像素映射: `px = ly, py = (539 - lx)` — 转置 + X翻转
- 局部刷新物理坐标: `phys_x = y0, phys_y = 540 - x1`
- 触摸坐标: GT911 直通，无需翻转

### 数据层
- NVS: 存储阅读进度和设置 (settings_store)
- SD 卡: 存储书籍文件, 目录 `/sdcard/book/`, 自动过滤隐藏文件
- 支持格式: TXT (自动检测 GBK/GB2312 并转为 UTF-8)

## 当前应用流程

1. `app_main()`: NVS → Board → GT911 → SD 卡 → 字体 → InkUI 初始化
2. 推入 `BootViewController` 作为首页
3. Boot 页面 → Library 页面 (书架) → Reader 页面 (阅读)
4. StatusBar 持久显示在顶部
