Add-Type -AssemblyName System.Drawing

$size = 512
$bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$center = ($size - 1) / 2.0
$maxRadius = $size / 2.0

# リアルな煙雲（スモークパフ）：複数のサブ核（Sub-puffs）を配置
# 各核: 中心座標 (cx, cy), 半径 (r), 重み (w)
$puffs = @(
    @{ cx = 0.00; cy = 0.00; r = 0.46; w = 1.00 }, # メイン核
    @{ cx = 0.12; cy = 0.08; r = 0.34; w = 0.85 }, # 右上サブ核
    @{ cx = -0.10; cy = 0.14; r = 0.32; w = 0.78 }, # 左上サブ核
    @{ cx = -0.14; cy = -0.09; r = 0.35; w = 0.80 }, # 左下サブ核
    @{ cx = 0.11; cy = -0.12; r = 0.30; w = 0.72 }, # 右下サブ核
    @{ cx = 0.02; cy = 0.20; r = 0.26; w = 0.65 }, # 上部モコモコ
    @{ cx = -0.18; cy = 0.02; r = 0.28; w = 0.70 }, # 左部モコモコ
    @{ cx = 0.20; cy = -0.04; r = 0.25; w = 0.60 }  # 右部モコモコ
)

# 簡単な疑似パーリンノイズ格子（煙の繊維・もやテクスチャ用）
$gridSize = 16
$randGrid = New-Object 'double[,]' $gridSize, $gridSize
$rng = New-Object System.Random(12345)
for ($i = 0; $i -lt $gridSize; $i++) {
    for ($j = 0; $j -lt $gridSize; $j++) {
        $randGrid[$i, $j] = $rng.NextDouble()
    }
}

function GetSmoothNoise([double]$u, [double]$v) {
    $gx = $u * ($gridSize - 1)
    $gy = $v * ($gridSize - 1)
    $x0 = [int][Math]::Floor($gx)
    $y0 = [int][Math]::Floor($gy)
    $x1 = [Math]::Min($x0 + 1, $gridSize - 1)
    $y1 = [Math]::Min($y0 + 1, $gridSize - 1)
    $tx = $gx - $x0
    $ty = $gy - $y0
    $fx = $tx * $tx * (3.0 - 2.0 * $tx)
    $fy = $ty * $ty * (3.0 - 2.0 * $ty)
    $v00 = $randGrid[$x0, $y0]
    $v10 = $randGrid[$x1, $y0]
    $v01 = $randGrid[$x0, $y1]
    $v11 = $randGrid[$x1, $y1]
    $v0 = $v00 + $fx * ($v10 - $v00)
    $v1 = $v01 + $fx * ($v11 - $v01)
    return $v0 + $fy * ($v1 - $v0)
}

for ($y = 0; $y -lt $size; $y++) {
    $ny = ($y - $center) / $maxRadius
    $vCoord = $y / ($size - 1.0)
    for ($x = 0; $x -lt $size; $x++) {
        $nx = ($x - $center) / $maxRadius
        $uCoord = $x / ($size - 1.0)
        
        $distFromCenter = [Math]::Sqrt($nx * $nx + $ny * $ny)
        if ($distFromCenter -ge 0.98) {
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 255, 255, 255))
            continue
        }
        
        # 1. 各サブパフのガウシアン蓄積
        $density = 0.0
        foreach ($p in $puffs) {
            $pdx = $nx - $p.cx
            $pdy = $ny - $p.cy
            $pdist = [Math]::Sqrt($pdx * $pdx + $pdy * $pdy) / $p.r
            if ($pdist -lt 1.0) {
                # 滑らかなエルミート減衰
                $fade = (1.0 - $pdist * $pdist)
                $density += $p.w * ($fade * $fade)
            }
        }
        
        # 2. 煙の微細な繊維ノイズの合成
        $noiseVal = GetSmoothNoise ($uCoord * 1.5) ($vCoord * 1.5)
        $noiseVal2 = GetSmoothNoise ($uCoord * 3.5) ($vCoord * 3.5)
        $detail = 0.82 + 0.28 * ($noiseVal * 0.7 + $noiseVal2 * 0.3)
        $density = $density * $detail
        
        # 3. 外周の確実なソフトフォールオフ（円周境界で完全にゼロに滑らかに消散）
        $edgeFade = [Math]::Max(0.0, 1.0 - $distFromCenter)
        $edgeFade = [Math]::Sin($edgeFade * [Math]::PI * 0.5)
        $finalVal = [Math]::Min(1.0, $density * 1.15) * $edgeFade
        
        # S字トーンカーブで中心部の密度と外周の透明感を両立
        $alphaF = [Math]::Pow($finalVal, 1.35)
        $alpha = [int][Math]::Round(255.0 * $alphaF)
        if ($alpha -gt 255) { $alpha = 255 }
        if ($alpha -lt 0) { $alpha = 0 }
        
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 255, 255, 255))
    }
}

$destProject = "project\Resources\smoke_puff_white.png"
$destTitle   = "project\Resources\title_smoke_puff.png"
$tempPath    = "scratch\temp_organic_smoke.png"

$bmp.Save($tempPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Copy-Item -Path $tempPath -Destination $destProject -Force
Copy-Item -Path $tempPath -Destination $destTitle -Force
Remove-Item -Path $tempPath -Force

Write-Host "Successfully generated organic puffy smoke textures at $destProject and $destTitle"
