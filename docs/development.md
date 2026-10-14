# 构建与开发

[返回首页](../README.md)

所有命令在仓库根目录执行。

独立版使用 LLVM-MinGW 的 `clang++.exe` 和 Windows SDK 的 `rc.exe` 编译：

```powershell
.\build-native.ps1 -Compiler 'C:\path\to\llvm-mingw\bin\clang++.exe'
```

生成的 `build/native/WindowScatter.exe` 可单独分发。脚本会将连接组件、图标和第三方许可嵌入 EXE，并静态链接 C/C++ 运行库；构建时需要编译器，最终用户无需安装它们。可以通过 `-ResourceCompiler` 显式指定 `rc.exe`，通过 `-OutputDirectory` 更改输出位置。

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
