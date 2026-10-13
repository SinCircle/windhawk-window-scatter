param(
    [string]$Compiler = 'clang++.exe',
    [ValidateSet('x86_64','i686')][string]$Architecture = 'x86_64',
    [string]$CapturePath
)
$ErrorActionPreference='Stop'
$build=Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$output=Join-Path $build "minimized-windows-test-$Architecture.exe"
$source=Join-Path $PSScriptRoot 'tests\minimized-windows.cpp'
$flags=@('-std=c++23','-target',"$Architecture-w64-mingw32",'-O2','-static','-municode','-DUNICODE','-D_UNICODE',
    '-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-DNTDDI_VERSION=0x0A000008','-D__USE_MINGW_ANSI_STDIO=0')
$libraries=@('-ld3d11','-ldxgi','-ldcomp','-ldwmapi','-ld2d1','-ldwrite','-lwindowscodecs','-lole32','-lshell32','-lgdi32','-luser32','-luuid','-lruntimeobject')
& $Compiler @flags $source @libraries -o $output
if($LASTEXITCODE -ne 0){throw 'Minimized-window test compilation failed'}
$log=Join-Path $build "minimized-windows-test-$Architecture.log"
$arguments=if($CapturePath){@('--capture',('"'+[IO.Path]::GetFullPath($CapturePath)+'"'))}else{@()}
$options=@{FilePath=$output;WindowStyle='Hidden';PassThru=$true;RedirectStandardOutput=$log}
if($arguments.Count){$options.ArgumentList=$arguments}
$process=Start-Process @options
if(!$process.WaitForExit(30000)){throw 'Minimized-window test timed out'}
$process.Refresh();Get-Content -LiteralPath $log
if($process.ExitCode -ne 0){throw "Minimized-window test failed: $($process.ExitCode)"}
