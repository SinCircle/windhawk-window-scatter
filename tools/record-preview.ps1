param(
    [string]$Compiler='clang++.exe',
    [string]$FFmpeg='ffmpeg.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=Join-Path $root 'build/readme'
$media=Join-Path $root 'docs/media'
New-Item -ItemType Directory -Path $build,$media -Force | Out-Null
$encoder=(Get-Command $FFmpeg -ErrorAction Stop).Source
$recorder=Join-Path $build 'record-preview.exe'
& $Compiler -std=c++23 -target x86_64-w64-mingw32 -municode -O2 -static -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000008 (Join-Path $PSScriptRoot 'record-preview.cpp') -ld3d11 -ldxgi -ldcomp -ldwmapi -ld2d1 -ldwrite -lwindowscodecs -lole32 -loleaut32 -lshell32 -lgdi32 -luser32 -luuid -lruntimeobject -lgdiplus -o $recorder
if($LASTEXITCODE -ne 0){throw 'Preview recorder compilation failed'}
foreach($mode in @('overview','minimize')){
    $capture=Join-Path $build "$mode.mkv"
    Write-Host "Recording fictional native windows: $mode. Press Esc to stop."
    & $recorder $encoder $capture $mode
    if($LASTEXITCODE -ne 0){throw "Recording failed or was interrupted: $mode"}
    & $encoder -hide_banner -loglevel error -y -i $capture -filter_complex '[0:v]fps=25,scale=1080:-1:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle' -loop 0 (Join-Path $media "$mode.gif")
    if($LASTEXITCODE -ne 0){throw "GIF conversion failed: $mode"}
}
& $encoder -hide_banner -loglevel error -y -ss 3 -i (Join-Path $build 'overview.mkv') -frames:v 1 (Join-Path $media 'overview.png')
if($LASTEXITCODE -ne 0){throw 'Still frame extraction failed'}
& $encoder -hide_banner -loglevel error -y -ss 3 -i (Join-Path $build 'overview.mkv') -vf 'crop=490:390:185:30' -frames:v 1 (Join-Path $media 'detail.png')
if($LASTEXITCODE -ne 0){throw 'Detail frame extraction failed'}
Write-Host "Preview assets saved to $media. Inspect both animations before publishing."
