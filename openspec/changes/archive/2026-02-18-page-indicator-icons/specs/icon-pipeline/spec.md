## MODIFIED Requirements

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
