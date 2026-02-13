## MODIFIED Requirements

### Requirement: 启动完成后跳转到书库
启动画面 SHALL 在初始化完成后自动跳转到书库页面，而非停留在启动画面。

#### Scenario: 正常启动跳转
- **WHEN** 启动画面定时器到期（2 秒）
- **THEN** 页面 SHALL 调用 `ui_page_replace(&page_library, NULL)` 跳转到书库页面

#### Scenario: 页面替换而非入栈
- **WHEN** 跳转到书库页面时
- **THEN** SHALL 使用 `ui_page_replace` 而非 `ui_page_push`，确保启动画面不留在页面栈中
