<p align="center">
  <img src="docs/media/icon.png" width="88" height="88" alt="Window Scatter">
</p>

<h1 align="center">Window Scatter</h1>

<p align="center">面向 Windows 11 独立设计的窗口总览工具。</p>

<p align="center">
  <a href="https://github.com/SinCircle/windhawk-window-scatter/releases/latest"><img src="https://img.shields.io/github/v/release/SinCircle/windhawk-window-scatter?style=flat-square&amp;color=3878df" alt="Latest release"></a>
  <img src="https://img.shields.io/badge/Windows_11-x64-555f72?style=flat-square" alt="Windows 11 x64">
</p>

<p align="center">
  <a href="https://github.com/SinCircle/windhawk-window-scatter/releases/latest/download/WindowScatter.exe"><strong>下载独立版</strong></a> ·
  <a href="#快速开始">快速开始</a> ·
  <a href="#功能说明">功能说明</a> ·
  <a href="docs/CHANGELOG.md">更新记录</a> ·
  <a href="docs/development.md">编译说明</a>
</p>

<p align="center">
  <img src="docs/media/overview.gif" width="1100" alt="窗口展开、选择和切换">
</p>

## 快速开始

Window Scatter 围绕窗口的空间关系和连续操作设计布局与交互，保留窗口之间的大小关系，用实时预览和连续动画衔接展开、选窗与恢复。支持鼠标、三指手势和多显示器，提供独立应用和 Windhawk 模组。

1. 下载 [WindowScatter.exe](https://github.com/SinCircle/windhawk-window-scatter/releases/latest/download/WindowScatter.exe)，双击启动。程序在后台运行，没有托盘图标，无需安装额外运行库。
2. 按 <kbd>Win</kbd> + <kbd>Tab</kbd> 打开总览，点击窗口切换。
3. 再次双击 EXE 打开设置，可调整动画、内存模式和开机启动。退出程序也在这里操作。

> 使用独立版前，请先停用 Windhawk 中的同名模组。

<details>
<summary><strong>使用 Windhawk 安装</strong></summary>

1. 安装 [Windhawk](https://windhawk.net/)，打开“创建新模组”。
2. 将 [window-scatter.wh.cpp](window-scatter.wh.cpp) 的内容粘贴到编辑器，保存、编译并启用。
3. 在模组设置中调整动画与内存模式。

</details>

## 功能说明

### 窗口布局

窗口按原比例缩小，中心之间的目标距离也按同一比例缩小，再调整位置以保持间距。预览中的内容会实时更新。

每次重新打开都会重新计算布局，包括动画未结束就收回、又立即展开的情况。总览内窗口变化时，允许较大范围调整，让空位得到利用，同时避免其他预览在重排中相互穿过。总览不会改变实际窗口的位置和大小。

### 键盘、鼠标和触摸板操作

| 操作 | 结果 |
| --- | --- |
| <kbd>Win</kbd> + <kbd>Tab</kbd> | 打开或关闭总览，松开按键后保持打开 |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>←</kbd> <kbd>→</kbd> | 优先选择相邻行或列中重叠更多的窗口；上下可进出最小化栏，支持跨屏 |
| <kbd>Enter</kbd> | 切换到选中窗口，最小化窗口会恢复 |
| 三指短上滑后松手 | 打开总览，停留浏览 |
| 三指上滑后继续移动 | 按窗口所在方向选中，松手切换；支持斜向选择 |
| 总览中三指下滑 | 退出总览 |
| 点击窗口 | 切换到该窗口 |
| <kbd>Esc</kbd> / 点击背景 | 取消，回到先前窗口 |
| <kbd>Shift</kbd> + <kbd>Win</kbd> + <kbd>Tab</kbd> | 打开 Windows 原生任务视图 |

触摸板的三指上滑需要在 Windows 设置中配置为“任务视图”。

### 最小化与恢复

<p align="center">
  <img src="docs/media/minimize.gif" width="1100" alt="在总览中最小化窗口，再从底部选择恢复">
</p>

最小化的窗口显示在底部，按任务栏应用顺序排列。点击即可恢复；取消总览不会恢复这些窗口。

悬停预览时，右上角显示 **最小化** 和 **关闭** 按钮。最小化后总览保持打开，预览移到底部。关闭窗口时，应用仍可提示保存未完成的内容。

### 标题、选中框和动画

<p align="center">
  <img src="docs/media/detail.png" width="490" alt="窗口标题、选中框、最小化和关闭按钮">
</p>

标题显示在预览下方，选中框颜色跟随 Windows 主题色。所有预览使用统一圆角。

动画时长可在设置中调整，设为 0 可关闭动画。动画未结束时再次操作，会从当前画面继续展开或收回。

### 多显示器

各屏幕上的窗口分别排列，任务栏保持可见。支持不同分辨率、缩放比例和显示器位置。

### 内存模式

| 内存模式 | 说明 |
| --- | --- |
| **自动 · 默认** | 缓存绘图资源以便快速打开，内存不足时释放 |
| **常驻** | 保留绘图资源，优先打开速度 |
| **省内存** | 关闭总览后释放绘图资源，下次打开重新加载 |

## 设计与参考

macOS 和 Linux 桌面的多任务总览界面是本项目的交互参考。Window Scatter 根据自身的设计目标，独立探索窗口的空间布局、手势选窗、连续重排与最小化恢复，让窗口管理适应 Windows 上的实际使用方式。

## 更多说明

[常见问题](docs/usage.md) · [性能测试](docs/performance.md) · [编译说明](docs/development.md) · [更新记录](docs/CHANGELOG.md) · [反馈问题](https://github.com/SinCircle/windhawk-window-scatter/issues)

作者 **SinCircle** · [来源与许可](docs/development.md#来源与许可)
