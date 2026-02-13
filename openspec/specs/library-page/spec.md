## ADDED Requirements

### Requirement: 页面布局符合线框规格
书库页面 SHALL 按照 `ui-wireframe-spec.md` 第 2 节定义的布局渲染，包含三个区域：Header（48px）、Subheader（36px）、书目列表（剩余空间 876px）。

#### Scenario: 页面首次渲染
- **WHEN** 书库页面被推入页面栈
- **THEN** 页面 SHALL 显示黑底白字 Header（左侧菜单图标、标题 "Parchment"、右侧设置图标）、LIGHT 底色 Subheader（书籍数量 + 排序方式）、以及书目列表区域

#### Scenario: 屏幕尺寸利用
- **WHEN** 页面渲染完成
- **THEN** 书目列表区域 SHALL 占据 y=84 到 y=960 的空间，每个书目项高度为 96px，最多可见 9 个项目

### Requirement: 书目项显示完整信息
每个书目项 SHALL 显示格式标签、书名、作者名（TXT 文件显示 "Unknown"）和阅读进度条。

#### Scenario: TXT 书籍显示
- **WHEN** SD 卡中存在 TXT 文件
- **THEN** 书目项 SHALL 显示：左侧 60×80 格式标签区域（显示 "TXT"，LIGHT 底色 BLACK 边框）、书名（20px BLACK）、作者 "Unknown"（16px DARK）、进度条（200×8px）及百分比数字

#### Scenario: 有阅读进度的书籍
- **WHEN** 某本书在 `settings_store` 中有阅读进度记录
- **THEN** 进度条 SHALL 按 `byte_offset / total_bytes` 百分比填充，已读部分为 BLACK，未读部分为 LIGHT，右侧显示百分比文字

#### Scenario: 无阅读进度的书籍
- **WHEN** 某本书没有阅读进度记录
- **THEN** 进度条 SHALL 显示 0% 进度

### Requirement: SD 卡书籍扫描与显示
书库页面 SHALL 在进入时调用 `book_store_scan()` 扫描 SD 卡中的 TXT 文件并显示结果。

#### Scenario: SD 卡有书籍
- **WHEN** 用户进入书库页面且 SD 卡中有 TXT 文件
- **THEN** 页面 SHALL 显示所有扫描到的书籍（最多 64 本），Subheader 显示书籍总数

#### Scenario: SD 卡无书籍
- **WHEN** 用户进入书库页面且 SD 卡中无 TXT 文件
- **THEN** 页面 SHALL 在列表区域居中显示提示文字 "未找到书籍"，Subheader 显示 "0 本书"

### Requirement: 列表滚动浏览
书库页面 SHALL 支持触摸滑动滚动浏览书目列表。

#### Scenario: 向上滑动翻页
- **WHEN** 书籍数量超过可见范围（>9 本）且用户向上滑动
- **THEN** 列表 SHALL 向上滚动一个 item 高度（96px），标记列表区域脏区域触发局部刷新

#### Scenario: 向下滑动翻页
- **WHEN** 列表已滚动且用户向下滑动
- **THEN** 列表 SHALL 向下滚动一个 item 高度（96px），标记列表区域脏区域触发局部刷新

#### Scenario: 滚动边界
- **WHEN** 列表已滚动到顶部/底部边界
- **THEN** 列表 SHALL 停止滚动，不产生无效刷新

### Requirement: 排序切换
书库页面 SHALL 支持通过 Subheader 排序按钮切换书籍排序方式。

#### Scenario: 点击排序按钮
- **WHEN** 用户点击 Subheader 的排序区域
- **THEN** 页面 SHALL 弹出 modal dialog 显示排序选项（"按名称排序"、"按大小排序"）

#### Scenario: 选择排序方式
- **WHEN** 用户在排序 dialog 中选择一个排序方式
- **THEN** 书籍列表 SHALL 按选中方式重新排序，dialog 关闭，列表滚动重置到顶部，排序偏好通过 `settings_store_save_sort_order()` 持久化

#### Scenario: 恢复上次排序
- **WHEN** 用户进入书库页面
- **THEN** 页面 SHALL 通过 `settings_store_load_sort_order()` 读取上次排序偏好并应用

### Requirement: 点击书目项导航
用户点击书目项 SHALL 触发向阅读页面的导航。

#### Scenario: 点击书目项
- **WHEN** 用户点击列表中的某个书目项
- **THEN** 页面 SHALL 调用 `ui_page_push()` 推入阅读页面，传递对应的 `book_info_t` 信息

#### Scenario: 阅读页面未实现时的降级
- **WHEN** 阅读页面尚未实现
- **THEN** 页面 SHALL 通过 `ESP_LOGI` 输出选中书籍信息日志，不执行导航

### Requirement: Header 按钮交互
Header 区域的图标按钮 SHALL 响应触摸事件。

#### Scenario: 点击设置图标
- **WHEN** 用户点击 Header 右侧设置图标
- **THEN** 当前版本 SHALL 仅打印日志（设置页面 Phase 3 实现）

#### Scenario: 点击菜单图标
- **WHEN** 用户点击 Header 左侧菜单图标
- **THEN** 当前版本 SHALL 仅打印日志（菜单功能后续实现）
