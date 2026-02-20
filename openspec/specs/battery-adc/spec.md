### Requirement: 电池 ADC 初始化
`battery_init()` SHALL 初始化 ESP32-S3 ADC1 通道（G3 / ADC1_CH2），配置 12-bit 分辨率和 12dB 衰减。SHALL 尝试创建 curve fitting 校准方案，若不可用则 fallback 到 line fitting。初始化成功 SHALL 返回 `ESP_OK`。

#### Scenario: 成功初始化
- **WHEN** 调用 `battery_init()`，硬件正常
- **THEN** 返回 `ESP_OK`，ADC handle 和校准 handle 已创建

#### Scenario: 校准 fallback
- **WHEN** curve fitting 校准方案不可用（eFuse 无校准数据）
- **THEN** 自动 fallback 到 line fitting 校准方案，仍返回 `ESP_OK`

### Requirement: 电池电量百分比读取
`battery_get_percent()` SHALL 执行以下步骤：
1. 连续采样 ADC 8 次并取均值
2. 通过校准 handle 将 raw 值转换为 mV
3. 将 ADC 电压 ×2 还原为电池实际电压（22K/22K 分压）
4. 通过 LiPo 放电曲线查找表 + 线性插值计算百分比
5. 返回 0–100 的整数百分比值

#### Scenario: 满电读取
- **WHEN** 电池电压为 4.2V（ADC 读到 ~2100mV）
- **THEN** 返回 100

#### Scenario: 低电读取
- **WHEN** 电池电压为 3.5V（ADC 读到 ~1750mV）
- **THEN** 返回 10

#### Scenario: 电压低于下限
- **WHEN** 电池电压 ≤ 3.3V
- **THEN** 返回 0

#### Scenario: 电压高于上限
- **WHEN** 电池电压 ≥ 4.2V
- **THEN** 返回 100
