$puffs = @(
    @{ cx = 0.00; cy = 0.00; r = 0.42; w = 1.00 }
)
$nx = 0.0
$ny = 0.0
$density = 0.0
foreach ($p in $puffs) {
    $pdx = ($nx - $p.cx) / $p.r
    $pdy = ($ny - $p.cy) / $p.r
    $pdistSq = $pdx * $pdx + $pdy * $pdy
    Write-Host "pdistSq: $pdistSq"
    if ($pdistSq -lt 1.0) {
        $f = 1.0 - $pdistSq
        $density += $p.w * ($f * $f)
    }
}
Write-Host "density: $density"
$edgeFade = [Math]::Max(0.0, 1.0 - 0.0)
$edgeFactor = [Math]::Sin($edgeFade * [Math]::PI * 0.5)
$finalDensity = [Math]::Min(1.0, $density * 0.95) * $edgeFactor
$alphaF = [Math]::Pow($finalDensity, 1.25)
$alpha = [int][Math]::Round(255.0 * $alphaF)
Write-Host "alpha: $alpha"
