## Context

ui_core 框架已有完整的几何绘图原语（`ui_canvas`），通过逻辑坐标系（540×960 portrait）到物理 framebuffer（960×540 landscape）的转置映射实现屏幕渲染。epdiy 库自带字体渲染引擎（`font.c`），但它直接在物理坐标系下通过 `epd_draw_pixel()` 写入，无法与我们的逻辑坐标系兼容。

当前需要为阅读器的核心功能——文本显示——构建渲染和排版能力。目标测试文本为中日英混排歌词（312 个唯一字符），字体选定 Noto Sans CJK SC，6 个字号（20/24/28/32/36/40px）。

## Goals / Non-Goals

**Goals:**
- 在 `ui_canvas` 逻辑坐标系下实现字符级渲染，直接复用 `fb_set_pixel()`，无临时 buffer
- 复用 epdiy 的 `EpdFont`/`EpdGlyph` 数据结构和 `fontconvert.py` 工具链
- 支持 CJK 混排自动换行（逐字断行 + 英文单词断行 + 标点禁则）
- 提供分页渲染 API，为阅读视图翻页做基础
- 预编译 6 个字号的字体资源，Flash 占用可控

**Non-Goals:**
- 不实现运行时 TrueType 解析（FreeType 移植）
- 不实现富文本格式（粗体/斜体/下划线），本阶段只支持单一字体
- 不实现文字滚动动画
- 不实现完整的 Unicode BiDi（双向文本）支持
- 不实现字体 fallback chain（缺字显示方块或跳过）

## Decisions

### D1: 自研渲染器而非封装 epdiy

**选择**: 自己实现 `draw_char`，直接在逻辑坐标系下通过 `fb_set_pixel()` 写入。

**备选方案**: 用临时 buffer 调用 epdiy 的 `epd_write_string()`，再逐像素转写到主 framebuffer。

**理由**: epdiy 的 `draw_char` 核心逻辑约 70 行，重写简单。自研方案零临时 buffer、零拷贝，且不受 epdiy 内部 API 变化影响。临时 buffer 方案在大字号下每个字符需要分配/释放内存，性能和碎片化都不理想。

### D2: 复用 epdiy 数据结构 + fontconvert 工具链

**选择**: 直接使用 `EpdFont`、`EpdGlyph`、`EpdUnicodeInterval` 结构，字体通过 `fontconvert.py` 生成 `.h` 文件编译进 Flash。

**理由**: 结构设计合理（4bpp 灰度、zlib 压缩、Unicode interval 索引），`fontconvert.py` 成熟可用。`epd_get_glyph()` 函数已链接在项目中可直接调用。没有理由重新发明。

### D3: fontconvert.py DPI 调整

**选择**: 使用 72 DPI 替代默认的 150 DPI，使 point size = pixel height（1:1 映射）。

**理由**: fontconvert.py 中 `face.set_char_size(size << 6, size << 6, 150, 150)` 默认 150 DPI，导致 size 参数含义不直观（20pt @ 150DPI ≈ 42px）。改为 72 DPI 后 `size` 参数直接等于像素高度。实现方式：复制 fontconvert.py 到项目 `tools/` 目录并修改 DPI 参数。

### D4: 字符集按需编译

**选择**: 使用 `fontconvert.py --string` 参数，只包含实际用到的字符。

**备选方案**: 包含完整 CJK 区间。

**理由**: 完整 CJK（~21000 字符 × 6 字号）估算 ~15MB，超出 Flash 容量。按需编译 312 字符 × 6 字号 ≈ 750KB，可控。后续扩展字符集只需重新运行 fontconvert。

### D5: 模块拆分 — ui_canvas 做字符级，ui_text 做排版

**选择**: `ui_canvas` 负责单字符/单行渲染和度量（低层），`ui_text` 负责自动换行和分页（高层）。

**理由**: 关注点分离。UI 组件（按钮、标题）只需 `ui_canvas_draw_text()`，阅读视图才需要排版引擎。两层可独立测试和演进。

### D6: CJK 断行策略

**选择**: CJK 字符逐字可断行 + 标点禁则（Kinsoku Shori）。

**规则**:
- CJK 字符（U+4E00-U+9FFF、平假名、片假名）每个字符后都是合法断点
- ASCII 字母序列按单词整体不拆分（在空格处断行）
- 行首禁止字符（不能出现在行首的标点）：`，。、；：？！）》」』】〉…——`
- 行尾禁止字符（不能出现在行尾的标点）：`（《「『【〈`
- 遇到禁则冲突时：行首禁止标点从下一行推到上一行末尾

### D7: 灰度 alpha 混合

**选择**: glyph 的 4bpp alpha 值与前景色和背景色线性插值。

**公式**: `color = bg + alpha * (fg - bg) / 15`，其中 alpha 范围 0-15。

**理由**: 与 epdiy 的 `draw_char` 逻辑一致，保证字体边缘的抗锯齿效果。

## Risks / Trade-offs

**[Flash 容量随字符集增长]** → 当前 312 字符足够测试，正式阅读需要扩展到常用 3500 汉字（GB2312 一级）。估算 3500 × 6 字号 ≈ 8MB（压缩后）。需要评估是否能接受，或考虑减少字号数量。

**[fontconvert.py 修改维护]** → 复制到 `tools/` 并修改 DPI 参数，与上游 epdiy 版本脱钩。风险可控，修改极小（一行代码）。

**[缺少字形时的表现]** → 当前不实现 fallback，遇到不在字符集中的字符将跳过（不渲染）。用户可能看到"缺字"但不会崩溃。后续可加方块占位符。

**[大字号单字符解压开销]** → 40px 字形压缩后可能有数百字节，每个字符需要 malloc + zlib 解压。对于长文本逐字解压可能有累积开销。可以考虑缓存最近解压的字形，但本阶段先不优化。

## Open Questions

无——通过前期探索讨论，所有关键决策已明确。
