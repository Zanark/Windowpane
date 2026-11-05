$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $root 'docs\assets'
[void](New-Item $destination -ItemType Directory -Force)

foreach ($name in @('original-pane', 'keyed-pane', 'controls')) {
    $inputPath = Join-Path $root "build\synthetic-$name.bmp"
    if (-not (Test-Path $inputPath)) { throw "Run the native synthetic test first: $inputPath" }
    $bytes = [IO.File]::ReadAllBytes($inputPath)
    $offset = [BitConverter]::ToInt32($bytes, 10)
    $width = [BitConverter]::ToInt32($bytes, 18)
    $height = -[BitConverter]::ToInt32($bytes, 22)
    if ($width -le 0 -or $height -le 0 -or $width -gt 4096 -or $height -gt 4096) {
        throw 'Expected a bounded top-down synthetic BGRA bitmap.'
    }
    $image = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppPArgb)
    $rectangle = [Drawing.Rectangle]::new(0, 0, $width, $height)
    $locked = $image.LockBits($rectangle, [Drawing.Imaging.ImageLockMode]::WriteOnly,
        [Drawing.Imaging.PixelFormat]::Format32bppPArgb)
    try {
        [Runtime.InteropServices.Marshal]::Copy($bytes, $offset, $locked.Scan0, $width * $height * 4)
    } finally { $image.UnlockBits($locked) }
    try {
        if ($name -eq 'controls') {
            $image.Save((Join-Path $destination 'controls.png'), [Drawing.Imaging.ImageFormat]::Png)
        } else {
            $image.Save((Join-Path $destination "synthetic-$name-alpha.png"), [Drawing.Imaging.ImageFormat]::Png)
            $preview = [Drawing.Bitmap]::new($width, $height)
            $graphics = [Drawing.Graphics]::FromImage($preview)
            $light = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(54, 65, 81))
            $dark = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(30, 40, 55))
            try {
                for ($y = 0; $y -lt $height; $y += 10) {
                    for ($x = 0; $x -lt $width; $x += 10) {
                        $brush = if ((($x / 10 + $y / 10) % 2) -eq 0) { $light } else { $dark }
                        $graphics.FillRectangle($brush, $x, $y, 10, 10)
                    }
                }
                $graphics.DrawImageUnscaled($image, 0, 0)
                $preview.Save((Join-Path $destination "synthetic-$name.png"), [Drawing.Imaging.ImageFormat]::Png)
            } finally {
                $light.Dispose()
                $dark.Dispose()
                $graphics.Dispose()
                $preview.Dispose()
            }
        }
    } finally { $image.Dispose() }
}
Write-Output $destination
