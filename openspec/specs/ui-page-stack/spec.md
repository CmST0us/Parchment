## ADDED Requirements

### Requirement: 页面接口定义
系统 SHALL 定义页面结构体 `ui_page_t`，包含以下回调和字段：
- `name` — 页面名称（`const char *`）
- `on_enter(void *arg)` — 进入页面时调用
- `on_exit(void)` — 离开页面时调用
- `on_event(ui_event_t *event)` — 处理事件
- `on_render(uint8_t *fb)` — 渲染到 framebuffer

所有回调字段 SHALL 允许为 NULL（表示该回调无操作）。

#### Scenario: 页面结构体声明
- **WHEN** 定义一个 `ui_page_t` 变量并设置 name 和 on_render 回调
- **THEN** 编译 SHALL 成功

#### Scenario: 回调为 NULL
- **WHEN** 页面的 on_exit 回调为 NULL 且该页面被 pop
- **THEN** 系统 SHALL 安全跳过 on_exit 调用，不发生崩溃

### Requirement: 页面栈初始化
系统 SHALL 提供 `ui_page_stack_init()` 函数，初始化最大深度为 8 的页面栈。

#### Scenario: 初始化后栈为空
- **WHEN** 调用 `ui_page_stack_init()`
- **THEN** `ui_page_current()` SHALL 返回 NULL

### Requirement: 页面入栈
系统 SHALL 提供 `ui_page_push(ui_page_t *page, void *arg)` 函数，将页面压入栈顶。

#### Scenario: push 调用 on_enter
- **WHEN** 调用 `ui_page_push(&my_page, arg)`
- **THEN** `my_page.on_enter` SHALL 被调用，参数为 `arg`

#### Scenario: push 调用前一页面的 on_exit
- **WHEN** 栈中已有 page_a，调用 `ui_page_push(&page_b, NULL)`
- **THEN** `page_a.on_exit` SHALL 在 `page_b.on_enter` 之前被调用

#### Scenario: 栈满时 push
- **WHEN** 页面栈已有 8 个页面，再次调用 `ui_page_push()`
- **THEN** SHALL 不执行入栈操作并记录错误日志

### Requirement: 页面出栈
系统 SHALL 提供 `ui_page_pop(void)` 函数，弹出栈顶页面。

#### Scenario: pop 回到前一页面
- **WHEN** 栈中有 page_a 和 page_b（page_b 在顶），调用 `ui_page_pop()`
- **THEN** `page_b.on_exit` SHALL 被调用，然后 `page_a.on_enter` SHALL 以 NULL 参数被调用

#### Scenario: 栈只有一个页面时 pop
- **WHEN** 栈中只有一个页面，调用 `ui_page_pop()`
- **THEN** SHALL 不执行出栈操作并记录警告日志

### Requirement: 页面替换
系统 SHALL 提供 `ui_page_replace(ui_page_t *page, void *arg)` 函数，替换栈顶页面。

#### Scenario: replace 等效 pop + push
- **WHEN** 栈顶为 page_a，调用 `ui_page_replace(&page_b, arg)`
- **THEN** `page_a.on_exit` SHALL 被调用，然后 `page_b.on_enter` SHALL 被调用

### Requirement: 获取当前页面
系统 SHALL 提供 `ui_page_current(void)` 函数，返回栈顶页面指针。

#### Scenario: 栈非空时返回栈顶
- **WHEN** 栈中有页面
- **THEN** `ui_page_current()` SHALL 返回最后 push 或 replace 的页面指针

#### Scenario: 栈空时返回 NULL
- **WHEN** 栈为空
- **THEN** `ui_page_current()` SHALL 返回 NULL
