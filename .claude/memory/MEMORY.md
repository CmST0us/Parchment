# Parchment 项目记忆

所有项目知识已统一迁移到项目根目录的 `CLAUDE.md` 中。

## 工作约定
- **禁止自动构建**: 不要主动执行 `idf.py build`，除非用户明确要求
- **不使用 M5GFX/M5Unified**: 项目已移除这两个依赖，不再计划使用
- clangd 诊断中的 `-mlongcalls`、`-mdisable-hardware-atomics` 等警告是正常的（交叉编译环境噪音）
- **构建环境**: 使用 `get_idf_5.5.1` 获取 ESP-IDF 环境，然后执行 `idf.py build` 构建
- **Simulator 构建**: `simulator/build/` 目录下执行 `make`

## 关键架构模式

### 事件循环渲染问题
- `Application::run()` 中 `queueReceive` 超时 30 秒，脏标记更新后必须 `postEvent` 唤醒主循环才能触发 `renderCycle`
- 后台 task 更新 UI 后需要 `app_.postEvent()` 唤醒事件循环（FreeRTOS 队列线程安全）
- `run()` 入口处有初始 `renderCycle` 确保首屏立即显示

### Library 分页
- 每页条目数由 `itemsPerPage()` 动态计算，基于屏幕高度减去固定 UI 元素高度（155px）
- 条目高度 96px + 分隔线 1px，避免底部截断半截 cell

### Simulator stubs
- `simulator/stubs/` 包含 FreeRTOS API 的 pthread 实现（task、semaphore、queue）
- 新增 FreeRTOS API 时需要同步更新 stub 头文件和实现
