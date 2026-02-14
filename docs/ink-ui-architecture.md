# InkUI 架构设计文档

> Parchment E-Reader UI 框架重构方案
> 创建日期: 2026-02-15

## 1. 背景与动机

### 1.1 当前架构的痛点

当前 UI 框架 (`ui_core`) 是一个轻量级的 C 立即模式框架，存在以下核心问题：

**无组件生命周期**
- Widget 是纯函数 (`draw(fb, &config)`)，没有 create → mount → update → unmount → destroy 流程
- 所有状态散落在 page 的 static 全局变量中
- Widget 结构体混合了不可变配置和可变状态 (`scroll_offset`, `visible`)

**全量重绘**
- `on_render(fb)` 每次清空整个 259KB framebuffer 再从头画所有 widget
- 即使只关闭一个下拉菜单也触发全屏重绘
- 成本: ~200ms CPU + 1.5-3s EPD 刷新

**粗糙的脏区域管理**
- 单一 bounding box 合并所有脏区域
- 两个小区域在屏幕两端 → 合并成巨大矩形 → 退化为全屏刷新
- 无法做多区域独立局部刷新

**墨水屏刷新策略太简单**
- 只有两档: 面积 < 60% 用 MODE_DU, ≥ 60% 用 MODE_GL16
- MODE_DU 是单色的，灰度内容（进度条、抗锯齿文字）会劣化
- 没有残影计数器，不知道何时该做一次 GC16 全清

**其他问题**
- 所有布局位置硬编码 (`{.x=0, .y=84, .w=540, .h=876}`)
- Widget 回调直接访问 page 内部状态，无法复用
- 页面返回时丢失状态 (`on_enter(NULL)`)
- Canvas 逐像素坐标变换，无行级优化

### 1.2 重构目标

1. **UIKit 风格的组件生命周期** — View 树 + ViewController，精确控制创建、显示、更新、销毁
2. **局部重绘** — 只有状态变化的 View 才重绘，其他部分不动
3. **智能墨水屏刷新** — 多脏区域独立刷新，每个区域选择最优刷新模式，残影自动管理
4. **翻页代替滚动** — 墨水屏帧率低，翻页体验远优于滚动
5. **FlexBox 布局** — 自动计算位置，告别硬编码坐标
6. **Modern C++17** — 智能指针、模板，关闭异常和 RTTI

---

## 2. 技术决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 语言 | C++17 | 类层次、智能指针、模板、RAII |
| 异常 | `-fno-exceptions` | 嵌入式标准做法，零开销 |
| RTTI | `-fno-rtti` | 不需要 `dynamic_cast`，节省空间 |
| View 树深度 | 完整嵌套 | 灵活性，HeaderView 内部可含 Icon + Title 子 View |
| 布局系统 | 简单 FlexBox | 自动计算，比 frame-based 灵活，比 Auto Layout 轻量 |
| 列表交互 | 翻页 | 墨水屏帧率低，翻页体验优于滚动 |
| 浮层遮挡 | Backing Store 为主 | 微秒级恢复，零重绘；损伤区域裁剪重绘为 fallback |
| 所有权模型 | `unique_ptr` 为主 | 零开销，View 树天然适合独占所有权 |
| 命名空间 | `ink::` | 所有框架类在 `ink` 命名空间下 |
| 底层驱动 | 保持 C | epdiy/gt911/sd_storage 不改，C++ 薄 wrapper 桥接 |

---

## 3. 整体架构

```
┌──────────────────────────────────────────────────────────────┐
│                        Application                            │
│  RunLoop: 事件等待 → 分发 → Layout → Render → EPD Refresh    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────┐    ┌──────────────────────────┐    │
│  │  NavigationController│    │    GestureRecognizer     │    │
│  │  ┌─────────────────┐│    │  (触摸状态机, 手势识别)    │    │
│  │  │ ViewController  ││    └──────────┬───────────────┘    │
│  │  │  ┌────────────┐ ││               │ TouchEvent         │
│  │  │  │  View Tree  │ ││               ▼                   │
│  │  │  │  ┌───────┐ │ ││    ┌──────────────────────────┐    │
│  │  │  │  │ View  │ │ ││    │    Event Queue            │    │
│  │  │  │  │┌─────┐│ │ ││    │  (FreeRTOS, typed events)  │   │
│  │  │  │  ││View ││ │ ││    └──────────────────────────┘    │
│  │  │  │  │└─────┘│ │ ││                                    │
│  │  │  │  └───────┘ │ ││                                    │
│  │  │  └────────────┘ ││                                    │
│  │  └─────────────────┘│                                    │
│  └─────────────────────┘                                    │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│                      RenderEngine                             │
│  收集脏 View → 局部重绘到 FB → 智能选择刷新模式 → EPD 更新    │
├──────────────────────────────────────────────────────────────┤
│                        Canvas                                 │
│  裁剪 + 坐标变换 + 绘图原语 (优化的行级操作)                   │
├──────────────────────────────────────────────────────────────┤
│  EpdDriver (C++ wrap) │  FontManager  │  IconManager         │
├──────────────────────────────────────────────────────────────┤
│  epdiy (C)  │  GT911 (C)  │  LittleFS  │  SD  │  NVS       │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. 核心类设计

### 4.1 Geometry — 基础几何类型

```cpp
// ink_ui/include/ink_ui/core/Geometry.h
namespace ink {

struct Point { int x = 0, y = 0; };
struct Size  { int w = 0, h = 0; };

struct Insets {
    int top = 0, right = 0, bottom = 0, left = 0;
    static Insets uniform(int v) { return {v, v, v, v}; }
    static Insets symmetric(int vertical, int horizontal) {
        return {vertical, horizontal, vertical, horizontal};
    }
};

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    bool contains(int px, int py) const;
    bool intersects(const Rect& other) const;
    Rect intersection(const Rect& other) const;
    Rect unionWith(const Rect& other) const;
    int area() const { return w * h; }
    Rect inset(const Insets& i) const;
    bool isEmpty() const { return w <= 0 || h <= 0; }

    int right() const { return x + w; }
    int bottom() const { return y + h; }

    static Rect zero() { return {0, 0, 0, 0}; }
};

} // namespace ink
```

### 4.2 View — 基类

```cpp
// ink_ui/include/ink_ui/core/View.h
namespace ink {

enum class RefreshHint {
    Fast,       // MODE_DU: ~200ms, 单色, 用于图标/按钮状态切换
    Quality,    // MODE_GL16: ~1.5s, 灰度准确, 不闪, 用于文本/进度条
    Full,       // MODE_GC16: ~3s, 闪黑, 消除残影
    Auto        // RenderEngine 根据内容自动决定
};

class View {
public:
    View();
    virtual ~View();

    // 不可拷贝，可移动
    View(const View&) = delete;
    View& operator=(const View&) = delete;
    View(View&&) = default;
    View& operator=(View&&) = default;

    // ── View 树操作 ──
    void addSubview(std::unique_ptr<View> child);
    std::unique_ptr<View> removeFromParent();
    void clearSubviews();
    View* parent() const { return parent_; }
    const std::vector<std::unique_ptr<View>>& subviews() const { return subviews_; }

    // ── 几何 ──
    void setFrame(const Rect& frame);
    Rect frame() const { return frame_; }
    Rect bounds() const { return {0, 0, frame_.w, frame_.h}; }
    Rect screenFrame() const;   // 累加所有祖先 frame 的绝对坐标

    // ── 脏标记 ──
    void setNeedsDisplay();     // 标记自身需要重绘, 冒泡到根
    void setNeedsLayout();      // 标记布局需要重算, 冒泡到根
    bool needsDisplay() const { return needsDisplay_; }
    bool needsLayout() const { return needsLayout_; }
    bool subtreeNeedsDisplay() const { return subtreeNeedsDisplay_; }

    // ── 属性 ──
    void setBackgroundColor(uint8_t gray) { backgroundColor_ = gray; setNeedsDisplay(); }
    void setHidden(bool hidden);
    void setRefreshHint(RefreshHint hint) { refreshHint_ = hint; }
    bool isHidden() const { return hidden_; }
    uint8_t backgroundColor() const { return backgroundColor_; }
    RefreshHint refreshHint() const { return refreshHint_; }

    // ── 虚函数: 子类重写 ──
    virtual void onDraw(Canvas& canvas) {}
    virtual void onLayout();                            // 默认实现: FlexBox 算法
    virtual Size intrinsicSize() { return {-1, -1}; }  // -1 表示无固有尺寸
    virtual View* hitTest(int x, int y);                // 默认: 递归子 View, 返回最深命中
    virtual bool onTouchEvent(const TouchEvent& e) { return false; }  // true=消费事件

    // ── FlexBox 布局属性 ──
    FlexStyle flexStyle_;       // 作为容器: 如何排列子 View
    int flexGrow_ = 0;         // 作为子项: 伸展权重 (0=固定)
    int flexBasis_ = -1;       // 作为子项: 基础尺寸 (-1=auto)
    Align alignSelf_ = Align::Auto; // 作为子项: 覆盖父容器 alignItems

protected:
    Rect frame_ = Rect::zero();
    View* parent_ = nullptr;    // non-owning
    std::vector<std::unique_ptr<View>> subviews_;

    uint8_t backgroundColor_ = 0xFF;   // 4bpp white
    bool hidden_ = false;
    bool opaque_ = true;
    RefreshHint refreshHint_ = RefreshHint::Auto;

    bool needsDisplay_ = true;
    bool needsLayout_ = true;
    bool subtreeNeedsDisplay_ = false;

    void propagateDirtyUp();    // setNeedsDisplay 内部调用
};

} // namespace ink
```

**`setNeedsDisplay()` 冒泡机制**:

```
button 文字变了 → button.setNeedsDisplay()
                      │
                      ├── button.needsDisplay_ = true
                      ├── toolbar.subtreeNeedsDisplay_ = true   (parent)
                      ├── rootView.subtreeNeedsDisplay_ = true  (grandparent)
                      └── 到根为止

RenderEngine 遍历树:
  rootView.subtreeNeedsDisplay_ == true → 递归
    header.needsDisplay_ == false → 跳过
    toolbar.subtreeNeedsDisplay_ == true → 递归
      button.needsDisplay_ == true → 重绘这个 View!
    listView → 跳过
```

### 4.3 Canvas — 裁剪 + 绘图

```cpp
// ink_ui/include/ink_ui/core/Canvas.h
namespace ink {

class Canvas {
public:
    Canvas(uint8_t* fb, Rect clip);

    // 创建子 Canvas (裁剪区域取交集)
    Canvas clipped(const Rect& subRect) const;

    // 绘图原语 — 所有坐标相对于 clip 的左上角
    void fillRect(const Rect& rect, uint8_t gray);
    void drawRect(const Rect& rect, uint8_t gray, int thickness = 1);
    void drawHLine(int x, int y, int width, uint8_t gray);
    void drawVLine(int x, int y, int height, uint8_t gray);
    void drawLine(Point from, Point to, uint8_t gray);

    // 位图绘制
    void drawBitmap(const uint8_t* data, int x, int y, int w, int h);
    void drawBitmapFg(const uint8_t* data, int x, int y, int w, int h,
                      uint8_t fgColor);

    // 文字
    void drawText(const EpdFont* font, const char* text,
                  int x, int y, uint8_t color);
    void drawTextN(const EpdFont* font, const char* text, int len,
                   int x, int y, uint8_t color);
    int measureText(const EpdFont* font, const char* text);

    // 填充整个 clip 区域
    void clear(uint8_t gray = 0xFF);

    Rect clipRect() const { return clip_; }

private:
    uint8_t* fb_;
    Rect clip_;             // 逻辑坐标系 (540x960 portrait)

    // 坐标变换: 逻辑 → 物理 framebuffer
    // 逻辑 (lx, ly) → 物理 (py = ly, px = 539 - lx) — 转置+X翻转
    void setPixel(int lx, int ly, uint8_t gray);
    uint8_t getPixel(int lx, int ly) const;

    // 优化路径: 一次性处理一整行
    void fillRowFast(int lx, int ly, int width, uint8_t gray);
};

} // namespace ink
```

### 4.4 FlexBox 布局

```cpp
// ink_ui/include/ink_ui/core/FlexLayout.h
namespace ink {

enum class FlexDirection { Column, Row };
enum class Align { Start, Center, End, Stretch, Auto };
enum class Justify { Start, Center, End, SpaceBetween, SpaceAround };

struct FlexStyle {
    FlexDirection direction = FlexDirection::Column;
    Align alignItems = Align::Stretch;
    Justify justifyContent = Justify::Start;
    int gap = 0;            // 子项间距 (px)
    Insets padding;         // 容器内边距
};

// 布局算法 (View::onLayout 默认实现调用此函数)
void flexLayout(View* container);

} // namespace ink
```

**FlexBox 算法**:

```
flexLayout(container):
  direction = container.flexStyle_.direction
  主轴 = (direction == Column) ? height : width
  交叉轴 = 另一个

  1. 测量阶段:
     fixedTotal = 0, flexTotal = 0
     for each visible child:
       if child.flexGrow_ == 0:
         // 固定尺寸: 用 flexBasis 或 intrinsicSize
         size = child.flexBasis_ >= 0 ? flexBasis_ : child.intrinsicSize()
         fixedTotal += size.主轴
       else:
         flexTotal += child.flexGrow_

  2. 分配阶段:
     available = 容器主轴尺寸 - padding - fixedTotal - (N-1) * gap
     flexUnit = available / flexTotal

  3. 定位阶段:
     cursor = padding.start (top or left)
     for each visible child:
       mainSize = (flexGrow > 0) ? flexGrow * flexUnit : fixedSize
       crossSize = (alignItems == Stretch) ? 交叉轴可用尺寸 : child.intrinsicSize().交叉轴
       crossPos = 根据 alignItems/alignSelf 计算

       child.setFrame({crossPos, cursor, crossSize, mainSize})  // Column
       // 或 {cursor, crossPos, mainSize, crossSize}            // Row
       cursor += mainSize + gap

       child.onLayout()  // 递归
```

### 4.5 RenderEngine — 墨水屏渲染引擎

```cpp
// ink_ui/include/ink_ui/core/RenderEngine.h
namespace ink {

class RenderEngine {
public:
    explicit RenderEngine(EpdDriver& driver);

    /// 主渲染循环 — Application 每帧调用
    void renderCycle(View* rootView);

    /// 浮层消失后修复底层内容 (损伤区域裁剪重绘)
    void repairDamage(View* rootView, const Rect& damage);

    /// 强制全屏 GC16 清除
    void forceFullClear();

    /// 获取残影计数
    int partialRefreshCount() const { return partialCount_; }

private:
    EpdDriver& driver_;
    uint8_t* fb_;

    // ── 多脏区域 ──
    static constexpr int MAX_DIRTY_REGIONS = 8;
    struct DirtyEntry {
        Rect rect;
        RefreshHint hint = RefreshHint::Auto;
        bool used = false;
    };
    DirtyEntry dirtyRegions_[MAX_DIRTY_REGIONS];
    int dirtyCount_ = 0;

    // ── 残影管理 ──
    int partialCount_ = 0;
    static constexpr int GHOST_THRESHOLD = 10;

    // ── 渲染阶段 ──
    void layoutPass(View* root);
    void collectDirty(View* view);
    void drawDirtyViews(View* view);
    void drawSubtree(View* view, Canvas& canvas);
    void flushToDisplay();
    void mergeOverlappingRegions();
    int resolveEpdMode(RefreshHint hint) const;
    void addDirtyRegion(const Rect& rect, RefreshHint hint);
};

} // namespace ink
```

**渲染流程**:

```
renderCycle(rootView):
│
├── Phase 1: Layout
│   if rootView.needsLayout_:
│     rootView.onLayout()  // 递归 FlexBox
│
├── Phase 2: 收集脏区域
│   collectDirty(rootView):
│     if view.needsDisplay_:
│       addDirtyRegion(view.screenFrame(), view.refreshHint_)
│     elif view.subtreeNeedsDisplay_:
│       for each child: collectDirty(child)
│
├── Phase 3: 重绘脏 View
│   drawDirtyViews(rootView):
│     if view.needsDisplay_:
│       Canvas canvas(fb_, view.screenFrame())
│       canvas.clear(view.backgroundColor_)
│       view.onDraw(canvas)
│       drawSubtree(view, canvas)    // 所有子 View 强制重绘
│       view.needsDisplay_ = false
│       view.subtreeNeedsDisplay_ = false
│     elif view.subtreeNeedsDisplay_:
│       for each child: drawDirtyViews(child)
│       view.subtreeNeedsDisplay_ = false
│
├── Phase 4: 合并 & 刷新
│   mergeOverlappingRegions()
│   for each region:
│     mode = resolveEpdMode(region.hint)
│     driver_.updateArea(region.rect, mode)
│     partialCount_++
│
└── Phase 5: 残影管理
    if partialCount_ >= GHOST_THRESHOLD:
      driver_.fullClear()
      partialCount_ = 0
```

### 4.6 RefreshPolicy — 刷新决策

```
┌─────────────────────┬──────────────┬──────────┬──────────┬──────────┐
│ 场景                 │ RefreshHint  │ EPD Mode │ 速度     │ 质量     │
├─────────────────────┼──────────────┼──────────┼──────────┼──────────┤
│ 图标/按钮状态切换    │ Fast         │ MODE_DU  │ ~200ms   │ 单色     │
│ 文本内容变化         │ Quality      │ MODE_GL16│ ~1.5s    │ 16级灰度 │
│ 进度条更新           │ Quality      │ MODE_GL16│ ~1.5s    │ 16级灰度 │
│ 列表翻页             │ Quality      │ MODE_GL16│ ~1.5s    │ 16级灰度 │
│ 页面切换             │ Quality      │ MODE_GL16│ ~1.5s    │ 16级灰度 │
│ 累计 N 次局部刷新后  │ 自动升级     │ MODE_GC16│ ~3s      │ 消残影   │
│ 用户手动触发         │ Full         │ MODE_GC16│ ~3s      │ 消残影   │
└─────────────────────┴──────────────┴──────────┴──────────┴──────────┘
```

**多区域合并策略**:

```
收集到的脏区域:
  A: (0, 0, 540, 48)    hint=Fast       ← header 图标变了
  B: (0, 300, 540, 80)  hint=Quality    ← list item 3 变了
  C: (0, 380, 540, 80)  hint=Quality    ← list item 4 变了

合并规则:
  1. 相邻 + 相同 hint → 合并 (B+C → (0,300,540,160) Quality)
  2. 不同 hint → 保持独立
  3. 合并后面积 > 60% 屏幕 → 退化为全屏刷新

结果:
  Region 1: (0, 0, 540, 48)     → MODE_DU    ~200ms
  Region 2: (0, 300, 540, 160)  → MODE_GL16  ~1.5s
  (比全屏 GL16 快, 且 header 刷新毫秒级)
```

### 4.7 ViewController — 页面生命周期

```cpp
// ink_ui/include/ink_ui/core/ViewController.h
namespace ink {

class ViewController {
public:
    virtual ~ViewController() = default;

    /// 获取根 View (首次调用触发 viewDidLoad)
    View& getView();

    std::string title_;

protected:
    std::unique_ptr<View> view_;

    // ── 生命周期回调 (子类重写) ──

    /// 创建 View 树 — 只调用一次
    virtual void viewDidLoad() = 0;

    /// 即将显示 — 每次 push 或从上层页面返回时调用
    virtual void viewWillAppear() {}

    /// 已显示 — 可以开始定时器、加载数据等
    virtual void viewDidAppear() {}

    /// 即将隐藏 — 新页面被 push 或本页面被 pop
    virtual void viewWillDisappear() {}

    /// 已隐藏 — 停止定时器、释放临时资源
    virtual void viewDidDisappear() {}

    /// 处理非触摸事件 (定时器、自定义事件等)
    virtual void handleEvent(const Event& e) {}

private:
    bool viewLoaded_ = false;
    friend class NavigationController;
};

} // namespace ink
```

**与当前架构的生命周期对比**:

```
当前 C 架构                    InkUI C++ 架构
──────────────────────────     ──────────────────────────
on_enter(arg)                  viewDidLoad()       ← 只调用一次
                               viewWillAppear()    ← 每次可见时
                               viewDidAppear()

(无)                           (View 树保持存活, 状态保留)

on_exit()                      viewWillDisappear()
                               viewDidDisappear()

on_event(event)                View 树自动 hitTest 分发
                               + handleEvent() 处理非触摸事件

on_render(fb)                  不需要!
                               RenderEngine 自动收集脏 View 重绘
```

### 4.8 NavigationController — 页面栈

```cpp
// ink_ui/include/ink_ui/core/NavigationController.h
namespace ink {

class NavigationController {
public:
    NavigationController();

    /// 推入新页面
    void push(std::unique_ptr<ViewController> vc);

    /// 弹出当前页面, 返回上一页
    void pop();

    /// 替换当前页面
    void replace(std::unique_ptr<ViewController> vc);

    /// 当前页面
    ViewController* current() const;

    /// 栈深度
    int depth() const { return static_cast<int>(stack_.size()); }

private:
    std::vector<std::unique_ptr<ViewController>> stack_;
    static constexpr int MAX_DEPTH = 8;
};

} // namespace ink
```

**生命周期调用顺序**:

```
push(B):
  A.viewWillDisappear()
  A.viewDidDisappear()
  stack_.push(B)
  B.getView()   // 触发 B.viewDidLoad() (如果首次)
  B.viewWillAppear()
  B.viewDidAppear()
  // A 的 View 树保持存活, 状态不丢失

pop():
  B.viewWillDisappear()
  B.viewDidDisappear()
  stack_.pop()   // B 被 unique_ptr 销毁
  A.viewWillAppear()   // A 重新出现, 状态完好
  A.viewDidAppear()
```

### 4.9 OverlayView — 浮层 (Backing Store)

```cpp
// ink_ui/include/ink_ui/views/OverlayView.h
namespace ink {

class OverlayView : public View {
public:
    /// 显示浮层 (自动保存底层快照)
    void show(View* anchorParent);

    /// 隐藏浮层 (自动恢复快照; 如果 invalidated 则走损伤重绘)
    void hide();

    /// 标记底层内容已变化 (快照失效)
    void invalidateBackingStore() { backingStoreValid_ = false; }

protected:
    void willAppear();      // 保存 framebuffer 快照
    void willDisappear();   // 恢复快照 或 请求损伤重绘

private:
    std::unique_ptr<uint8_t[]> backingStore_;
    Rect savedRegion_;
    bool backingStoreValid_ = false;
};

} // namespace ink
```

**工作流程**:

```
dropdown.show():
  1. backingStore_ = malloc(area / 2)   // 4bpp
  2. captureFramebuffer(backingStore_, screenFrame())
  3. backingStoreValid_ = true
  4. addSubview to parent
  5. setNeedsDisplay()  → RenderEngine 画出浮层

dropdown.hide():
  if backingStoreValid_:
    restoreFramebuffer(backingStore_, savedRegion_)  // 微秒级
    markEpdRegionDirty(savedRegion_)                 // 通知 EPD 刷新
  else:
    RenderEngine::repairDamage(rootView, savedRegion_)  // fallback
  removeFromParent()
  backingStore_.reset()
```

### 4.10 触摸事件分发

```cpp
// ink_ui/include/ink_ui/core/Event.h
namespace ink {

enum class TouchType { Down, Move, Up, Tap, LongPress };
enum class SwipeDirection { Left, Right, Up, Down };

struct TouchEvent {
    TouchType type;
    int x, y;           // 逻辑坐标
    int startX, startY; // 起始点 (用于判断移动距离)
};

struct SwipeEvent {
    SwipeDirection direction;
};

struct TimerEvent {
    int timerId;
};

// 统一事件类型 (union-like)
enum class EventType { Touch, Swipe, Timer, Custom };

struct Event {
    EventType type;
    union {
        TouchEvent touch;
        SwipeEvent swipe;
        TimerEvent timer;
        int32_t customParam;
    };
};

} // namespace ink
```

**分发流程**:

```
GestureRecognizer 识别 Tap at (270, 500)
  │
  ▼
Application::dispatchTouch(event)
  │
  ▼
rootView->hitTest(270, 500)
  递归搜索, 返回最深层包含该点的 View
  结果: listItem[3] at (0, 324, 540, 80)
  │
  ▼
listItem[3].onTouchEvent(tap)
  return true  → 事件消费, 结束
  return false → 冒泡给 listView.onTouchEvent()
                 return false → 冒泡给 rootView
                 ...直到某个 View 消费或到根为止
```

### 4.11 Application — 主循环

```cpp
// ink_ui/include/ink_ui/core/Application.h
namespace ink {

class Application {
public:
    Application();

    /// 初始化所有硬件和子系统
    bool init();

    /// 获取导航控制器
    NavigationController& navigator() { return navigator_; }

    /// 获取渲染引擎
    RenderEngine& renderer() { return *renderEngine_; }

    /// 启动主循环 (永不返回)
    [[noreturn]] void run();

    /// 发送自定义事件
    void postEvent(const Event& event);

    /// 延迟执行 (毫秒)
    void postDelayed(const Event& event, int delayMs);

private:
    EpdDriver& epdDriver_;
    std::unique_ptr<RenderEngine> renderEngine_;
    std::unique_ptr<GestureRecognizer> gestureRecognizer_;
    NavigationController navigator_;
    QueueHandle_t eventQueue_;

    void runLoop();
    void dispatchEvent(const Event& event);
    void dispatchTouch(const TouchEvent& event);
};

} // namespace ink
```

**主循环**:

```
Application::runLoop():
  while (true):
    Event event;
    if (xQueueReceive(eventQueue_, &event, 30000ms)):
      dispatchEvent(event)

    // Layout + Render (如果有脏 View)
    View* rootView = navigator_.current()->getView()
    renderEngine_->renderCycle(rootView)
```

### 4.12 PagedListView — 翻页列表

```cpp
// ink_ui/include/ink_ui/views/PagedListView.h
namespace ink {

class PagedListView : public View {
public:
    // ── 数据源接口 ──
    struct DataSource {
        virtual ~DataSource() = default;
        virtual int itemCount() = 0;
        virtual int itemHeight() = 0;
        virtual std::unique_ptr<View> createItemView(int index) = 0;
    };

    void setDataSource(DataSource* source);
    void goToPage(int page);
    void nextPage();
    void prevPage();

    int currentPage() const { return currentPage_; }
    int totalPages() const;
    int itemsPerPage() const { return itemsPerPage_; }

protected:
    void onLayout() override;

private:
    DataSource* dataSource_ = nullptr;
    int currentPage_ = 0;
    int itemsPerPage_ = 0;         // 由 onLayout 自动计算
    // 子 View 列表由 View::subviews_ 管理
};

} // namespace ink
```

### 4.13 EpdDriver — C++ Wrapper

```cpp
// components/epd_driver/include/EpdDriver.h
#pragma once

extern "C" {
#include "epd_driver.h"
}
#include <ink_ui/core/Geometry.h>

namespace ink {

class EpdDriver {
public:
    static EpdDriver& instance();

    bool init();
    uint8_t* framebuffer();

    /// 全屏刷新 (MODE_GL16, 不闪)
    void updateScreen();

    /// 局部刷新 (指定区域和模式)
    void updateArea(const Rect& logicalArea, int mode);

    /// 全屏闪黑清除 (MODE_GC16)
    void fullClear();

    void powerOn();
    void powerOff();

    static constexpr int SCREEN_WIDTH = 540;
    static constexpr int SCREEN_HEIGHT = 960;
    static constexpr int FB_PHYS_WIDTH = 960;
    static constexpr int FB_PHYS_HEIGHT = 540;

private:
    EpdDriver() = default;
    // 内部调用现有 C 函数
};

} // namespace ink
```

---

## 5. View 子类一览

```
View (基类)
├── frame_, subviews_, flexStyle_, refreshHint_
├── setNeedsDisplay(), setNeedsLayout()
├── onDraw(), onLayout(), hitTest(), onTouchEvent()
│
├── TextLabel                   refreshHint=Quality
│   ├── text_, font_, color_, alignment_
│   └── 单行/多行文字
│
├── IconView                    refreshHint=Fast
│   ├── icon_, tintColor_
│   └── 32x32 4bpp 图标
│
├── ButtonView                  refreshHint=Fast
│   ├── label_, icon_, style_
│   ├── onTap callback
│   └── 含点击区域扩展 (touch padding)
│
├── HeaderView                  refreshHint=Quality
│   ├── title (TextLabel), leftIcon (IconView), rightIcon (IconView)
│   └── 组合 View: 内部含子 View, FlexBox Row 布局
│
├── ProgressBarView             refreshHint=Quality
│   ├── value_ (0-100), trackColor_, fillColor_
│   └── 灰度进度条
│
├── SeparatorView               refreshHint=Fast
│   └── 1px 水平/垂直分隔线
│
├── PagedListView               refreshHint=Quality
│   ├── DataSource 接口
│   ├── currentPage_, itemsPerPage_
│   └── 翻页管理, 子 View 动态创建/销毁
│
├── PageIndicatorView           refreshHint=Fast
│   ├── currentPage_, totalPages_
│   ├── prevButton, nextButton, pageLabel
│   └── "< Prev    2/5    Next >"
│
├── TextContentView             refreshHint=Quality
│   ├── text_, offset_, font_, lineHeight_
│   ├── 集成 TextLayout 排版引擎
│   └── 阅读器正文区域
│
└── OverlayView (特殊)          refreshHint=Quality
    ├── backingStore_ (framebuffer 快照)
    ├── show() / hide()
    │
    ├── DropdownView
    │   ├── items[], selectedIndex_
    │   └── 锚定在某个 View 下方弹出
    │
    └── ModalDialogView
        ├── title_, message_, buttons_[]
        └── 居中弹出, 半透明遮罩
```

---

## 6. 目录结构

```
Parchment/
├── main/
│   ├── main.cpp                       C++ 入口
│   ├── board.c / board.h              板级定义 (保持 C)
│   └── controllers/                   应用层 ViewController
│       ├── BootViewController.h/.cpp
│       ├── LibraryViewController.h/.cpp
│       ├── ReaderViewController.h/.cpp
│       ├── TocViewController.h/.cpp
│       └── SettingsViewController.h/.cpp
│
├── components/
│   ├── ink_ui/                        新 C++ UI 框架
│   │   ├── CMakeLists.txt
│   │   ├── include/ink_ui/
│   │   │   ├── core/
│   │   │   │   ├── Geometry.h
│   │   │   │   ├── View.h
│   │   │   │   ├── Canvas.h
│   │   │   │   ├── RenderEngine.h
│   │   │   │   ├── RefreshPolicy.h
│   │   │   │   ├── FlexLayout.h
│   │   │   │   ├── Event.h
│   │   │   │   ├── GestureRecognizer.h
│   │   │   │   ├── ViewController.h
│   │   │   │   ├── NavigationController.h
│   │   │   │   └── Application.h
│   │   │   ├── views/
│   │   │   │   ├── TextLabel.h
│   │   │   │   ├── IconView.h
│   │   │   │   ├── ButtonView.h
│   │   │   │   ├── HeaderView.h
│   │   │   │   ├── ProgressBarView.h
│   │   │   │   ├── SeparatorView.h
│   │   │   │   ├── PagedListView.h
│   │   │   │   ├── PageIndicatorView.h
│   │   │   │   ├── TextContentView.h
│   │   │   │   ├── OverlayView.h
│   │   │   │   ├── DropdownView.h
│   │   │   │   └── ModalDialogView.h
│   │   │   ├── text/
│   │   │   │   ├── FontManager.h
│   │   │   │   └── TextLayout.h
│   │   │   └── InkUI.h               统一入口头文件
│   │   └── src/
│   │       ├── core/    (对应 include 结构)
│   │       ├── views/
│   │       └── text/
│   │
│   ├── epd_driver/                    加 C++ wrapper EpdDriver.h
│   ├── epdiy/                         不动 (C 库)
│   ├── gt911/                         不动 (C 库)
│   ├── sd_storage/                    不动 (C 库)
│   │
│   └── ui_core/                       旧框架, 迁移完成后删除
│
├── fonts_data/                        .pfnt 字体文件 (不变)
├── tools/                             转换脚本 (不变)
└── docs/
    └── ink-ui-architecture.md         本文档
```

---

## 7. CMake 配置

### 7.1 ink_ui 组件

```cmake
# components/ink_ui/CMakeLists.txt
idf_component_register(
    SRCS
        "src/core/View.cpp"
        "src/core/Canvas.cpp"
        "src/core/RenderEngine.cpp"
        "src/core/RefreshPolicy.cpp"
        "src/core/FlexLayout.cpp"
        "src/core/GestureRecognizer.cpp"
        "src/core/ViewController.cpp"
        "src/core/NavigationController.cpp"
        "src/core/Application.cpp"
        "src/views/TextLabel.cpp"
        "src/views/IconView.cpp"
        "src/views/ButtonView.cpp"
        "src/views/HeaderView.cpp"
        "src/views/ProgressBarView.cpp"
        "src/views/SeparatorView.cpp"
        "src/views/PagedListView.cpp"
        "src/views/PageIndicatorView.cpp"
        "src/views/TextContentView.cpp"
        "src/views/OverlayView.cpp"
        "src/views/DropdownView.cpp"
        "src/views/ModalDialogView.cpp"
        "src/text/FontManager.cpp"
        "src/text/TextLayout.cpp"
    INCLUDE_DIRS "include"
    REQUIRES epd_driver gt911 esp_timer freertos esp_littlefs
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall -Wextra
)
```

### 7.2 main

```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS
        "main.cpp"
        "board.c"
        "controllers/BootViewController.cpp"
        "controllers/LibraryViewController.cpp"
        "controllers/ReaderViewController.cpp"
        "controllers/TocViewController.cpp"
        "controllers/SettingsViewController.cpp"
    INCLUDE_DIRS "."
    REQUIRES ink_ui epd_driver gt911 sd_storage
)

littlefs_create_partition_image(fonts ../fonts_data FLASH_IN_PROJECT)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fno-exceptions -fno-rtti
)
```

---

## 8. 实现阶段

### 阶段 0: 基础设施

**目标**: C++ 编译通过, 能链接 epdiy

**任务**:
1. 创建 `components/ink_ui/` 目录结构和 CMakeLists.txt
2. 实现 `Geometry.h` (Point, Size, Rect, Insets)
3. 创建 `EpdDriver.h` C++ wrapper (调用现有 C 函数)
4. `main.cpp` (替换 main.c): 最小化验证, 仅初始化硬件
5. 配置 C++17 编译选项, 验证 `idf.py build` 通过

**验证**: `idf.py build` 成功, 固件大小合理

### 阶段 1: 核心渲染

**目标**: View 树 → Canvas → framebuffer → 墨水屏显示

**任务**:
1. 实现 `View` 基类 (frame, subviews, dirty flags, 树操作)
2. 实现 `Canvas` (裁剪, 坐标变换, fillRect, drawRect)
3. 实现 `RenderEngine` 基础版 (收集脏区域, 重绘, 单区域刷新)
4. 实现 `FlexLayout` 算法
5. 编写验证代码: 创建 View 树, 渲染灰色矩形到屏幕

**验证**: 墨水屏上显示白色背景 + 灰色矩形, 且只刷新了矩形区域

### 阶段 2: 交互系统

**目标**: 触摸输入 → 事件分发 → View 响应

**任务**:
1. 实现 `Event` 类型定义
2. 实现 `GestureRecognizer` (从 ui_touch.c 迁移逻辑)
3. 实现 `View::hitTest()` 递归命中测试
4. 实现 `ViewController` + `NavigationController`
5. 实现 `Application` 主循环

**验证**: 触摸屏幕上的矩形区域有 log 输出, 触摸其他区域无输出

### 阶段 3: 基础 Views

**目标**: 能组合出完整的页面 UI

**任务**:
1. `TextLabel` — 单行/多行文字显示
2. `IconView` — 32x32 图标
3. `ButtonView` — 可点击按钮 (icon + text, tap callback)
4. `HeaderView` — 组合: leftIcon + title + rightIcon, FlexBox Row
5. `ProgressBarView` — 灰度进度条
6. `SeparatorView` — 分隔线
7. `PageIndicatorView` — 翻页导航 ("< 2/5 >")

**验证**: 用基础 View 组合出 Boot 页面 (logo + 版本号 + 进度条)

### 阶段 4: 复合 Views + 文字系统

**目标**: 完整的列表、浮层、文字排版

**任务**:
1. `PagedListView` — 分页列表, DataSource 接口
2. `OverlayView` — backing store 机制
3. `DropdownView` — 锚定下拉菜单
4. `ModalDialogView` — 居中模态弹窗
5. `FontManager` — 从 ui_font.c 重写: LittleFS 加载, UI/阅读字体管理
6. `TextLayout` — 从 ui_text.c 重写: CJK 断行, 禁则处理, 分页
7. `TextContentView` — 阅读区域 (集成 TextLayout)
8. 实现 `RenderEngine` 完整版: 多区域, 合并策略, 残影管理

**验证**: 分页列表翻页正常; 下拉菜单出现/消失无残留; 文字排版正确

### 阶段 5: 应用 ViewControllers

**目标**: 迁移所有页面, 删除旧框架

**任务**:
1. `BootViewController` — 替换 page_boot.c
2. `LibraryViewController` — 替换 page_library.c
3. `ReaderViewController` — 新增 (翻页阅读)
4. `TocViewController` — 新增 (目录)
5. `SettingsViewController` — 新增 (设置)
6. 删除 `components/ui_core/` 旧框架
7. 清理: 删除旧的 `main/pages/` 目录

**验证**: 完整应用流程: 启动 → 书库 → 阅读 → 目录 → 设置, 所有页面可用

---

## 9. 迁移策略

```
时间线:
──────────────────────────────────────────────────

阶段 0-1:  ink_ui 组件创建, ui_core 不动
           main.cpp 临时测试代码
           两套框架共存, 不冲突

阶段 2-3:  ink_ui 基本可用
           main.cpp 切换到 ink_ui
           BootViewController 替换 page_boot.c
           page_library.c 暂时不用

阶段 4-5:  逐个 ViewController 替换 page_*.c
           每替换一个, 删除对应旧 page
           最后一个 page 迁完 → 删除 ui_core/

完成:      只剩 ink_ui, 无 ui_core
           main.cpp 干净的 C++ 入口
```

**原则**:
- 旧 `ui_core` (C) 和新 `ink_ui` (C++) 是独立的 ESP-IDF component, 不冲突
- 迁移期间 `main.cpp` 只引用一个框架, 不存在半 C 半 C++ 混用
- 每个阶段结束都能在屏幕上看到东西
- 每个阶段都可以 `idf.py flash` 到设备上验证

---

## 10. 内存预算

```
PSRAM 8MB:
├── epdiy framebuffers      ~1.0 MB (front + back + diff)
├── UI fonts (常驻)          ~1.5 MB
├── Reading font (按需)      ~5-7 MB
├── View tree (~100 View)   ~12 KB
├── Backing store (浮层)    ~30-60 KB (临时)
├── FlexBox 计算临时数据    ~1 KB
└── 余量                    ~0.5 MB

Flash 16MB:
├── firmware                ~500-600 KB (C++ 略大于 C)
├── fonts partition          15.6 MB
└── 余量                    充足
```

C++ 额外开销:
- vtable: 每个类一份 (Flash 中), ~15 个类 × ~40 bytes = ~600 bytes
- `std::unique_ptr`: 零运行时开销 (编译期)
- `std::vector`: 每个实例 12 bytes (ptr + size + capacity), PSRAM 分配
- 无异常/RTTI: 无隐藏开销

---

## 11. 编码规范

- **类名**: PascalCase (`HeaderView`, `RenderEngine`)
- **方法名**: camelCase (`setNeedsDisplay`, `onDraw`)
- **成员变量**: 尾部下划线 (`frame_`, `subviews_`)
- **常量**: UPPER_SNAKE (`MAX_DIRTY_REGIONS`) 或 constexpr camelCase
- **文件名**: PascalCase.h/.cpp (`RenderEngine.cpp`)
- **命名空间**: `ink` (所有框架类)
- **注释**: 公共 API 用 Doxygen `///`, 内部实现用 `//`
- **语言**: 注释使用中文, 技术术语保留英文
- **头文件保护**: `#pragma once`

---

## 12. 最终 main.cpp

```cpp
#include <ink_ui/InkUI.h>
#include "board.h"
#include "controllers/BootViewController.h"

extern "C" void app_main() {
    board_init();

    ink::Application app;
    app.init();   // EPD + Touch + Font + SD + 渲染引擎

    auto bootVC = std::make_unique<BootViewController>();
    app.navigator().push(std::move(bootVC));

    app.run();    // 主循环, 永不返回
}
```

---

## 附录 A: 当前代码文件清单 (待迁移)

| 旧文件 | 行数 | 迁移目标 |
|--------|------|---------|
| ui_core/ui_core.c | 85 | Application.cpp |
| ui_core/ui_page.c | 116 | NavigationController.cpp |
| ui_core/ui_render.c | 115 | RenderEngine.cpp |
| ui_core/ui_canvas.c | 423 | Canvas.cpp |
| ui_core/ui_widget.c | 574 | views/*.cpp (拆分) |
| ui_core/ui_touch.c | 324 | GestureRecognizer.cpp |
| ui_core/ui_event.c | 62 | Event.h + Application.cpp |
| ui_core/ui_text.c | 284 | TextLayout.cpp |
| ui_core/ui_font.c | 259 | FontManager.cpp |
| ui_core/ui_icon.c | 71 | IconView.cpp |
| pages/page_boot.c | 121 | BootViewController.cpp |
| pages/page_library.c | 395 | LibraryViewController.cpp |
| **合计** | **~2829** | |

## 附录 B: 关键约定

- **坐标系**: 逻辑 540x960 portrait, 物理 960x540 landscape
- **像素映射**: `px = ly, py = 539 - lx` (转置+X翻转)
- **4bpp 灰度**: 0x00=黑, 0xFF=白, 每字节 2 像素
- **EPD 电源**: 必须显式 poweron/poweroff, epdiy 不自动管理
- **触摸坐标**: GT911 直通, 无需变换
