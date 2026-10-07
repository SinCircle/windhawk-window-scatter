# Window Scatter

为 Windows 11 提供类似 macOS Exposé 的窗口总览。Windhawk 模组，当前版本 **0.3.5**。

## 功能

- 按 **Win+Tab** 打开或关闭总览，松开按键后保持打开；点击窗口切换，按 Esc 或点击背景取消。
- **Shift+Win+Tab** 打开系统任务视图；**Ctrl+Alt+Space** 和托盘图标也可触发总览。
- 窗口按实际比例排列，使用实时画面、圆角、标题和非线性动画。
- DirectComposition 执行动画，显示器刷新率由 DWM 驱动；任务栏保持可见。
- 默认动画时长 320 ms，可调整时长和圆角。

## 安装

1. 安装 [Windhawk](https://windhawk.net/)。本版本使用 Windhawk 1.7.3 验证。
2. 打开本仓库的 [window-scatter.wh.cpp](window-scatter.wh.cpp)，复制完整源码。
3. Windhawk → 创建新模组 → 全选替换代码 → 编译并启用。

关闭模组可恢复原生 Win+Tab。需要 Windows 11；当前实现不展示已最小化窗口，也不管理虚拟桌面。它使用未公开的 DWM 接口，系统更新可能需要适配；受保护内容可能无法预览。

## 本地编译

在仓库目录运行 PowerShell，按实际安装位置指定 Windhawk：

```powershell
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk'
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk' -Architecture i686
```

生成文件位于 `build/`。脚本只编译；安装仍使用 Windhawk 编辑器中的源码。

## 来源

作者：SinCircle。共享窗口画面实现参考 [ADeltaX 的 DWM 接口研究](https://gist.github.com/ADeltaX/aea6aac248604d0cb7d423a61b06e247)。独立进程启动代码来自 [Ramen Software 的官方示例](https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process)，源码保留相应署名与链接。原始模组未指定整体许可证，本仓库暂未另行指定。
