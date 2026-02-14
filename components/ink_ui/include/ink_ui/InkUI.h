/**
 * @file InkUI.h
 * @brief InkUI 框架统一入口头文件。
 *
 * 包含所有核心类型的头文件。
 */

#pragma once

#include "ink_ui/core/Geometry.h"
#include "ink_ui/core/Event.h"
#include "ink_ui/core/FlexLayout.h"
#include "ink_ui/core/EpdDriver.h"
#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"
#include "ink_ui/core/RenderEngine.h"
#include "ink_ui/core/GestureRecognizer.h"
#include "ink_ui/core/ViewController.h"
#include "ink_ui/core/NavigationController.h"
#include "ink_ui/core/Application.h"

// Views (widget 组件)
#include "ink_ui/views/TextLabel.h"
#include "ink_ui/views/IconView.h"
#include "ink_ui/views/ButtonView.h"
#include "ink_ui/views/HeaderView.h"
#include "ink_ui/views/ProgressBarView.h"
#include "ink_ui/views/SeparatorView.h"
#include "ink_ui/views/PageIndicatorView.h"
