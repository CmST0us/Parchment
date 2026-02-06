## Context

M5Stack PaperS3 搭载 ESP32-S3R8（8MB PSRAM, 16MB Flash），配备 4.7" E-Ink 屏幕（ED047TC2, 960×540）、GT911 触摸、SD 卡槽。我们使用 ESP-IDF 5.5.1 构建纯 C 项目，不依赖 Arduino 框架。

## Goals / Non-Goals

**Goals:**
- 搭建可编译、可烧录的 ESP-IDF 项目骨架
- 驱动 E-Ink 屏幕完成基础渲染
- 验证触摸和 SD 卡基础功能
- 建立清晰的组件化项目结构，便于后续扩展

**Non-Goals:**
- 不实现 ePub/TXT 解析（后续 change）
- 不实现字体渲染系统（后续 change）
- 不实现 UI 框架或设置界面（后续 change）
- 不实现电源管理和深度睡眠（后续 change）

## Decisions

### Decision 1: 项目结构

采用 ESP-IDF 标准组件化结构：

```
Parchment/
├── CMakeLists.txt              # 顶层 CMake
├── partitions.csv              # 自定义分区表
├── sdkconfig.defaults          # ESP-IDF 默认配置
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                  # 应用入口 app_main()
│   ├── board.h                 # M5PaperS3 板级引脚定义
│   └── board.c                 # 板级初始化
├── components/
│   ├── epd_driver/             # E-Ink 显示驱动封装
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── epd_driver.h
│   │   └── epd_driver.c
│   ├── gt911/                  # 触摸驱动
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── gt911.h
│   │   └── gt911.c
│   └── sd_storage/             # SD 卡存储封装
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── sd_storage.h
│       └── sd_storage.c
└── managed_components/         # ESP-IDF 组件管理器（自动生成）
```

### Decision 2: E-Ink 驱动方案

使用 epdiy 库作为底层驱动。由于 epdiy 对 M5PaperS3 的官方支持尚未合入主线（PR #392 已关闭），我们将：
- 通过 ESP-IDF component manager 引入 epdiy
- 如果组件管理器版本不包含 M5PaperS3 支持，则在 `epd_driver` 组件中自行定义板级配置
- 使用 `ED047TC2` 显示类型（非 TC1，经社区验证）

初始化流程：
```c
epd_init(&epd_board_m5papers3, &ED047TC2, EPD_LUT_64K);
EpdiyHighlevelState hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);
uint8_t *fb = epd_hl_get_framebuffer(&hl);
```

### Decision 3: GT911 触摸驱动

自行实现轻量级 GT911 I2C 驱动，而非引入完整的第三方库。GT911 协议较简单：
- I2C 地址：0x5D（或 0x14，取决于 INT 引脚电平）
- 读取寄存器 0x814E 获取触摸点数
- 读取寄存器 0x814F-0x8156 获取第一个触摸点坐标

### Decision 4: SD 卡接入方式

使用 ESP-IDF 内置的 `sdmmc` 组件，SPI 模式挂载。ESP-IDF 提供完整的 VFS 集成，无需额外依赖。

### Decision 5: 分区表

16MB Flash 分区布局：
| 名称 | 类型 | 大小 | 用途 |
|------|------|------|------|
| nvs | data/nvs | 24KB | 非易失存储 |
| phy_init | data/phy | 4KB | PHY 初始化数据 |
| factory | app | 4MB | 应用程序 |
| storage | data/fat | ~11.5MB | 内部文件存储 |

### Decision 6: PSRAM 配置

ESP32-S3R8 配备 8MB Octal PSRAM，必须在 sdkconfig 中启用：
- `CONFIG_SPIRAM=y`
- `CONFIG_SPIRAM_MODE_OCT=y`
- `CONFIG_SPIRAM_SPEED_80M=y`

帧缓冲区（960×540 ÷ 2 ≈ 253KB，4bpp）分配在 PSRAM 中。
