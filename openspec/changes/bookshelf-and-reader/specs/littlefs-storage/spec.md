## ADDED Requirements

### Requirement: LittleFS 分区挂载
系统 SHALL 在启动时将 flash `storage` 分区（类型 `littlefs`，8MB）挂载到 VFS 路径 `/littlefs`。

#### Scenario: 正常挂载
- **WHEN** 系统启动且 flash `storage` 分区存在有效 LittleFS 镜像
- **THEN** 分区挂载到 `/littlefs` 路径，可通过标准 POSIX API（`fopen`/`fread` 等）访问文件

#### Scenario: 首次启动或镜像损坏
- **WHEN** LittleFS 分区不含有效文件系统
- **THEN** 系统自动格式化分区后完成挂载

### Requirement: 内置字体文件打包
构建系统 SHALL 将项目 `fonts/` 目录中的字体文件打包为 LittleFS 镜像，随固件烧录到 `storage` 分区。

#### Scenario: 固件烧录包含字体
- **WHEN** 执行 `idf.py flash`
- **THEN** `fonts/` 目录下的所有文件（含文泉驿微米黑 `wqy-microhei.ttc`）被写入 `storage` 分区的 LittleFS 镜像中

#### Scenario: 运行时读取内置字体
- **WHEN** 系统挂载 LittleFS 后访问 `/littlefs/wqy-microhei.ttc`
- **THEN** 返回完整的 TTC 字体文件数据

### Requirement: 分区表配置
`partitions.csv` SHALL 将 `storage` 分区配置为 LittleFS 类型、8MB 大小。

#### Scenario: 分区表定义
- **WHEN** 查看 `partitions.csv` 中 `storage` 行
- **THEN** 类型为 `data`，子类型为 `0x83`（littlefs），大小为 `0x800000`（8MB）

### Requirement: LittleFS 组件依赖
项目 SHALL 通过 ESP-IDF 组件管理器引入 `joltwallet/littlefs` 组件。

#### Scenario: 组件可用
- **WHEN** 执行 `idf.py build`
- **THEN** `esp_vfs_littlefs.h` 头文件可用，编译链接成功
