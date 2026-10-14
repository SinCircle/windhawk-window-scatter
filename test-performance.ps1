param(
    [string]$Compiler='clang++.exe',
    [ValidateSet('x86_64','i686')][string]$Architecture='x86_64',
    [ValidateSet('layout','resources','idle')][string]$Mode='layout'
)
$ErrorActionPreference='Stop'
$build=Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$output=Join-Path $build "performance-$Mode-$Architecture.exe"
& $Compiler -std=c++23 -target "$Architecture-w64-mingw32" -O2 -static -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 (Join-Path $PSScriptRoot "tests/performance-$Mode.cpp") -ld3d11 -ldxgi -ldcomp -ldwmapi -ld2d1 -ldwrite -lwindowscodecs -lole32 -loleaut32 -lshell32 -lgdi32 -luser32 -luuid -lruntimeobject -lpsapi -o $output
if($LASTEXITCODE -ne 0){throw 'Performance benchmark compilation failed'}
& $output
if($LASTEXITCODE -ne 0){throw 'Performance benchmark failed'}
