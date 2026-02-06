## Why

Parchment 项目旨在基于 M5Stack PaperS3（ESP32-S3R8 + 4.7" E-Ink）构建一个墨水屏电子书阅读器。作为第一步，需要搭建 ESP-IDF 5.5.1 项目骨架并驱动 E-Ink 屏幕，为后续的 ePub/TXT 解析、字体管理和 UI 系统奠定基础。

## What Changes

- 创建完整的 ESP-IDF 5.5.1 项目结构（CMakeLists、分区表、sdkconfig 默认配置）
- 集成 epdiy 库作为 E-Ink 显示驱动组件，配置 M5PaperS3 板级定义（ED047TC2）
- 实现基础屏幕操作：初始化、清屏、帧缓冲区绘制、全刷与局部刷新
- 集成 GT911 电容触摸驱动，支持基本触摸事件读取
- 集成 SD 卡（SPI 模式）驱动，验证文件挂载与读写
- 启用 PSRAM（Octal 模式）以支持大帧缓冲区

## Capabilities

### New Capabilities
- `epd-display`: E-Ink 屏幕初始化与基础渲染（全刷、局部刷新、16 级灰度绘制）
- `touch-input`: GT911 电容触摸输入读取（坐标、触摸事件）
- `sd-storage`: MicroSD 卡挂载与文件读写

### Modified Capabilities
<!-- 无，这是全新项目 -->

## Impact

- `main/` — 应用主入口和板级初始化
- `components/epdiy/` — epdiy 显示驱动组件（git submodule）
- `components/gt911/` — GT911 触摸驱动组件
- `CMakeLists.txt` — 顶层 CMake 构建配置
- `partitions.csv` — 自定义分区表（16MB Flash 布局）
- `sdkconfig.defaults` — ESP-IDF 默认配置（PSRAM、Flash、日志等级）
- 依赖：ESP-IDF 5.5.1、epdiy 库
