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
  <img src="docs/media/overview.gif" width="1100" alt="展开总览，用方向键选窗，按 Enter 切换并恢复最小化窗口">
</p>

## 快速开始

将打开的窗口按原比例展开，提供实时预览、方向键选窗和触摸板手势。支持多显示器，可使用独立应用或 Windhawk 模组。

1. 下载 [WindowScatter.exe](https://github.com/SinCircle/windhawk-window-scatter/releases/latest/download/WindowScatter.exe)，双击启动。程序在后台运行，没有托盘图标，无需安装额外运行库。
2. 按 <kbd>Win</kbd> + <kbd>Tab</kbd> 打开总览，用方向键选择窗口，按 <kbd>Enter</kbd> 切换；也可以直接点击。
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

相互重叠的窗口会展开为互不遮挡的预览。窗口按统一比例缩小，保留大小关系，并尽量保持原来的相对位置。横向、纵向和不同尺寸的窗口可以混合排布，预览内容实时更新。

每次打开都会按当前窗口重新计算布局。最小化、关闭或新增窗口后，其他预览会连续移动、利用空出的空间，并避免在重排中相互穿过。总览不会改变实际窗口的位置和大小。

覆盖最大化、半屏、四分屏，以及不同程度的窗口遮挡：展开后找到需要的窗口，直接回到要继续的工作。

<table>
  <tr>
    <td align="center"><strong>最大化工作时，找回阅读笔记</strong><br><img src="docs/media/layout-fullscreen.gif" width="520" alt="最大化编辑器挡住其他窗口，展开后用方向键找到阅读笔记，切换后停留在笔记窗口"></td>
    <td align="center"><strong>左右分屏时，打开项目文件</strong><br><img src="docs/media/layout-halves.gif" width="520" alt="编辑器与笔记左右分屏，展开总览，从底栏恢复项目文件并停留在文件窗口"></td>
  </tr>
  <tr>
    <td align="center"><strong>四分屏下，找回被盖住的笔记</strong><br><img src="docs/media/layout-quarters.gif" width="520" alt="四个平铺窗口盖住中央的笔记，展开后连续用方向键定位笔记并切到前台"></td>
    <td align="center"><strong>小窗遮挡时，回到编辑器</strong><br><img src="docs/media/layout-floating.gif" width="520" alt="笔记小窗遮住部分代码，展开后选择编辑器，结束时完整代码区域重新露出"></td>
  </tr>
  <tr>
    <td align="center"><strong>横竖混排中，找到阅读笔记</strong><br><img src="docs/media/layout-mixed.gif" width="520" alt="六个横竖比例不同的窗口交错遮挡，展开后找到纵向阅读笔记并切到前台"></td>
    <td align="center"><strong>参考窗交错时，找到项目素材</strong><br><img src="docs/media/layout-varied.gif" width="520" alt="主画布与参考窗口盖住项目文件，展开后找到素材窗口，结束时文件列表清晰可见"></td>
  </tr>
  <tr>
    <td align="center"><strong>十二个窗口里，找到发布文件</strong><br><img src="docs/media/layout-dense.gif" width="520" alt="十二个窗口密集堆叠，展开后连续按方向键找到发布文件，切换后停留在发布素材窗口"></td>
    <td align="center"><strong>两组项目中，找回本周日程</strong><br><img src="docs/media/layout-clusters.gif" width="520" alt="两组项目窗口相互重叠，展开后连续用方向键进入底栏，恢复本周日历并停留在日程窗口"></td>
  </tr>
</table>

### 键盘、鼠标和触摸板操作

| 操作 | 结果 |
| --- | --- |
| <kbd>Win</kbd> + <kbd>Tab</kbd> | 打开或关闭总览，松开按键后保持打开 |
| <kbd>↑</kbd> <kbd>↓</kbd> <kbd>←</kbd> <kbd>→</kbd> | 按窗口所在方向移动选中框，支持跨屏 |
| <kbd>Enter</kbd> | 切换到选中窗口，最小化窗口会恢复 |
| 三指短上滑后松手 | 打开总览，停留浏览 |
| 三指上滑后继续移动 | 按窗口所在方向选中，松手切换；支持斜向选择 |
| 总览中三指下滑 | 退出总览 |
| 点击窗口 | 切换到该窗口 |
| <kbd>Esc</kbd> / 点击背景 | 取消，回到先前窗口 |
| <kbd>Shift</kbd> + <kbd>Win</kbd> + <kbd>Tab</kbd> | 打开 Windows 原生任务视图 |

左右键在普通窗口区或最小化栏内选择，上下键可在两者之间切换。方向键选中后，选中框会保持；移动鼠标后恢复悬停选择。

触摸板的三指上滑需要在 Windows 设置中配置为“任务视图”。

### 最小化与恢复

<p align="center">
  <img src="docs/media/minimize.gif" width="1100" alt="在总览中最小化窗口，再从底部选择恢复">
</p>

最小化的窗口显示在底部，按任务栏应用顺序排列。点击或用方向键选中后按 <kbd>Enter</kbd> 即可恢复；取消总览不会恢复这些窗口。

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
