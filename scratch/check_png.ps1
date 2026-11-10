Add-Type -AssemblyName System.Drawing
$b = [System.Drawing.Bitmap]::FromFile((Resolve-Path "project\Resources\smoke_puff_white.png"))
$p = $b.GetPixel(256, 256)
$a = $p.A
$r = $p.R
$g = $p.G
$blue = $p.B
Write-Host "Pixel at center: A=$a R=$r G=$g B=$blue"
$maxA = 0
for ($y = 0; $y -lt $b.Height; $y += 8) {
    for ($x = 0; $x -lt $b.Width; $x += 8) {
        $pix = $b.GetPixel($x, $y)
        if ($pix.A -gt $maxA) { $maxA = $pix.A }
    }
}
Write-Host "Max Alpha across image: $maxA"
$b.Dispose()
