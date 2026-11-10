Add-Type -AssemblyName System.Drawing

$size = 512
$bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$center = ($size - 1.0) / 2.0
$maxRadius = $size / 2.0

# リアルな煙雲（スモークパフ）：モコモコした複数の球状パフの合成
$puffs = @(
    @{ cx = 0.00; cy = 0.00; r = 0.42; w = 1.00 }, # メイン中心核
    @{ cx = 0.14; cy = 0.10; r = 0.32; w = 0.85 }, # 右上パフ
    @{ cx = -0.12; cy = 0.15; r = 0.30; w = 0.80 }, # 左上パフ
    @{ cx = -0.16; cy = -0.08; r = 0.32; w = 0.82 }, # 左下パフ
    @{ cx = 0.12; cy = -0.14; r = 0.28; w = 0.75 }, # 右下パフ
    @{ cx = 0.00; cy = 0.22; r = 0.26; w = 0.70 }, # 上部パフ
    @{ cx = -0.20; cy = 0.04; r = 0.25; w = 0.68 }, # 左部パフ
    @{ cx = 0.20; cy = -0.02; r = 0.24; w = 0.65 }  # 右部パフ
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
        
        # 複数パフの濃度加算
        $density = 0.0
        foreach ($p in $puffs) {
            $pdx = ($nx - $p.cx) / $p.r
            $pdy = ($ny - $p.cy) / $p.r
            $pdistSq = $pdx * $pdx + $pdy * $pdy
            if ($pdistSq -lt 1.0) {
                # ガウシアン風の滑らかな減衰 (1 - r^2)^2
                $f = 1.0 - $pdistSq
                $density += $p.w * ($f * $f)
            }
        }
        
        # 最外周のソフトフェードアウト
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

$destProject = "project\Resources\smoke_puff_white.png"
$destTitle   = "project\Resources\title_smoke_puff.png"
$tempPath    = "scratch\temp_puff.png"

$bmp.Save($tempPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Copy-Item -Path $tempPath -Destination $destProject -Force
Copy-Item -Path $tempPath -Destination $destTitle -Force
Remove-Item -Path $tempPath -Force

Write-Host "Puff texture generated successfully!"
