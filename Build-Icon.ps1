$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$source = Join-Path $PSScriptRoot 'assets\touchpad-scroll.png'
$destination = Join-Path $PSScriptRoot 'assets\touchpad-scroll.ico'
$image = [Drawing.Bitmap]::new($source)
$sizes = @(16,20,24,32,48,64,128,256)
$frames = [Collections.Generic.List[byte[]]]::new()
try {
    foreach ($size in $sizes) {
        $bitmap = [Drawing.Bitmap]::new($size,$size,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $memory = [IO.MemoryStream]::new()
        try {
            $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.Clear([Drawing.Color]::Transparent)
            $graphics.DrawImage($image,[Drawing.Rectangle]::new(0,0,$size,$size),0,0,$image.Width,$image.Height,[Drawing.GraphicsUnit]::Pixel)
            $bitmap.Save($memory,[Drawing.Imaging.ImageFormat]::Png)
            $frames.Add($memory.ToArray())
        } finally { $memory.Dispose(); $graphics.Dispose(); $bitmap.Dispose() }
    }
    $file = [IO.File]::Create($destination)
    $writer = [IO.BinaryWriter]::new($file)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
        [uint32]$offset = 6+16*$sizes.Count
        for ($i=0;$i -lt $sizes.Count;$i++) {
            [byte]$dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
            $writer.Write($dimension); $writer.Write($dimension)
            $writer.Write([byte]0); $writer.Write([byte]0)
            $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$frames[$i].Length); $writer.Write($offset)
            $offset += $frames[$i].Length
        }
        foreach ($frame in $frames) { $writer.Write($frame) }
    } finally { $writer.Dispose(); $file.Dispose() }
    "ICO sizes: $($sizes -join ', '); original: $($image.Width)x$($image.Height); corner alpha: $($image.GetPixel(0,0).A)"
} finally { $image.Dispose() }
