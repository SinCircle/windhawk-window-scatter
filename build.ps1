param(
    [Parameter(Mandatory = $true)][string]$WindhawkRoot,
    [ValidateSet('x86_64', 'i686')][string]$Architecture = 'x86_64'
)
$ErrorActionPreference = 'Stop'
$sourceFiles = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.wh.cpp' -File)
if ($sourceFiles.Count -ne 1) { throw 'Expected exactly one .wh.cpp source file.' }
$source = $sourceFiles[0]
$text = [IO.File]::ReadAllText($source.FullName)
$modId = [regex]::Match($text, '(?m)^//\s*@id\s+(\S+)').Groups[1].Value
$version = [regex]::Match($text, '(?m)^//\s*@version\s+(\S+)').Groups[1].Value
$modArchitecture = [regex]::Match($text, '(?m)^//\s*@architecture\s+([^\r\n]+)').Groups[1].Value.Trim()
if (!$modId -or !$version) { throw 'Missing Windhawk mod ID or version.' }
if ($modArchitecture -eq 'x86-64' -and $Architecture -ne 'x86_64') { throw 'This mod supports x64 only.' }
$compiler = Join-Path $WindhawkRoot 'Compiler\bin\clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw "Windhawk compiler not found: $compiler" }
$engine = Get-ChildItem -LiteralPath (Join-Path $WindhawkRoot 'Engine') -Directory |
    Where-Object Name -Match '^\d+\.\d+\.\d+$' |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!$engine) { throw 'Windhawk Engine was not found.' }
$bits = if ($Architecture -eq 'x86_64') { '64' } else { '32' }
$library = Join-Path $engine.FullName "$bits\windhawk.lib"
if (!(Test-Path -LiteralPath $library)) { throw "Engine library not found: $library" }
$buildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$output = Join-Path $buildDir "$modId-$bits.dll"
$flags = @('-std=c++23', '-target', "$Architecture-w64-mingw32", '-O2', '-shared',
    '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
    '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008', '-D__USE_MINGW_ANSI_STDIO=0',
    '-DWH_MOD', ('-DWH_MOD_ID=L"{0}"' -f $modId), ('-DWH_MOD_VERSION=L"{0}"' -f $version),
    '-include', 'windhawk_api.h', '-Wno-pragma-pack', '-Wno-pragma-system-header-outside-header',
    $source.FullName, $library)
if ($text -notmatch '(?m)^\s*#define\s+NOMINMAX\b') { $flags += '-DNOMINMAX' }
$options = [regex]::Match($text, '(?m)^//\s*@compilerOptions\s+([^\r\n]+)').Groups[1].Value.Trim()
if ($options) { $flags += $options -split '\s+' }
$flags += @('-o', $output)
& $compiler @flags
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
Write-Output "Built $output"
Write-Output 'To install, paste the .wh.cpp source into the Windhawk editor and compile there.'
