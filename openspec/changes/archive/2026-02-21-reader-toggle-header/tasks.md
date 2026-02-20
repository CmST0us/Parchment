## 1. 移除冗余 UI 元素

- [x] 1.1 从 `ReaderViewController.h` 中移除 `headerLabel_` 成员变量
- [x] 1.2 从 `ReaderViewController.cpp` `viewDidLoad()` 中移除 `headerContainer` 和 `headerLabel_` 的创建代码
- [x] 1.3 移除文件加载失败时对 `headerLabel_` 的引用（改为 `ESP_LOGE` 日志）

## 2. Header 显隐控制

- [x] 2.1 在 `ReaderViewController.h` 中添加 `headerView_` raw pointer 成员
- [x] 2.2 在 `viewDidLoad()` 中保存 HeaderView raw pointer（在 `std::move` 之前）
- [x] 2.3 在 `viewDidLoad()` 末尾调用 `headerView_->setHidden(true)` 使 Header 默认隐藏

## 3. 中间区域 Tap 切换

- [x] 3.1 在 ReaderTouchView 的中间 1/3 区域 tap 处理中添加 `onTapMiddle_` 回调调用
- [x] 3.2 在 `ReaderViewController` 中设置 `onTapMiddle_` 回调，实现 `headerView_->setHidden(!headerView_->isHidden())` + `view_->setNeedsLayout()`
