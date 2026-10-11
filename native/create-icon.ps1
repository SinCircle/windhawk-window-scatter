param([Parameter(Mandatory = $true)][string]$OutputPath)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
function New-RoundedPath([single]$x,[single]$y,[single]$width,[single]$height,[single]$radius) {
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    [single]$diameter = $radius * 2
    $path.AddArc($x,$y,$diameter,$diameter,180,90)
    $path.AddArc($x+$width-$diameter,$y,$diameter,$diameter,270,90)
    $path.AddArc($x+$width-$diameter,$y+$height-$diameter,$diameter,$diameter,0,90)
    $path.AddArc($x,$y+$height-$diameter,$diameter,$diameter,90,90)
    $path.CloseFigure()
    return ,$path
}
$images = @()
foreach ($size in @(16,24,32,48,64,128,256)) {
    $bitmap = [Drawing.Bitmap]::new($size,$size,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.ScaleTransform($size / 128.0,$size / 128.0)
    $background = New-RoundedPath 5 5 118 118 28
    $blue = [Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.Point]::new(0,0),[Drawing.Point]::new(0,128),[Drawing.Color]::FromArgb(32,149,251),[Drawing.Color]::FromArgb(0,95,211))
    $graphics.FillPath($blue,$background)
    $back = New-RoundedPath 24 30 57 45 7
    $glass = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(40,255,255,255))
    $stroke = [Drawing.Pen]::new([Drawing.Color]::FromArgb(245,255,255,255),4)
    $graphics.FillPath($glass,$back)
    $graphics.DrawPath($stroke,$back)
    $front = New-RoundedPath 47 51 53 43 7
    $white = [Drawing.SolidBrush]::new([Drawing.Color]::White)
    $graphics.FillPath($white,$front)
    $stream = [IO.MemoryStream]::new()
    $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
    $images += [pscustomobject]@{ Size=$size; Data=$stream.ToArray() }
    foreach ($item in @($stream,$white,$front,$stroke,$glass,$back,$blue,$background,$graphics,$bitmap)) { $item.Dispose() }
}
$output = [IO.File]::Create($OutputPath)
$writer = [IO.BinaryWriter]::new($output)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
    [uint32]$offset = 6 + 16 * $images.Count
    foreach ($image in $images) {
        [byte]$dimension = if ($image.Size -eq 256) { 0 } else { $image.Size }
        $writer.Write($dimension); $writer.Write($dimension); $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32)
        $writer.Write([uint32]$image.Data.Length); $writer.Write($offset)
        $offset += $image.Data.Length
    }
    foreach ($image in $images) { $writer.Write([byte[]]$image.Data) }
} finally { $writer.Dispose(); $output.Dispose() }
