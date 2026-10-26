# 构建与开发

[返回首页](../README.md)

所有命令在仓库根目录执行。

独立版使用 LLVM-MinGW 的 `clang++.exe` 和 Windows SDK 的 `rc.exe` 编译：

```powershell
.\build-native.ps1 -Compiler 'C:\path\to\llvm-mingw\bin\clang++.exe'
```

生成的 `build/native/WindowScatter.exe` 可单独分发。脚本将连接组件、设置页 HTML/CSS/JavaScript、官方 WebView2 Loader、图标和第三方许可嵌入 EXE，并静态链接 C/C++ 运行库。设置界面使用用户机器上的 Microsoft Edge WebView2 Runtime。可以通过 `-ResourceCompiler` 显式指定 `rc.exe`，通过 `-OutputDirectory` 更改输出位置。

首次构建从 NuGet 下载固定版本 `Microsoft.Web.WebView2 1.0.4314.35` 并校验 SHA-256，缓存于 `build/deps/`；后续构建可复用缓存。版本和校验值见 `native/restore-webview2.ps1`。

设置页由 WebView2 统一合成，不再使用逐控件的 GDI 自绘。只有打开设置时才创建浏览器环境；关闭面板时销毁 Controller 和环境。`--startup` 不加载前端。页面内容来自 EXE 内嵌资源，无外部脚本、字体或请求；设置命令经过来源、类型和范围校验，再交回主线程处理。原生后端保存成功后才更新有效设置，失败时页面回退。自启目录提醒仍使用 Windows 原生对话框，且在 WebView2 回调返回后打开。

内置自检使用临时配置项检查保存、默认值、开机启动命令和组件校验，不更改用户的实际开机启动项：

```powershell
.\build\native\WindowScatter.exe --self-test --report "$PWD\self-test.txt"
```

在仓库目录运行 PowerShell，按实际安装位置指定 Windhawk：

```powershell
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk'
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk' -Architecture i686
```

生成文件位于 `build/`。脚本只编译；安装仍使用 Windhawk 编辑器中的源码。

## 来源与许可

作者：SinCircle。共享窗口画面实现参考 [ADeltaX 的 DWM 接口研究](https://gist.github.com/ADeltaX/aea6aac248604d0cb7d423a61b06e247)。独立进程启动代码来自 [Ramen Software 的官方示例](https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process)，源码保留相应署名与链接。原始模组未指定整体许可证，本仓库暂未另行指定。

独立版系统连接组件静态链接 [MinHook 1.3.4](https://github.com/TsudaKageyu/minhook/tree/c3fcafdc10146beb5919319d0683e44e3c30d537)，许可保留在 `native/vendor/minhook/LICENSE.txt`，也可在独立版设置中打开“开源许可”查看。独立版复用窗口总览和手势实现，不依赖 Windhawk 引擎。

设置页使用 [Microsoft Edge WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4314.35)。官方 Loader 的许可证和第三方声明随构建嵌入 EXE，可在“关于 → 开源许可”中查看。
