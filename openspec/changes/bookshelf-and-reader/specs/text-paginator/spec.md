## ADDED Requirements

### Requirement: 预扫描分页
`text_paginator` SHALL 扫描整个 TXT 文件，生成每页起始字节偏移的分页表。

#### Scenario: 生成分页表
- **WHEN** 对一个 TXT 文件调用 `paginator_build(file_path, layout_params)`
- **THEN** 返回 `page_index_t` 结构，包含 `total_pages` 和 `offsets[]` 数组，其中 `offsets[i]` 为第 i 页在文件中的起始字节偏移

#### Scenario: 空文件
- **WHEN** TXT 文件为空（0 字节）
- **THEN** `total_pages` 为 0

#### Scenario: 单页文件
- **WHEN** TXT 文件内容不足一页
- **THEN** `total_pages` 为 1，`offsets[0]` 为 0

### Requirement: 分页缓存持久化
`text_paginator` SHALL 将分页表缓存到 SD 卡，避免重复扫描。

#### Scenario: 保存缓存
- **WHEN** 分页扫描完成
- **THEN** 将分页表写入 `/sdcard/books/.parchment/<filename>.idx`，自动创建 `.parchment` 目录

#### Scenario: 加载缓存
- **WHEN** 打开书籍且对应 `.idx` 缓存文件存在且有效
- **THEN** 直接加载缓存中的分页表，不重新扫描文件

#### Scenario: 缓存失效检测
- **WHEN** 缓存文件中的字体 hash、字号或排版参数 hash 与当前设置不匹配
- **THEN** 丢弃缓存，重新执行预扫描

### Requirement: 缓存文件格式
缓存文件 SHALL 使用二进制格式，包含 header 和 offsets 数据。

#### Scenario: 文件结构
- **WHEN** 读取 `.idx` 文件
- **THEN** header 包含：magic 标识（4字节）、版本号（2字节）、字体 hash（4字节）、字号（2字节）、排版参数 hash（4字节）、总页数（4字节）；body 为 `uint32_t` 数组，长度等于总页数

#### Scenario: magic 标识校验
- **WHEN** 读取 `.idx` 文件的前 4 字节
- **THEN** 值为 `"PIDX"`（0x50494458），否则视为无效缓存

### Requirement: 按页读取文本
`text_paginator` SHALL 根据分页表定位并读取指定页的文本内容。

#### Scenario: 读取中间页
- **WHEN** 调用 `paginator_read_page(index, page_num, buffer, buf_size)`
- **THEN** 从 `offsets[page_num]` 开始读取到 `offsets[page_num + 1]`（或文件末尾），将文本写入 buffer

#### Scenario: 读取最后一页
- **WHEN** `page_num == total_pages - 1`
- **THEN** 从 `offsets[page_num]` 读取到文件末尾
