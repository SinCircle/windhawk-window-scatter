param([Parameter(Mandatory)][string]$CacheDirectory)
$ErrorActionPreference = 'Stop'
$version = '1.0.4314.35'
$expectedHash = '90778DEB1B77E36292828B4B5833C9255C27533F40E0DDD55891AE718944EDC4'
$directory = Join-Path $CacheDirectory "Microsoft.Web.WebView2.$version"
$archive = Join-Path $CacheDirectory "Microsoft.Web.WebView2.$version.nupkg"
New-Item -ItemType Directory -Path $CacheDirectory -Force | Out-Null
if (!(Test-Path -LiteralPath $archive)) {
    $temporary = "$archive.download"
    Invoke-WebRequest -Uri "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$version/microsoft.web.webview2.$version.nupkg" -OutFile $temporary
    if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $expectedHash) { throw 'WebView2 SDK checksum mismatch.' }
    Move-Item -LiteralPath $temporary -Destination $archive -Force
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) { throw 'Cached WebView2 SDK checksum mismatch.' }
if (!(Test-Path -LiteralPath (Join-Path $directory '.restored'))) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    # Use APIs also available in Windows PowerShell 5.1; retrying an interrupted
    # extraction overwrites only files from the verified archive.
    $rootPath = [IO.Path]::GetFullPath($directory).TrimEnd('\') + '\'
    $package = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        foreach ($entry in $package.Entries) {
            $targetPath = [IO.Path]::GetFullPath((Join-Path $directory $entry.FullName))
            if (!$targetPath.StartsWith($rootPath, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid SDK archive path.' }
            if (!$entry.Name) { [IO.Directory]::CreateDirectory($targetPath) | Out-Null; continue }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($targetPath)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $targetPath, $true)
        }
    } finally { $package.Dispose() }
    [IO.File]::WriteAllText((Join-Path $directory '.restored'), $expectedHash)
}
Write-Output $directory
