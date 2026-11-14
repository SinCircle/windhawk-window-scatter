# 预览素材说明

[返回首页](../README.md)

当前动图恢复原先的模拟应用窗口，展示虚构的设计稿、代码、笔记、文件、音乐和日历。所有演示界面使用浅色主题，图标为对应演示应用绘制，不使用真实浏览器或编辑器的界面截图。

演示窗口在当前桌面限定的 1440×900 区域内运行。窗口界面由演示程序绘制；窗口枚举之后的布局、实时预览、展开、重排、关闭、最小化、恢复、圆角和阴影由当前版本的生产代码与 Windows 合成器执行，没有另写动画模型。每段结束后关闭演示窗口，并恢复先前的前台窗口。

模拟任务栏、鼠标和按键标注用于展示操作；录制不改动用户的窗口布局或系统壁纸。背景为 Windows 随附壁纸，权利归 Microsoft。设置截图来自本项目实际的 WebView2 设置页。

GIF 保持 16∶10 比例，以 1200×750、50 fps 编码，按录制时的原始速度播放。GIF 编码帧率不代表产品性能；测量范围见[性能与验证](performance.md)。

<details>
<summary>1.0.5 历史素材来源与许可</summary>

1.0.5 的预览使用真实软件界面截图和应用图标，替换原来的手绘示意界面。窗口外框的宽高比直接取自截图；展开、重排和最小化栏中的预览保持该比例，不在任意尺寸的外框中塞入截图或用空白填满。

这些 GIF 是离线生成的交互示意。数值布局直接调用本版本的 `App::Arrange`、`StableLayout`，动画插值使用相同的 `Curve`；截图、标题、光标、控件和演示任务栏由离线画面合成。背景、阴影及交接效果是示意，不经过实际 DWM 合成器。生成时没有控制桌面或键鼠，也没有录制正在运行的第三方应用。50 fps 是 GIF 的编码帧率，不是产品帧率测量；实测结果见[性能与验证](performance.md)。

设置截图来自本版本的内嵌 WebView2 设置页。素材为演示数据或公开图片，没有录制用户私人窗口。

| 界面 | 来源 | 作者与许可 |
| --- | --- | --- |
| Firefox 中的 Wikipedia | [原图与完整来源](https://commons.wikimedia.org/wiki/File:Firefox_133.0.3_on_Windows_11_screenshot.webp) | 截图 VulcanSphere；Mozilla、Wikimedia 及原图列明的内容作者；Wikipedia 内容 CC BY-SA 4.0，其他组成部分按原页许可 |
| Visual Studio Code | [VS Code Screenshot.png](https://commons.wikimedia.org/wiki/File:VS_Code_Screenshot.png) | Cycling2，CC0 1.0 |
| Windows 任务管理器 | [原图](https://commons.wikimedia.org/wiki/File:File_Explorer_App_windows_11.png) | Hstoops 上传，原页标注 CC0 1.0；文件名写作 File Explorer，实际画面是任务管理器 |
| VLC 关于窗口 | [VLC Media Player 3.0.20.png](https://commons.wikimedia.org/wiki/File:VLC_Media_Player_3.0.20.png) | MayconHVAM，CC0 1.0 |
| Window Scatter 设置 | 本项目 1.0.5 | SinCircle |

截图在演示中等比缩放；VS Code 原图裁去桌面任务栏，任务管理器裁去桌面背景，VLC 取实际「关于」窗口。组合生成的 GIF 按 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) 提供，组成素材及软件商标仍按各自权利归属。此项素材许可不改变项目源代码的 MIT 许可。

Firefox、VLC 图标分别来自 Mozilla 与 VideoLAN 官方站点；VS Code、任务管理器和 Window Scatter 图标取自相应应用资源。任务栏开关对比用于说明「进入总览时折叠」与「保持原位」的区别，两个版本使用相同窗口场景。

</details>
