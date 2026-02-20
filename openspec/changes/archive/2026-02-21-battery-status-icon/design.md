## Context

StatusBar 当前仅在左侧绘制时间文字，右侧空白。M5Stack PaperS3 板上 BAT_ADC (G3) 通过 22K/22K 分压网络连接电池电压（含 1µF 滤波电容），ADC 读数为实际电压的 1/2。需要读取 ADC 并在 StatusBar 右侧绘制电量图标。

当前 StatusBarView 采用直接 canvas 绘图方式（非子 View 组合），高度固定 20px。

## Goals / Non-Goals

**Goals:**
- 通过 ESP-IDF ADC oneshot API 读取 G3 电池电压
- 电压→百分比映射（LiPo 放电曲线查找表）
- StatusBar 右侧用 canvas 原语绘制电池图标（无需位图资源）
- 定期更新（~30 秒），复用 Application 事件循环

**Non-Goals:**
- BM8563 RTC 集成
- USB 充电状态检测（USB_DET）
- 充电动画
- 低电量警告弹窗

## Decisions

### 1. ADC 驱动选型：ESP-IDF ADC oneshot API

使用 `esp_adc/adc_oneshot.h` + `esp_adc/adc_cali.h`（ESP-IDF v5.x 推荐方式），而非废弃的 `adc1_get_raw()`。

ADC oneshot 支持校准（curve fitting 或 line fitting），可直接输出 mV，精度更高。

**替代方案**: `adc_continuous` — 不需要，电池读取无需连续采样。

### 2. 电池组件架构：独立 C 组件

新建 `components/battery/`，纯 C 接口，职责单一：
- `battery_init()`: 初始化 ADC oneshot handle + 校准
- `battery_get_percent()`: 读取 ADC → 校准为 mV → ×2 还原电池电压 → 查表得百分比

与 UI 层解耦，方便复用。

**替代方案**: 在 StatusBarView 内部直接调 ADC — 违反关注点分离。

### 3. 电压→百分比映射：分段线性查找表

LiPo 放电曲线非线性，使用 8 段查找表 + 线性插值：

| 电压 (mV) | 百分比 |
|-----------|--------|
| 4200      | 100    |
| 4100      | 85     |
| 4000      | 70     |
| 3900      | 55     |
| 3800      | 40     |
| 3700      | 25     |
| 3500      | 10     |
| 3300      | 0      |

### 4. 图标绘制：canvas 原语直接绘制

在 StatusBarView::onDraw 中用 fillRect + drawRect 绘制电池图标，不使用位图资源。这与时间文字的绘制方式一致，且可直接根据百分比计算填充宽度。

图标尺寸约 20×10px（含正极凸起），垂直居中于 20px StatusBar 中。

**替代方案**: 预渲染 4~5 级位图 — 增加资源管理复杂度，灵活性低。

### 5. 采样策略：多次采样取均值

每次读取连续采样 8 次取均值，降低 ADC 噪声。硬件已有 1µF 滤波电容，软件均值进一步平滑。

### 6. 更新频率：复用事件循环 30 秒超时

Application 事件循环已有 30 秒 queue 超时，每次超时或事件到达时检查是否需要更新电池。实际更新间隔 ≥30 秒即可，无需独立 timer。

## Risks / Trade-offs

- **ADC 精度**: ESP32-S3 ADC 非线性，依赖校准方案（curve fitting 需要 eFuse 有校准数据）→ 使用 `adc_cali_create_scheme_curve_fitting` 并 fallback 到 line fitting
- **首次读取延迟**: ADC 初始化在 `app_main` 中同步完成，不影响 UI 启动 → 电池图标在首次 renderCycle 时即可显示
- **墨水屏刷新开销**: 电池图标区域很小（~20×10px），局部刷新成本极低
