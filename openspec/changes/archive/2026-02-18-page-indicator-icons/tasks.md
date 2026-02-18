## 1. 图标资源

- [x] 1.1 下载 Tabler Icons 的 `arrow-right.svg` 到 `tools/icons/src/`
- [x] 1.2 运行 `convert_icons.py` 重新生成 `ui_icon_data.h`
- [x] 1.3 在 `ui_icon.h` 中添加 `UI_ICON_ARROW_RIGHT` 外部常量声明
- [x] 1.4 在 `ui_icon.c` 中添加 `UI_ICON_ARROW_RIGHT` 常量定义

## 2. PageIndicatorView 改造

- [x] 2.1 修改 `PageIndicatorView.cpp` 构造函数：将 prevButton\_ 从 `ButtonStyle::Secondary` + `setLabel("上一页")` 改为 `ButtonStyle::Icon` + `setIcon(UI_ICON_ARROW_LEFT.data)`
- [x] 2.2 修改 `PageIndicatorView.cpp` 构造函数：将 nextButton\_ 从 `ButtonStyle::Secondary` + `setLabel("下一页")` 改为 `ButtonStyle::Icon` + `setIcon(UI_ICON_ARROW_RIGHT.data)`
- [x] 2.3 在 `PageIndicatorView.cpp` 中添加 `#include "ui_icon.h"` 以访问图标常量

## 3. 验证

- [x] 3.1 构建项目确认编译通过
