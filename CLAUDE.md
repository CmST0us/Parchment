# Parchment E-Reader

M5Stack PaperS3 (ESP32-S3) 墨水屏阅读器项目。

## 技术栈

- **芯片**: ESP32-S3, 8MB PSRAM, 16MB Flash
- **框架**: ESP-IDF v5.5.1
- **显示**: ED047TC2 E-Ink (960x540), 通过 epdiy 库驱动
- **触摸**: GT911 电容触摸 (I2C)
- **存储**: SPI SD 卡, 挂载于 `/sdcard`
- **语言**: C

## 项目结构

```
main/
  main.c          - 应用入口，硬件初始化 + ui_core 启动
  board.c/h       - M5PaperS3 板级定义（电源保持引脚等）
components/
  epd_driver/     - E-Ink 显示驱动封装（基于 epdiy）
  epdiy/          - epdiy 库（含 M5PaperS3 板级 epd_board_m5papers3.c）
  gt911/          - GT911 触摸驱动
  sd_storage/     - SD 卡挂载
  ui_core/        - 生产级 UI 框架
    ui_core.c/h       - 主循环
    ui_event.c/h      - FreeRTOS 事件队列
    ui_page.c/h       - 页面栈（生命周期回调）
    ui_canvas.c/h     - 绘图原语（540x960 逻辑坐标，portrait）
    ui_render.c/h     - 脏区域追踪 + 局部刷新
    ui_touch.c/h      - GT911 手势识别状态机
docs/
  ui-wireframe-spec.md - 生产 UI 线框规格（8 个页面）
openspec/              - OpenSpec 变更管理
```

## 编码规范

- 使用 Google C 代码风格
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

## 关键架构知识

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

### UI 框架
- ui_core 使用页面栈模型，通过 `ui_page_push()` 推入页面
- 页面栈为空时事件循环正常运行但不分发事件
- 帧缓冲区: 4bpp 灰度（每字节 2 像素），位于 PSRAM
- ui_canvas 绘图原语: pixel, line, hline, vline, rect, fill_rect, fill, bitmap

### 当前状态
- main.c 有临时触摸验证页面（待清理后接入生产页面）
- `docs/ui-wireframe-spec.md` 定义了 8 个目标页面的线框
