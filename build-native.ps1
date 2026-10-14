param(
    [string]$Compiler = 'clang++.exe',
    [string]$ResourceCompiler,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$native = Join-Path $root 'native'
$build = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $root 'build\native' }
New-Item -ItemType Directory -Path $build -Force | Out-Null
if (!(Get-Command $Compiler -ErrorAction SilentlyContinue)) { throw 'Install LLVM-MinGW, or pass -Compiler with the absolute path to clang++.exe.' }
if (!$ResourceCompiler) {
    $command = Get-Command rc.exe -ErrorAction SilentlyContinue
    if ($command) { $ResourceCompiler = $command.Source }
    else {
        $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
        $sdk = Get-ChildItem -LiteralPath $kits -Directory -ErrorAction SilentlyContinue |
            Where-Object Name -Match '^10\.\d+\.\d+\.\d+$' | Sort-Object { [version]$_.Name } -Descending |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'x64\rc.exe') } | Select-Object -First 1
        if ($sdk) { $ResourceCompiler = Join-Path $sdk.FullName 'x64\rc.exe' }
    }
}
if (!$ResourceCompiler -or !(Test-Path -LiteralPath $ResourceCompiler)) { throw 'Windows SDK resource compiler was not found. Pass -ResourceCompiler with rc.exe.' }
$utf8 = [Text.UTF8Encoding]::new($false)
$source = [IO.File]::ReadAllText((Join-Path $root 'window-scatter.wh.cpp'))
$constants = [regex]::Match($source,'(?s)(constexpr wchar_t kController\[\].*?constexpr ULONG_PTR kShellAck=.*?;)').Groups[1].Value
$bridge = [regex]::Match($source,'(?s)(namespace scatter::shell \{.*?\} // namespace scatter::shell)').Groups[1].Value
if (!$constants -or !$bridge) { throw 'Shared renderer protocol or native gesture code was not found.' }
$target = 'HWND target=FindWindow(kController,L"Window Scatter Controller");'
if (!$bridge.Contains($target)) { throw 'Native controller binding changed; update the standalone bridge adapter.' }
$bridge = $bridge.Replace($target,'HWND target=g_nativeTarget.load();')
[IO.File]::WriteAllText((Join-Path $build 'renderer-protocol.inc'),$constants,$utf8)
[IO.File]::WriteAllText((Join-Path $build 'shell-hooks.inc'),$bridge,$utf8)
$common = @('-std=c++23','-target','x86_64-w64-mingw32','-O2','-static','-Wl,--no-insert-timestamp','-DUNICODE','-D_UNICODE',
    '-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00','-DNTDDI_VERSION=0x0A000008',
    '-D__USE_MINGW_ANSI_STDIO=0','-I',$native,'-I',$build,'-Wno-deprecated','-Wno-pragma-pack')
$payload = Join-Path $build 'window-scatter-bridge.dll'
$hookSources = @('buffer.c','hook.c','trampoline.c','hde\hde64.c') | ForEach-Object { Join-Path $native "vendor\minhook\src\$_" }
& $Compiler @common -shared (Join-Path $native 'bridge.cpp') @hookSources '-luser32' '-o' $payload
if ($LASTEXITCODE -ne 0) { throw 'Standalone bridge build failed.' }
$icon = Join-Path $build 'window-scatter.ico'
& (Join-Path $native 'create-icon.ps1') -OutputPath $icon
$notice = "Window Scatter $([char]0x2014) third-party notices`r`n`r`nMinHook v1.3.4, commit c3fcafdc10146beb5919319d0683e44e3c30d537`r`nhttps://github.com/TsudaKageyu/minhook`r`n`r`n" + [IO.File]::ReadAllText((Join-Path $native 'vendor\minhook\LICENSE.txt'))
$noticePath = Join-Path $build 'THIRD-PARTY-NOTICES.txt'
[IO.File]::WriteAllText($noticePath,$notice,$utf8)
function ResourcePath([string]$path) { return $path.Replace('\','\\') }
$rc = @"
101 ICON "$(ResourcePath $icon)"
1 24 "$(ResourcePath (Join-Path $native 'app.manifest'))"
201 RCDATA "$(ResourcePath $payload)"
202 RCDATA "$(ResourcePath $noticePath)"
1 VERSIONINFO
FILEVERSION 0,5,7,0
PRODUCTVERSION 0,5,7,0
FILEFLAGSMASK 0x3fL
FILEFLAGS 0x0L
FILEOS 0x40004L
FILETYPE 0x1L
FILESUBTYPE 0x0L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "SinCircle\0"
      VALUE "FileDescription", "Window Scatter\0"
      VALUE "FileVersion", "0.5.7.0\0"
      VALUE "InternalName", "WindowScatter\0"
      VALUE "OriginalFilename", "WindowScatter.exe\0"
      VALUE "ProductName", "Window Scatter\0"
      VALUE "ProductVersion", "0.5.7\0"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
"@
$rcPath = Join-Path $build 'app.rc'
$resPath = Join-Path $build 'app.res'
[IO.File]::WriteAllText($rcPath,$rc,$utf8)
& $ResourceCompiler /nologo /fo $resPath $rcPath
if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
$libraries = @('-ld3d11','-ldxgi','-ldcomp','-ldwmapi','-ld2d1','-ldwrite','-lwindowscodecs','-lole32','-loleaut32','-lshell32',
    '-lgdi32','-luser32','-luuid','-lruntimeobject','-lcomctl32','-lgdiplus','-luxtheme','-loleacc','-ldbghelp','-lwinhttp','-lbcrypt')
$output = Join-Path $build 'WindowScatter.exe'
& $Compiler @common -municode -mwindows (Join-Path $native 'app.cpp') (Join-Path $native 'backend.cpp') (Join-Path $native 'settings-window.cpp') $resPath @libraries '-o' $output
if ($LASTEXITCODE -ne 0) { throw 'Standalone application build failed.' }
Write-Output "Built $output"
Write-Output 'Single-file Windows 11 x64 application; no Windhawk or additional runtime is required.'
