## Why

书库界面 (Library Screen) 和阅读界面 (Reading View) 需要持久化数据支持：扫描 SD 卡上的 TXT 文件生成书籍列表、保存/恢复阅读进度和用户偏好设置。当前只有底层 SD 卡挂载功能 (`sd_storage`)，缺少上层的书籍管理和设置存储逻辑。

## What Changes

- 新增 `book_store` 组件：扫描 `/sdcard` 目录的 TXT 文件，构建书籍列表（文件名、路径、大小），支持按名称/日期排序
- 新增 `settings_store` 组件：基于 NVS 存储用户偏好（字体、字号、行距、页边距、全刷间隔等）和每本书的阅读进度（当前页/总页/百分比）
- 两个组件均提供纯 C API，不依赖 UI 层

## Capabilities

### New Capabilities
- `book-store`: SD 卡 TXT 文件扫描、书籍列表构建与排序
- `settings-store`: NVS 持久化存储用户阅读偏好和每本书阅读进度

### Modified Capabilities

（无需修改现有 spec）

## Impact

- 新增 `components/book_store/` 组件目录
- 新增 `components/settings_store/` 组件目录
- 依赖 `sd_storage` 组件（已存在）
- 依赖 ESP-IDF `nvs_flash` 组件
- `main/main.c` 需要在初始化流程中加入 NVS init 和 book_store 扫描
