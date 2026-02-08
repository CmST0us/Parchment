## Why

Parchment 的 UI 框架和硬件驱动已就绪，但目前仅有交互测试页面，缺少核心的电子书阅读功能。需要实现书架浏览和 TXT 阅读器，使设备成为真正可用的墨水屏阅读器。

## What Changes

- 新增 LittleFS 组件，将 flash `storage` 分区（8MB）作为内置字体存储，预置文泉驿微米黑字体
- 新增字体管理器，基于 stb_truetype 实现 TTF 运行时渲染，支持从 LittleFS 和 SD 卡 `/fonts/` 扫描加载字体，PSRAM 字形缓存
- 新增文本排版引擎，实现 UTF-8 解析、中英文自动换行、行距/边距控制、渲染文本到 framebuffer
- 新增文本分页器，预扫描 TXT 文件生成分页偏移表，`.idx` 缓存持久化到 SD 卡
- 新增书架页面，扫描 `/sdcard/books/*.txt` 文件列表，以文件名显示书名，点击进入阅读
- 新增阅读页面，分页渲染 TXT 内容，左半屏 TAP 上一页、右半屏 TAP 下一页、LONG_PRESS 返回书架
- 修改 flash 分区表，将 `storage` 分区从 FAT 改为 LittleFS（8MB）
- 修改 `main.c`，将入口页面从交互测试切换为书架页面

## Capabilities

### New Capabilities
- `littlefs-storage`: LittleFS 分区挂载、内置字体文件打包、标准 POSIX 文件读取
- `font-manager`: TTF 字体加载（stb_truetype）、多源字体扫描（LittleFS + SD）、PSRAM 字形缓存
- `text-layout`: UTF-8 文本排版、中英文自动换行、行距/边距参数、文本块渲染到 framebuffer
- `text-paginator`: TXT 文件预扫描分页、页偏移表生成、`.idx` 缓存文件读写
- `bookshelf-page`: SD 卡 books 目录扫描、书名列表渲染、点击选书导航
- `reader-page`: 分页 TXT 渲染、左右翻页、长按返回书架

### Modified Capabilities
（无现有 spec 需要修改）

## Impact

- **Flash 分区**: `partitions.csv` 中 `storage` 分区类型从 FAT 改为 LittleFS，大小调整为 8MB
- **新增依赖**: `joltwallet/littlefs` ESP-IDF 组件、`stb_truetype.h` 第三方头文件
- **PSRAM 使用增加**: 字形缓存占用额外 PSRAM（预估 200~500KB，取决于缓存大小配置）
- **SD 卡目录结构**: 约定 `/sdcard/books/` 存放 TXT、`/sdcard/books/.parchment/` 存放分页缓存、`/sdcard/fonts/` 存放用户字体
- **构建系统**: 需配置 LittleFS 镜像打包（含字体文件）随固件烧录
- **入口页面**: `main.c` 中 `test_page` 替换为 `bookshelf_page`
