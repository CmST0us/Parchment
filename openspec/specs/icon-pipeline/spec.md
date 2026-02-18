## ADDED Requirements

### Requirement: 图标数据结构
系统 SHALL 定义 `ui_icon_t` 结构体，包含以下字段：
- `data`: 指向 4bpp alpha 位图数据的指针 (`const uint8_t *`)
- `w`: 图标宽度（像素）
- `h`: 图标高度（像素）

数据格式 SHALL 为 4bpp，每字节存储 2 像素，高 4 位为左像素，与 framebuffer 格式一致。数据值表示 alpha（不透明度），0x00 为完全透明，0xF0 为完全不透明。

#### Scenario: 结构体定义可用
- **WHEN** 包含 `ui_icon.h`
- **THEN** SHALL 能够声明 `ui_icon_t` 类型变量并访问 `data`、`w`、`h` 字段

### Requirement: 预定义图标常量
系统 SHALL 提供以下预定义图标常量，每个为 32×32 像素的 `ui_icon_t`：
- `UI_ICON_ARROW_LEFT`: 左箭头（返回）
- `UI_ICON_ARROW_RIGHT`: 右箭头（前进）
- `UI_ICON_MENU`: 菜单（三横线）
- `UI_ICON_SETTINGS`: 设置（齿轮）
- `UI_ICON_BOOKMARK`: 书签
- `UI_ICON_LIST`: 列表（目录）
- `UI_ICON_SORT_DESC`: 降序排序
- `UI_ICON_TYPOGRAPHY`: 字体设置
- `UI_ICON_CHEVRON_RIGHT`: 右箭头（进入）
- `UI_ICON_BOOK`: 书本

所有图标数据 SHALL 存储在 Flash `.rodata` 段（使用 `const` 声明）。

#### Scenario: 图标常量可访问
- **WHEN** 包含 `ui_icon.h`
- **THEN** SHALL 能够通过 `UI_ICON_ARROW_LEFT`、`UI_ICON_ARROW_RIGHT` 等常量获取图标数据

#### Scenario: 图标尺寸一致
- **WHEN** 读取任意预定义图标的 `w` 和 `h`
- **THEN** SHALL 均为 32

### Requirement: 图标绘制函数
系统 SHALL 提供 `ui_icon_draw(uint8_t *fb, int x, int y, const ui_icon_t *icon, uint8_t color)` 函数，在 framebuffer 上以指定前景色绘制图标。

该函数 SHALL 调用 `ui_canvas_draw_bitmap_fg` 完成实际绘制。

#### Scenario: 黑底上绘制白色图标
- **WHEN** framebuffer 区域已填充黑色 (0x00)，调用 `ui_icon_draw(fb, 10, 10, &UI_ICON_MENU, UI_COLOR_WHITE)`
- **THEN** 图标不透明区域 SHALL 显示为白色，透明区域 SHALL 保持黑色

#### Scenario: 白底上绘制黑色图标
- **WHEN** framebuffer 区域已填充白色 (0xF0)，调用 `ui_icon_draw(fb, 10, 10, &UI_ICON_SETTINGS, UI_COLOR_BLACK)`
- **THEN** 图标不透明区域 SHALL 显示为黑色，透明区域 SHALL 保持白色

#### Scenario: 半透明抗锯齿
- **WHEN** 图标数据中某像素 alpha 值为 0x80
- **THEN** 该像素 SHALL 显示为前景色与背景色的中间值

#### Scenario: NULL 参数保护
- **WHEN** fb 或 icon 为 NULL
- **THEN** SHALL 静默返回，不发生崩溃

### Requirement: SVG 转换脚本
项目 SHALL 提供 `tools/icons/convert_icons.py` Python 脚本，功能：
1. 读取 `tools/icons/src/` 目录下的 SVG 文件
2. 将每个 SVG 渲染为 32×32 灰度图像
3. 量化为 4bpp (16 级)
4. 输出 `ui_icon_data.h` 文件，包含每个图标的 `const uint8_t` C 数组

生成的变量名 SHALL 遵循 `icon_<name>_data` 格式，其中 `<name>` 从文件名派生（连字符转下划线）。

#### Scenario: 批量转换
- **WHEN** `tools/icons/src/` 下有 `arrow-left.svg` 和 `menu-2.svg`
- **THEN** 运行脚本后 SHALL 生成包含 `icon_arrow_left_data[]` 和 `icon_menu_2_data[]` 的头文件

#### Scenario: 输出格式
- **WHEN** 转换 32×32 图标
- **THEN** 每个数组 SHALL 包含 512 个字节 (32×32÷2)

### Requirement: SVG 源文件
项目 SHALL 在 `tools/icons/src/` 目录下包含从 Tabler Icons (MIT) 获取的以下 SVG 文件：
- `arrow-left.svg`
- `arrow-right.svg`
- `menu-2.svg`
- `settings.svg`
- `bookmark.svg`
- `list.svg`
- `sort-descending.svg`
- `typography.svg`
- `chevron-right.svg`
- `book-2.svg`

#### Scenario: 源文件存在
- **WHEN** 检查 `tools/icons/src/` 目录
- **THEN** SHALL 包含上述 10 个 SVG 文件
