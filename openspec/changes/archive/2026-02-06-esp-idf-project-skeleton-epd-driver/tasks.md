## 1. 项目骨架

- [x] 1.1 创建顶层 `CMakeLists.txt`，设置最低 IDF 版本要求和项目名称
- [x] 1.2 创建 `partitions.csv` 自定义分区表（16MB Flash 布局）
- [x] 1.3 创建 `sdkconfig.defaults`（PSRAM Octal 模式、Flash 16MB、日志等级等）
- [x] 1.4 创建 `main/CMakeLists.txt` 和 `main/main.c`（空的 `app_main` 入口）
- [x] 1.5 创建 `main/board.h` 和 `main/board.c`（M5PaperS3 引脚定义和板级初始化）

## 2. E-Ink 显示驱动

- [x] 2.1 通过 git submodule 添加 epdiy 到 `components/epdiy/`，并添加 M5PaperS3 板级定义
- [x] 2.2 创建 `components/epd_driver/` 封装层（epd_driver.h/c），提供初始化、清屏、全刷、局部刷新、帧缓冲区操作接口
- [x] 2.3 在 `app_main` 中调用 EPD 初始化并执行清屏验证

## 3. GT911 触摸驱动

- [x] 3.1 创建 `components/gt911/`（gt911.h/c），实现 I2C 初始化和触摸点读取
- [x] 3.2 在 `app_main` 中初始化触摸并打印触摸坐标日志验证

## 4. SD 卡存储

- [x] 4.1 创建 `components/sd_storage/`（sd_storage.h/c），实现 SPI 模式 SD 卡挂载和卸载
- [x] 4.2 在 `app_main` 中挂载 SD 卡，执行写入/读回测试验证

## 5. 集成验证

- [x] 5.1 完善 `app_main` 演示流程：初始化所有组件 → 清屏 → 绘制测试图案 → 显示触摸坐标 → 读取 SD 卡文件列表
- [x] 5.2 确保项目可通过 `idf.py build` 编译，无错误无警告
