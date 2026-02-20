## Why

StatusBar 当前仅显示时间，无法反映设备电量状态。用户在阅读时无法得知电池余量，可能导致意外关机丢失阅读进度。M5Stack PaperS3 硬件上已有 BAT_ADC (G3) 通过 22K/22K 分压采集电池电压，但软件层面完全未使用。

## What Changes

- 新增电池 ADC 读取组件，通过 G3 引脚采集电池电压并转换为百分比
- 在 StatusBar 右侧绘制电池图标，根据电量等级显示不同填充状态
- Application 事件循环中定期（~30s）更新电量读数

## Capabilities

### New Capabilities
- `battery-adc`: 电池电压 ADC 采集、滤波、电压→百分比转换
- `battery-status-icon`: StatusBar 右侧的电池电量图标绘制与更新

### Modified Capabilities
- `ink-application`: Application 事件循环增加电池电量定期更新逻辑

## Impact

- 新增 `components/battery/` 组件（battery.h / battery.c）
- 修改 `main/board.h`：增加 `BOARD_BAT_ADC` 引脚定义
- 修改 `StatusBarView`：扩展 `onDraw` 在右侧绘制电池图标
- 修改 `Application`：事件循环增加电池更新调用
- 修改 `main.cpp`：启动时初始化电池 ADC
- 依赖 ESP-IDF ADC oneshot 驱动（`esp_adc/adc_oneshot.h`）
