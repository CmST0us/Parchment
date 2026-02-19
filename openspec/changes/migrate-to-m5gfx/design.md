## Context

当前 Parchment 项目基于 epdiy 库驱动 ED047TC2 E-Ink 显示屏（960×540 物理、540×960 逻辑 portrait），使用独立 gt911 驱动处理触摸，ui_core 旧框架已基本废弃但仍编译。字体系统使用自定义 .pfnt 格式，依赖 epdiy 的 `EpdFont`/`EpdGlyph` 类型。

M5GFX v0.2.15 和 M5Unified v0.2.10 已作为 git submodule 添加（`components/M5GFX/`、`components/M5Unified/`），但迁移尚未开始。

参考项目 M5ReadPaper 提供了成熟的字体引擎架构：离线预渲染 + 多级 PSRAM 缓存 + 单字号运行时缩放，已在 M5PaperS3 上验证可行。

## Goals / Non-Goals

**Goals:**
- 完全移除 epdiy 和 gt911 依赖，改用 M5GFX/M5Unified 作为唯一硬件抽象层
- 实现全新字体引擎：自定义 .bin 格式（4bpp 灰度 + RLE 压缩）、单一 32px 基础字号 + 面积加权缩放、多级 PSRAM 缓存
- 清除 ui_core 旧框架代码
- 保持 ink_ui 的 View 树/FlexLayout/VC 架构不变

**Non-Goals:**
- 不支持竖排排版（后续单独做）
- 不实现 M5GFX 的 IFont 接口桥接——字体渲染完全自研，通过 `pushImage()` 输出
- 不重构 ink_ui 的 View/FlexLayout/Event 系统——只改显示和输入底层
- 不做 M5GFX 的 Sprite（M5Canvas）离屏渲染——直接在主 Display 上绘制

## Decisions

### Decision 1: M5GFX 集成方式——直接 API 调用

**选择**: ink_ui Canvas 直接调用 M5GFX 的 `fillRect()`, `drawPixel()`, `pushImage()` 等 API

**替代方案**:
- A) 最小替换：只替换 epd_driver，Canvas 仍操作 raw framebuffer → 保留了手动坐标变换和 4bpp 打包，改动小但技术债务残留
- B) 用 M5Canvas sprite 做离屏渲染 → 额外 PSRAM 开销，增加复杂度

**理由**: M5GFX 的 Panel_EPD 自动处理坐标旋转、4bpp 打包、电源管理和脏区域刷新。直接调用可以删除大量手动逻辑。Canvas 变为 M5GFX 绑定的薄封装层。

### Decision 2: 字体格式——自定义 .bin（4bpp + RLE）

**选择**: 参考 M5ReadPaper 的架构，设计全新的 .bin 字体格式

**替代方案**:
- A) 迁移到 M5GFX 的 VLW 格式 → 无 glyph 缓存（每字符 2 次 file seek），CJK 渲染极慢；同时只能有 1 个 VLW 活跃
- B) 保留 .pfnt + IFont 适配器 → 字体类型仍然耦合，只是从 epdiy 类型换成自定义类型，不够彻底
- C) 使用 M5ReadPaper 的 .bin 格式 → 只有 3 级灰度，EPD 4bpp 能力未充分利用

**理由**: 4bpp 灰度提供 16 级抗锯齿，在 EPD 上文字边缘更平滑。RLE 压缩简单且解码快速。单一 32px 字号 + 面积加权缩放节省存储空间。多级 PSRAM 缓存保证 CJK 渲染性能。

### Decision 3: 字体缩放——面积加权灰度采样

**选择**: 单一 32px 基础字号，运行时面积加权缩放到目标字号

**理由**: 32px 是阅读字体的最大常用字号。缩小时面积加权采样计算源矩形内灰度密度，保留抗锯齿信息。16 级灰度输出比 M5ReadPaper 的 3 级更平滑。缩放比例范围 0.5~1.0（对应 16px~32px），避免放大带来的质量问题。

### Decision 4: 触摸——M5.Touch 替代 gt911

**选择**: GestureRecognizer 改为轮询 `M5.Touch.getDetail()`，去掉 GPIO48 中断 + 信号量机制

**替代方案**:
- 保留独立 gt911 驱动 → 可行但多余，M5Unified 已内置完整 GT911 支持
- 用 M5Unified 的内置手势 → M5Unified 没有提供 Swipe/LongPress 手势识别，我们的状态机更完整

**理由**: M5.Touch 坐标已映射到逻辑坐标系，省去坐标转换。手势状态机（Tap/LongPress/Swipe）保持不变，只替换数据采集层。触摸任务从中断驱动改为 20ms 轮询（M5.Touch 内部仍有中断优化）。

### Decision 5: 字体输出——pushImage 而非 IFont

**选择**: 字体引擎自行解码 + 缩放 glyph bitmap，通过 `M5.Display.pushImage()` 输出到屏幕

**替代方案**:
- 实现 `IFont` 接口 → 需要适配 M5GFX 的 `drawChar()` 签名和内部渲染管线，增加耦合

**理由**: pushImage 是最简单的像素输出方式。字体引擎完全独立于 M5GFX 字体系统，可以自由控制缓存、缩放、灰度处理。与 M5ReadPaper 的方案一致。

### Decision 6: RenderEngine 与 M5GFX 脏区域

**选择**: 保留 ink_ui 自己的脏区域追踪，在 flush 阶段调用 `M5.Display.display(x, y, w, h)` 刷新

**理由**: M5GFX 的 auto-display 在 `endWrite()` 时自动刷新，但我们需要精确控制刷新时机和模式（epd_quality vs epd_fast）。保留自有脏区域管理更灵活。关闭 M5GFX 的 auto-display (`setAutoDisplay(false)`)。

## Risks / Trade-offs

### Risk 1: 字体缩放质量
- **风险**: 从 32px 缩放到 16px（0.5×）时小号 CJK 字符可能模糊
- **缓解**: 面积加权采样 + 16 级灰度比 M5ReadPaper 的 3 级好很多；如果不可接受可后续增加 16px 基础字号

### Risk 2: RLE 压缩率
- **风险**: 4bpp 灰度数据的 RLE 压缩率可能不如 zlib
- **缓解**: 预估 ~2:1 压缩率，7000 字的 .bin 约 1.3MB，SD 卡和 LittleFS 均可承受；如果过大可考虑改用 zlib

### Risk 3: M5GFX Panel_EPD 刷新控制
- **风险**: M5GFX 的 `display(x,y,w,h)` 对局部刷新的控制力可能不如 epdiy 的 `epd_hl_update_area()` 精细（例如无法指定 temperature 参数）
- **缓解**: M5GFX 内部默认 temperature=25，与当前 epdiy 用法一致

### Risk 4: M5GFX 的 startWrite/endWrite 批量绘图
- **风险**: 忘记 startWrite/endWrite 包裹会导致每个绘图操作触发独立刷新
- **缓解**: Canvas 构造时 startWrite，析构时 endWrite（RAII）；且关闭 auto-display

### Risk 5: 迁移期间双系统共存
- **风险**: 大规模迁移可能长时间处于不可编译/不可运行状态
- **缓解**: 分阶段实施：先新建字体引擎（独立组件），再替换显示驱动，最后清理旧代码

## Open Questions

1. **字体文件存储位置**: .bin 文件放 SD 卡还是 LittleFS？SD 卡容量大但需要用户手动拷贝；LittleFS 可烧录但 Flash 空间有限
2. **M5GFX 8bpp 颜色映射**: M5GFX 使用 0x00=黑/0xFF=白，当前 ink_ui 使用 0x00=黑/0xF0=白（4bpp 高位）。需确认 Canvas 层颜色常量如何调整
3. **GestureRecognizer 任务模型**: 改为轮询后是否保留独立 FreeRTOS 任务，还是合入 Application 主循环？
