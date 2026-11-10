Add-Type -AssemblyName System.Drawing

$size = 512
$bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$center = ($size - 1.0) / 2.0
$maxRadius = $size / 2.0

$puffs = @(
    @{ cx = 0.00; cy = 0.00; r = 0.42; w = 1.00 }
)

$sampleAlpha = 0
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
        
        if ($x -eq 256 -and $y -eq 256) {
            $sampleAlpha = $alpha
            Write-Host "Loop at center (256,256): nx=$nx, ny=$ny, dist=$dist, density=$density, alpha=$alpha"
        }
        
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 255, 255, 255))
    }
}

$pCenter = $bmp.GetPixel(256, 256)
Write-Host "Read back from bmp (256,256): Alpha=$($pCenter.A)"

$bmp.Dispose()
