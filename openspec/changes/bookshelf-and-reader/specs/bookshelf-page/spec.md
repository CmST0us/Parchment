## ADDED Requirements

### Requirement: 扫描书籍目录
书架页面 SHALL 在进入时扫描 `/sdcard/books/` 目录下所有 `.txt` 文件。

#### Scenario: 正常扫描
- **WHEN** 进入书架页面且 SD 卡已挂载
- **THEN** 获取 books 目录下所有 `.txt` 文件的文件名列表

#### Scenario: 目录不存在
- **WHEN** `/sdcard/books/` 目录不存在
- **THEN** 显示空书架（无书籍）

#### Scenario: SD 卡未挂载
- **WHEN** SD 卡未插入或挂载失败
- **THEN** 显示空书架

### Requirement: 书名列表渲染
书架页面 SHALL 以文件名（去除 `.txt` 后缀）作为书名，渲染为垂直列表。

#### Scenario: 正常显示
- **WHEN** 扫描到 3 个 TXT 文件：`红楼梦.txt`、`西游记.txt`、`三体.txt`
- **THEN** 屏幕上显示 3 行文字：`红楼梦`、`西游记`、`三体`

#### Scenario: 书名过长
- **WHEN** 文件名超过屏幕宽度可显示的字符数
- **THEN** 截断显示（尾部截断）

#### Scenario: 书架为空
- **WHEN** 无 TXT 文件
- **THEN** 屏幕显示空状态（可用几何图形表示）

### Requirement: 书架列表分页
书架页面 SHALL 支持在书籍数量超过单屏显示上限时进行翻页。

#### Scenario: 上下翻页
- **WHEN** 书籍数量超过单屏可显示数量
- **THEN** SWIPE_UP 显示下一页书籍，SWIPE_DOWN 显示上一页书籍

#### Scenario: 边界翻页
- **WHEN** 已在书籍列表第一页且 SWIPE_DOWN
- **THEN** 无响应（不循环翻页）

### Requirement: 选书进入阅读
书架页面 SHALL 支持点击书名进入该书的阅读页面。

#### Scenario: 点击书籍
- **WHEN** TAP 某行书名区域
- **THEN** 通过 `ui_page_push()` 进入 reader_page，传递选中的文件路径作为参数

#### Scenario: 点击空白区域
- **WHEN** TAP 书名列表以外的区域
- **THEN** 无响应
