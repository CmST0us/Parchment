## 1. TextLabel 文字显示

- [x] 1.1 创建 `include/ink_ui/views/TextLabel.h`: 声明 TextLabel 类，属性（text_, font_, color_, alignment_, maxLines_），setter 方法，intrinsicSize() 覆写
- [x] 1.2 创建 `src/views/TextLabel.cpp`: 实现 onDraw（对齐计算 + 文字绘制 + 多行支持），setText 去重检查，intrinsicSize 基于 measureText 计算
- [x] 1.3 验证 TextLabel 编译通过，在 main.cpp 中临时创建 TextLabel 确认文字能正确显示

## 2. IconView 图标显示

- [x] 2.1 创建 `include/ink_ui/views/IconView.h`: 声明 IconView 类，属性（iconData_, tintColor_），setIcon 方法，intrinsicSize() 固定返回 {32, 32}
- [x] 2.2 创建 `src/views/IconView.cpp`: 实现 onDraw（居中计算 + drawBitmapFg），setIcon 触发重绘

## 3. ButtonView 按钮

- [x] 3.1 创建 `include/ink_ui/views/ButtonView.h`: 声明 ButtonStyle 枚举和 ButtonView 类，属性（label_, font_, icon_, style_, onTap_, enabled_），setter 方法
- [x] 3.2 创建 `src/views/ButtonView.cpp`: 实现 onDraw（三种样式绘制 + 禁用状态），onTouchEvent 处理 Tap，intrinsicSize 计算
- [x] 3.3 验证 ButtonView 触摸响应，在 DemoVC 中添加按钮确认 Tap 回调能触发

## 4. HeaderView 标题栏

- [x] 4.1 创建 `include/ink_ui/views/HeaderView.h`: 声明 HeaderView 类，setTitle/setLeftIcon/setRightIcon/setFont 方法
- [x] 4.2 创建 `src/views/HeaderView.cpp`: 实现构造逻辑（创建内部 TextLabel + ButtonView 子 View），FlexBox Row 布局，图标白色 tint

## 5. ProgressBarView 进度条

- [x] 5.1 创建 `include/ink_ui/views/ProgressBarView.h`: 声明 ProgressBarView 类，属性（value_, trackColor_, fillColor_, trackHeight_），setValue 方法
- [x] 5.2 创建 `src/views/ProgressBarView.cpp`: 实现 onDraw（轨道 + 填充矩形绘制），setValue 范围裁剪 + 去重检查，intrinsicSize

## 6. SeparatorView 分隔线

- [x] 6.1 创建 `include/ink_ui/views/SeparatorView.h`: 声明 Orientation 枚举和 SeparatorView 类，属性（orientation_, lineColor_, thickness_）
- [x] 6.2 创建 `src/views/SeparatorView.cpp`: 实现 onDraw（水平/垂直线居中绘制），intrinsicSize 根据方向返回

## 7. PageIndicatorView 翻页指示器

- [x] 7.1 创建 `include/ink_ui/views/PageIndicatorView.h`: 声明 PageIndicatorView 类，属性（currentPage_, totalPages_, onPageChange_），setPage 方法
- [x] 7.2 创建 `src/views/PageIndicatorView.cpp`: 实现构造逻辑（创建 prevButton + pageLabel + nextButton），翻页逻辑 + 按钮禁用状态管理，FlexBox Row 布局

## 8. 集成与构建

- [x] 8.1 更新 `components/ink_ui/CMakeLists.txt`: 添加 7 个新 .cpp 源文件
- [x] 8.2 更新 `include/ink_ui/InkUI.h`: 添加 7 个新 widget 头文件的 include
- [x] 8.3 重写 `main.cpp`: 用 TextLabel + HeaderView + ProgressBarView + SeparatorView 组合出 Boot 页面验证所有 widget 工作正常
- [x] 8.4 执行 `idf.py build` 确认编译通过，检查固件大小合理
