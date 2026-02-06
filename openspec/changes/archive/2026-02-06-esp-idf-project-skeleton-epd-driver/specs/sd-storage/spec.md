## ADDED Requirements

### Requirement: SD 卡挂载

系统 SHALL 通过 SPI 接口挂载 MicroSD 卡到 VFS 文件系统，使应用可以通过标准文件 API 访问 SD 卡。

#### Scenario: SD 卡正常挂载

- **WHEN** SD 卡已插入且系统初始化存储驱动
- **THEN** SD 卡 SHALL 成功挂载到 `/sdcard` 路径
- **AND** SHALL 可以通过 `fopen`/`fread`/`fwrite` 等标准 C 文件操作访问

#### Scenario: SD 卡未插入

- **WHEN** 初始化时 SD 卡未插入
- **THEN** 系统 SHALL 记录警告日志
- **AND** SD 存储功能 SHALL 标记为不可用，不影响其他功能运行

---

### Requirement: 文件读写验证

系统 SHALL 支持在 SD 卡上创建、读取和删除文件，验证基础 I/O 功能正常。

#### Scenario: 写入并读回文件

- **WHEN** 向 SD 卡写入一个测试文件
- **THEN** 读回该文件内容 SHALL 与写入内容完全一致

#### Scenario: 列举目录

- **WHEN** 请求列举 SD 卡根目录
- **THEN** SHALL 返回目录中的文件和子目录列表
