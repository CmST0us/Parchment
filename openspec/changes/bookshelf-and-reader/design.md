## Context

Parchment 已具备 ui_core 框架（事件系统、页面栈、Canvas 绘图、脏区域刷新、触摸手势识别）和硬件驱动（EPD、GT911 触摸、SD 卡）。当前入口是交互测试页面，需要替换为书架 + 阅读器的核心功能。

硬件约束：
- ESP32-S3R8，8MB PSRAM，16MB Flash
- 4.7" 墨水屏 960×540（逻辑竖屏 540×960），4bpp 灰度
- SD 卡通过 SPI 外挂，已有 FAT 文件系统挂载
- Flash 有 `storage` 分区可用（将调整为 8MB LittleFS）

## Goals / Non-Goals

**Goals:**
- 从 LittleFS 和 SD 卡加载 TTF 字体并渲染中英文文本
- 实现书架页面：扫描 SD 卡 books 目录，列表显示 TXT 文件
- 实现阅读页面：分页渲染 TXT，左右翻页，长按返回
- 预扫描分页 + 缓存，支持未来书签/进度跳转扩展
- 字形缓存到 PSRAM，避免重复光栅化

**Non-Goals:**
- 不实现 EPUB/PDF 等复杂格式解析
- 不实现字体选择 UI（后续迭代）
- 不实现导航工具条（后续迭代）
- 不实现阅读进度持久化（后续迭代，分页表架构已预留支持）
- 不实现竖排排版

## Decisions

### D1: 字体渲染引擎选择 stb_truetype

**选择**: stb_truetype（单头文件 TTF 光栅化库）

**替代方案**:
- FreeType（ESP-IDF 官方组件）：功能完整但内存预分配大，ESP32-S3 上多人报告问题
- epdiy 内置字体系统：仅支持编译时位图字体，CJK 字符集太大无法编译打包

**理由**: stb_truetype 按需分配内存，已在 ESP32 上被多个项目验证，代码体积小（单 .h 文件），配合 PSRAM 字形缓存可满足 CJK 渲染需求。

### D2: 字体存储方案 LittleFS + SD 卡双源

**选择**: Flash `storage` 分区 8MB LittleFS 存放内置字体 + SD 卡 `/fonts/` 存放用户字体

**替代方案**:
- 仅 SD 卡：无 SD 卡时设备完全不可用
- 仅 LittleFS：无法扩展，8MB 空间有限

**理由**: LittleFS 内置文泉驿微米黑（4.9MB）保证基本可用性，SD 卡字体目录提供扩展性。

### D3: 内置字体选择文泉驿微米黑

**选择**: WenQuanYi Micro Hei（文泉驿微米黑，4.9MB TTC）

**理由**: 在所有开源中文字体中体积最小（4.9MB 包含 Regular + Mono 两个字面），GBK 覆盖完整，Apache 2.0 许可证。8MB 分区放下绰绰有余，还剩 ~3MB 备用。

### D4: 分页策略选择预扫描分页表

**选择**: 打开书籍时预扫描全文，生成 `page_offsets[]` 数组，持久化为 `.idx` 缓存文件

**替代方案**:
- 流式分页：内存极小但反向翻页困难，不支持随机跳转
- 块缓存按需分页：实现复杂，书签/进度需要额外映射

**理由**:
- 书签 = 页码数字，直接 `offsets[page]` 跳转
- 进度 = `current_page / total_pages`，精确百分比
- 翻页零延迟：fseek 到 offset 即可
- 首次扫描代价可接受（500KB TXT < 1秒，5MB < 5秒）
- 缓存持久化后复用，字体/字号变化时重新生成

**缓存文件格式**:
```
路径: /sdcard/books/.parchment/<filename>.idx
内容: 二进制，header + offsets 数组
header: magic(4B) + version(2B) + font_hash(4B) + font_size(2B) +
        layout_params_hash(4B) + total_pages(4B)
body:   uint32_t offsets[total_pages]
```
字体或排版参数变化时通过 hash 比对检测缓存失效。

### D5: 文本排版规则

- 中文：逐字换行（任意字符间可断行）
- 英文：按词边界换行（空格/连字符处断行）
- 混排：中文字符后可断行，英文单词内不断行
- 行距：字号的 1.5 倍（可配置）
- 页面边距：上下左右各 30px

### D6: 组件划分

```
components/
  littlefs_storage/   LittleFS 挂载，依赖 joltwallet/littlefs
  font_manager/       stb_truetype 封装，字体扫描/加载/字形缓存
  text_layout/        文本排版引擎（换行、分页计算、渲染）
pages/
  bookshelf_page      书架页面（在 main/pages/ 目录）
  reader_page         阅读页面（在 main/pages/ 目录）
```

页面代码放在 `main/pages/` 下而非独立 component，因为页面是应用层逻辑，直接依赖具体的 component API。

### D7: 阅读页面交互设计

- 屏幕左半（x < 270）TAP → 上一页
- 屏幕右半（x >= 270）TAP → 下一页
- LONG_PRESS → 返回书架（`ui_page_pop()`）
- 首页左翻/末页右翻无响应（不循环）

## Risks / Trade-offs

**[stb_truetype 渲染速度]** → CJK 字形复杂，首次光栅化可能较慢
→ 缓解：PSRAM 字形缓存（LRU，容量 512~1024 个字形），常用字预热

**[大文件预扫描耗时]** → 10MB+ 的 TXT 首次打开可能需要 10+ 秒
→ 缓解：显示扫描进度；缓存持久化后仅首次需要

**[LittleFS 分区写入磨损]** → 字体文件只读，实际无磨损问题
→ 注意：`format_if_mount_failed = true` 仅在首次或损坏时格式化

**[TTC 格式兼容性]** → 文泉驿微米黑是 TTC（TrueType Collection）格式
→ stb_truetype 支持 TTC，通过 `stbtt_GetFontOffsetForIndex()` 选择字面

**[UTF-8 编码假设]** → TXT 文件可能是 GBK 编码
→ 首版仅支持 UTF-8，后续可增加编码检测
