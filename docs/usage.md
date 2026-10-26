# 常见问题

[返回首页](../README.md)

## 支持哪些系统？

独立版面向 Windows 11 x64，已在内部版本 26200.9457 上验证系统入口连接与手势。其他 Windows 版本尚未逐一验证。Windhawk 版使用 Windhawk 1.7.3 验证。

程序依赖 Windows 内部接口。已验证的系统可离线启动；其他版本首次适配时可能从微软符号服务器下载匹配符号，之后使用缓存。接口不兼容时会在设置中提示，不接管系统任务视图。

## 如何打开设置？

手动启动程序即打开设置；程序已在运行时，再次双击同一个 EXE 也会打开设置。也可以运行 `WindowScatter.exe --settings`。开机自启使用 `--startup`，在后台运行，不弹出设置。

各项说明集中在旁边的「?」中。右上角显示连接状态，点击可查看详情；连接异常时可在其中重新连接。

## 如何退出？

在设置中点击“退出程序”，或运行 `WindowScatter.exe --exit`。只关闭设置面板不会退出程序。

## 设置保存在哪里？

设置位于 `HKCU\Software\SinCircle\WindowScatter`；运行组件与符号缓存位于 `%LOCALAPPDATA%\WindowScatter`。程序会将内置连接组件释放到该目录。

开机启动仅注册当前用户启动项。在下载目录、临时目录或浏览器缓存目录中开启时，会先提醒将程序移到固定位置，也可以确认后继续开启。取消提醒不会写入启动项。启用后若移动 EXE，请在新位置关闭并重新开启该选项。

## 默认设置是什么？

内存模式为自动，开机启动关闭。独立版默认动画时长为 666 ms，Windhawk 版为 320 ms。升级保留已有设置。

## 为什么部分最小化窗口没有画面？

部分应用没有可用的 DWM 保留画面，此时显示占位图，仍可通过标题识别并选择恢复。

## 如何校验下载文件或报告问题？

[Release](https://github.com/SinCircle/windhawk-window-scatter/releases/latest) 附件中的 `SHA256SUMS.txt` 提供独立版 EXE 和模组源码的 SHA-256 校验值。

[报告问题](https://github.com/SinCircle/windhawk-window-scatter/issues)时，请附上 Windows 版本、显示器布局与缩放比例、程序版本和复现步骤。
