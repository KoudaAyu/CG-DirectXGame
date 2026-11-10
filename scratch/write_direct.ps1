Add-Type -AssemblyName System.Drawing

$size = 512
$bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$center = ($size - 1.0) / 2.0
$maxRadius = $size / 2.0

$puffs = @(
    @{ cx = 0.00; cy = 0.00; r = 0.42; w = 1.00 },
    @{ cx = 0.14; cy = 0.10; r = 0.32; w = 0.85 },
    @{ cx = -0.12; cy = 0.15; r = 0.30; w = 0.80 },
    @{ cx = -0.16; cy = -0.08; r = 0.32; w = 0.82 },
    @{ cx = 0.12; cy = -0.14; r = 0.28; w = 0.75 },
    @{ cx = 0.00; cy = 0.22; r = 0.26; w = 0.70 },
    @{ cx = -0.20; cy = 0.04; r = 0.25; w = 0.68 },
    @{ cx = 0.20; cy = -0.02; r = 0.24; w = 0.65 }
)

for ($y = 0; $y -lt $size; $y++) {
    $ny = ($y - $center) / $maxRadius
    for ($x = 0; $x -lt $size; $x++) {
        $nx = ($x - $center) / $maxRadius
        $dist = [Math]::Sqrt($nx * $nx + $ny * $ny)
        
        if ($dist -ge 0.98) {
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 255, 255, 255))
            continue
        }
        
        $density = 0.0
        foreach ($p in $puffs) {
            $pdx = ($nx - $p.cx) / $p.r
            $pdy = ($ny - $p.cy) / $p.r
            $pdistSq = $pdx * $pdx + $pdy * $pdy
            if ($pdistSq -lt 1.0) {
                $f = 1.0 - $pdistSq
                $density += $p.w * ($f * $f)
            }
        }
        
        $edgeFade = [Math]::Max(0.0, 1.0 - $dist)
        $edgeFactor = [Math]::Sin($edgeFade * [Math]::PI * 0.5)
        
        $finalDensity = [Math]::Min(1.0, $density * 0.95) * $edgeFactor
        $alphaF = [Math]::Pow($finalDensity, 1.25)
        $alpha = [int][Math]::Round(255.0 * $alphaF)
        if ($alpha -gt 255) { $alpha = 255 }
        if ($alpha -lt 0) { $alpha = 0 }
        
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 255, 255, 255))
    }
}

$targetFiles = @(
    "project\Resources\smoke_puff_white.png",
    "project\Resources\title_smoke_puff.png",
    "project\generated\output\Debug\Resources\smoke_puff_white.png",
    "project\generated\output\Debug\Resources\title_smoke_puff.png",
    "project\generated\output\Release\Resources\smoke_puff_white.png",
    "project\generated\output\Release\Resources\title_smoke_puff.png",
    "generated\output\Debug\Resources\smoke_puff_white.png",
    "generated\output\Debug\Resources\title_smoke_puff.png",
    "generated\output\Release\Resources\smoke_puff_white.png",
    "generated\output\Release\Resources\title_smoke_puff.png"
)

# メモリにPNGストリームを作成
$ms = New-Object System.IO.MemoryStream
$bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
$bytes = $ms.ToArray()
$ms.Dispose()
$bmp.Dispose()

Write-Host "Generated PNG byte size: $($bytes.Length)"

foreach ($target in $targetFiles) {
    $dir = [System.IO.Path]::GetDirectoryName($target)
    if (Test-Path $dir) {
        [System.IO.File]::WriteAllBytes($target, $bytes)
        Write-Host "Wrote to $target (Size: $($bytes.Length))"
    }
}
