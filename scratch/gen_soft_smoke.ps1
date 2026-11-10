Add-Type -AssemblyName System.Drawing

$size = 512
$bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$center = ($size - 1) / 2.0
$maxRadius = $size / 2.0

for ($y = 0; $y -lt $size; $y++) {
    for ($x = 0; $x -lt $size; $x++) {
        $dx = $x - $center
        $dy = $y - $center
        $dist = [Math]::Sqrt($dx * $dx + $dy * $dy)
        $normDist = $dist / $maxRadius
        
        if ($normDist -ge 1.0) {
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 255, 255, 255))
        } else {
            # 完全なソフトガウシアン減衰フォールオフ
            # 中心部(r < 0.25)は高密度、そこから外周(r -> 1.0)にかけて滑らかに0へ減衰
            $fade = [Math]::Pow([Math]::Cos($normDist * [Math]::PI * 0.5), 2.2)
            $alpha = [int][Math]::Round(255.0 * $fade)
            if ($alpha -gt 255) { $alpha = 255 }
            if ($alpha -lt 0) { $alpha = 0 }
            
            # 純白 (RGB=255,255,255) でアルファのみグラデーション
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 255, 255, 255))
        }
    }
}

$destPath = "project\Resources\smoke_puff_white.png"
$tempPath = "scratch\temp_smoke.png"
$bmp.Save($tempPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Move-Item -Path $tempPath -Destination $destPath -Force
Write-Host "Successfully generated soft gaussian smoke texture at $destPath"
