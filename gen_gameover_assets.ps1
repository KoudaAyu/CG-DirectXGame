Add-Type -AssemblyName System.Drawing
$resDir = Join-Path (Get-Location).Path "project\Resources"

function U([int[]]$codes) { -join ($codes | ForEach-Object { [char]$_ }) }

function New-GameOverLogo {
    param(
        [string]$Text, [string]$Sub,
        [System.Drawing.Color]$TopCol, [System.Drawing.Color]$MidCol, [System.Drawing.Color]$BotCol,
        [System.Drawing.Color]$RibbonCol, [System.Drawing.Color]$RibbonBorderCol,
        [System.Drawing.Color]$OuterCol,
        [string]$OutPath
    )
    $W = 1000; $H = 250
    $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    try { $ff = New-Object System.Drawing.FontFamily "Yu Gothic" } catch { $ff = New-Object System.Drawing.FontFamily "Meiryo" }
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $sf = [System.Drawing.StringFormat]::GenericTypographic
    $path.AddString($Text, $ff, [int][System.Drawing.FontStyle]::Bold, 100.0, (New-Object System.Drawing.PointF 0, 0), $sf)
    $b = $path.GetBounds()

    $skew = 0.18
    $boxW = 850.0; $boxH = 138.0
    $s = [Math]::Min($boxW / ($b.Width + $skew * $b.Height), $boxH / $b.Height)
    $m = New-Object System.Drawing.Drawing2D.Matrix
    $m.Translate(-$b.X, -$b.Y)
    $m.Scale($s, $s, [System.Drawing.Drawing2D.MatrixOrder]::Append)
    $m.Shear(-$skew, 0, [System.Drawing.Drawing2D.MatrixOrder]::Append)
    $path.Transform($m)
    $nb = $path.GetBounds()
    $dx = ($W - $nb.Width) / 2.0 - $nb.X
    $dy = 22.0 - $nb.Y
    $m2 = New-Object System.Drawing.Drawing2D.Matrix
    $m2.Translate($dx, $dy)
    $path.Transform($m2)
    $nb = $path.GetBounds()

    # drop shadow
    $sh = $path.Clone()
    $ms = New-Object System.Drawing.Drawing2D.Matrix; $ms.Translate(0, 9)
    $sh.Transform($ms)
    $shPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(220, 20, 6, 8)), 32
    $shPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($shPen, $sh)
    $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(220, 20, 6, 8))), $sh)

    # outlines: white outer -> dark accent -> obsidian black
    foreach ($o in @(@(30, [System.Drawing.Color]::White), @(21, $OuterCol), @(9, [System.Drawing.Color]::FromArgb(255, 20, 6, 8)))) {
        $pen = New-Object System.Drawing.Pen $o[1], $o[0]
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
    }

    # gradient fill
    $rect = New-Object System.Drawing.RectangleF $nb.X, $nb.Y, $nb.Width, $nb.Height
    $lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $TopCol, $BotCol, 90.0
    $cb = New-Object System.Drawing.Drawing2D.ColorBlend 3
    $cb.Colors = @($TopCol, $MidCol, $BotCol)
    $cb.Positions = @(0.0, 0.45, 1.0)
    $lg.InterpolationColors = $cb
    $g.FillPath($lg, $path)

    # gloss on upper half
    $state = $g.Save()
    $g.SetClip($path)
    $gl = New-Object System.Drawing.RectangleF $nb.X, $nb.Y, $nb.Width, ($nb.Height * 0.46)
    $glb = New-Object System.Drawing.Drawing2D.LinearGradientBrush $gl, ([System.Drawing.Color]::FromArgb(160, 255, 255, 255)), ([System.Drawing.Color]::FromArgb(15, 255, 255, 255)), 90.0
    $g.FillRectangle($glb, $gl)
    $g.Restore($state)

    # ribbon with sub caption (slanted)
    $ribY = 190.0; $ribH = 40.0
    $ribX1 = 130.0; $ribX2 = 870.0; $sk = 16.0
    $pts = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF ($ribX1 + $sk), $ribY),
        (New-Object System.Drawing.PointF ($ribX2 + $sk), $ribY),
        (New-Object System.Drawing.PointF ($ribX2 - $sk), ($ribY + $ribH)),
        (New-Object System.Drawing.PointF ($ribX1 - $sk), ($ribY + $ribH))
    )
    $rs = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(210, 15, 4, 6))
    $shadowPts = [System.Drawing.PointF[]]@($pts | ForEach-Object { New-Object System.Drawing.PointF $_.X, ($_.Y + 6) })
    $g.FillPolygon($rs, $shadowPts)
    $rb = New-Object System.Drawing.Drawing2D.LinearGradientBrush ((New-Object System.Drawing.RectangleF $ribX1, $ribY, ($ribX2 - $ribX1), $ribH)), $RibbonCol, ([System.Drawing.Color]::FromArgb(255, [int]($RibbonCol.R * 0.6), [int]($RibbonCol.G * 0.6), [int]($RibbonCol.B * 0.6))), 90.0
    $g.FillPolygon($rb, $pts)
    $wp = New-Object System.Drawing.Pen $RibbonBorderCol, 3
    $wp.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPolygon($wp, $pts)

    $fnt = New-Object System.Drawing.Font "Arial Black", 19.0, ([System.Drawing.FontStyle]::Italic)
    $tsf = New-Object System.Drawing.StringFormat
    $tsf.Alignment = [System.Drawing.StringAlignment]::Center
    $tsf.LineAlignment = [System.Drawing.StringAlignment]::Center
    $tr = New-Object System.Drawing.RectangleF $ribX1, ($ribY + 1), ($ribX2 - $ribX1), $ribH
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.DrawString($Sub, $fnt, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(180, 0, 0, 0))), (New-Object System.Drawing.RectangleF $ribX1, ($ribY + 3), ($ribX2 - $ribX1), $ribH), $tsf)
    $g.DrawString($Sub, $fnt, [System.Drawing.Brushes]::White, $tr, $tsf)

    # tactical glints / spark accents
    foreach ($sp in @(@(($nb.Right - 8), 24, 24), @(($nb.X + 28), 32, 18), @(($nb.Right - 130), 168, 14))) {
        $cx = [single]$sp[0]; $cy = [single]$sp[1]; $r = [single]$sp[2]
        $star = New-Object System.Drawing.Drawing2D.GraphicsPath
        $star.AddPolygon([System.Drawing.PointF[]]@(
            (New-Object System.Drawing.PointF $cx, ($cy - $r)),
            (New-Object System.Drawing.PointF ($cx + $r * 0.18), ($cy - $r * 0.18)),
            (New-Object System.Drawing.PointF ($cx + $r), $cy),
            (New-Object System.Drawing.PointF ($cx + $r * 0.18), ($cy + $r * 0.18)),
            (New-Object System.Drawing.PointF $cx, ($cy + $r)),
            (New-Object System.Drawing.PointF ($cx - $r * 0.18), ($cy + $r * 0.18)),
            (New-Object System.Drawing.PointF ($cx - $r), $cy),
            (New-Object System.Drawing.PointF ($cx - $r * 0.18), ($cy - $r * 0.18))
        ))
        $gp = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(200, 255, 100, 100)), 5
        $gp.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($gp, $star)
        $g.FillPath([System.Drawing.Brushes]::White, $star)
    }

    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "wrote $OutPath"
}

function New-MilitaryStamp {
    param(
        [string]$MainText, [string]$SubText, [string]$TopArc, [string]$BottomArc,
        [System.Drawing.Color]$InkCol,
        [string]$OutPath
    )
    $S = 400
    $bmp = New-Object System.Drawing.Bitmap $S, $S, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $cx = $S / 2.0; $cy = $S / 2.0
    $R = 170.0

    # Outer circle (double line)
    $pen1 = New-Object System.Drawing.Pen $InkCol, 7
    $pen2 = New-Object System.Drawing.Pen $InkCol, 3
    $pen2.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
    $g.DrawEllipse($pen1, [float]($cx - $R), [float]($cy - $R), [float]($R * 2), [float]($R * 2))
    $g.DrawEllipse($pen2, [float]($cx - ($R - 12)), [float]($cy - ($R - 12)), [float](($R - 12) * 2), [float](($R - 12) * 2))

    # Inner decorative circle
    $pen3 = New-Object System.Drawing.Pen $InkCol, 2
    $g.DrawEllipse($pen3, [float]($cx - ($R - 22)), [float]($cy - ($R - 22)), [float](($R - 22) * 2), [float](($R - 22) * 2))

    # Top Arc Text
    $fntArc = New-Object System.Drawing.Font "Arial Black", 14.0, ([System.Drawing.FontStyle]::Bold)
    $sfArc = New-Object System.Drawing.StringFormat
    $sfArc.Alignment = [System.Drawing.StringAlignment]::Center
    $sfArc.LineAlignment = [System.Drawing.StringAlignment]::Center

    # Draw top header text
    $g.DrawString($TopArc, $fntArc, (New-Object System.Drawing.SolidBrush $InkCol), (New-Object System.Drawing.RectangleF 30, 48, 340, 30), $sfArc)
    # Draw bottom footer text
    $g.DrawString($BottomArc, $fntArc, (New-Object System.Drawing.SolidBrush $InkCol), (New-Object System.Drawing.RectangleF 30, 322, 340, 30), $sfArc)

    # Central Box (slanted by -6 degrees for authentic rubber stamp vibe)
    $state = $g.Save()
    $g.TranslateTransform($cx, $cy)
    $g.RotateTransform(-6.0)

    $boxW = 270.0; $boxH = 110.0
    $boxRect = New-Object System.Drawing.RectangleF (-$boxW / 2.0), (-$boxH / 2.0), $boxW, $boxH

    # Box background slight tint
    $bgBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(40, $InkCol.R, $InkCol.G, $InkCol.B))
    $g.FillRectangle($bgBrush, $boxRect)

    # Box border (heavy stencil outline)
    $boxPen = New-Object System.Drawing.Pen $InkCol, 6
    $g.DrawRectangle($boxPen, $boxRect.X, $boxRect.Y, $boxRect.Width, $boxRect.Height)

    # Inner thin border
    $boxPenInner = New-Object System.Drawing.Pen $InkCol, 2
    $g.DrawRectangle($boxPenInner, ($boxRect.X + 5), ($boxRect.Y + 5), ($boxRect.Width - 10), ($boxRect.Height - 10))

    # Big Stamp Text (e.g. K.I.A.)
    $fntMain = New-Object System.Drawing.Font "Arial Black", 46.0, ([System.Drawing.FontStyle]::Bold)
    $sfMain = New-Object System.Drawing.StringFormat
    $sfMain.Alignment = [System.Drawing.StringAlignment]::Center
    $sfMain.LineAlignment = [System.Drawing.StringAlignment]::Center

    $mainRect = New-Object System.Drawing.RectangleF (-$boxW / 2.0), (-$boxH / 2.0 - 5), $boxW, ($boxH * 0.72)
    $g.DrawString($MainText, $fntMain, (New-Object System.Drawing.SolidBrush $InkCol), $mainRect, $sfMain)

    # Sub-status inside box
    $fntSub = New-Object System.Drawing.Font "Arial Black", 11.0, ([System.Drawing.FontStyle]::Bold)
    $subRect = New-Object System.Drawing.RectangleF (-$boxW / 2.0), ($boxH * 0.22), $boxW, 26
    $g.DrawString($SubText, $fntSub, (New-Object System.Drawing.SolidBrush $InkCol), $subRect, $sfMain)

    $g.Restore($state)

    # Weathered grunge stamp texture: random ink flecks & tiny voids
    $rnd = New-Object System.Random 12345
    for ($i = 0; $i -lt 120; $i++) {
        $fx = $rnd.Next(20, $S - 20)
        $fy = $rnd.Next(20, $S - 20)
        $d = [Math]::Sqrt(($fx - $cx) * ($fx - $cx) + ($fy - $cy) * ($fy - $cy))
        if ($d -lt $R + 15) {
            $sz = [float]($rnd.Next(1, 4))
            $alpha = $rnd.Next(60, 210)
            $fleckCol = [System.Drawing.Color]::FromArgb($alpha, $InkCol.R, $InkCol.G, $InkCol.B)
            $g.FillEllipse((New-Object System.Drawing.SolidBrush $fleckCol), [float]$fx, [float]$fy, $sz, $sz)
        }
    }

    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "wrote $OutPath"
}

# 1. Main KIA Logo: 作戦失敗 (KILLED IN ACTION)
$kiaText = U @(0x4F5C, 0x6226, 0x5931, 0x6557)
New-GameOverLogo -Text $kiaText -Sub "K I L L E D   I N   A C T I O N" `
    -TopCol ([System.Drawing.Color]::FromArgb(255, 255, 220, 220)) `
    -MidCol ([System.Drawing.Color]::FromArgb(255, 235, 40, 50)) `
    -BotCol ([System.Drawing.Color]::FromArgb(255, 125, 12, 22)) `
    -RibbonCol ([System.Drawing.Color]::FromArgb(255, 36, 18, 24)) `
    -RibbonBorderCol ([System.Drawing.Color]::FromArgb(255, 235, 45, 55)) `
    -OuterCol ([System.Drawing.Color]::FromArgb(255, 140, 16, 24)) `
    -OutPath (Join-Path $resDir "gameover_logo.png")

# 2. MIA Logo: 作戦時間切れ (MISSING IN ACTION)
$miaText = U @(0x4F5C, 0x6226, 0x6642, 0x9593, 0x5207, 0x308C)
New-GameOverLogo -Text $miaText -Sub "M I S S I N G   I N   A C T I O N" `
    -TopCol ([System.Drawing.Color]::FromArgb(255, 255, 238, 195)) `
    -MidCol ([System.Drawing.Color]::FromArgb(255, 245, 135, 25)) `
    -BotCol ([System.Drawing.Color]::FromArgb(255, 145, 55, 12)) `
    -RibbonCol ([System.Drawing.Color]::FromArgb(255, 42, 28, 16)) `
    -RibbonBorderCol ([System.Drawing.Color]::FromArgb(255, 245, 140, 30)) `
    -OuterCol ([System.Drawing.Color]::FromArgb(255, 150, 65, 15)) `
    -OutPath (Join-Path $resDir "gameover_logo_mia.png")

# 3. Tutorial Logo: 訓練失敗 (TRAINING FAILED)
$tutText = U @(0x8A13, 0x7DF4, 0x5931, 0x6557)
New-GameOverLogo -Text $tutText -Sub "T R A I N I N G   F A I L E D" `
    -TopCol ([System.Drawing.Color]::FromArgb(255, 240, 235, 255)) `
    -MidCol ([System.Drawing.Color]::FromArgb(255, 180, 50, 70)) `
    -BotCol ([System.Drawing.Color]::FromArgb(255, 90, 25, 40)) `
    -RibbonCol ([System.Drawing.Color]::FromArgb(255, 30, 22, 35)) `
    -RibbonBorderCol ([System.Drawing.Color]::FromArgb(255, 200, 70, 90)) `
    -OuterCol ([System.Drawing.Color]::FromArgb(255, 120, 25, 45)) `
    -OutPath (Join-Path $resDir "gameover_logo_tutorial.png")

# 4. Authentic Military Ink Stamp - KIA
New-MilitaryStamp -MainText "K. I. A." -SubText "STATUS: CASUALTY CONFIRMED" `
    -TopArc "CASUALTY REPORT" -BottomArc "TASK FORCE DUCKOV" `
    -InkCol ([System.Drawing.Color]::FromArgb(235, 225, 38, 48)) `
    -OutPath (Join-Path $resDir "gameover_stamp_kia.png")

# 5. Authentic Military Ink Stamp - MIA
New-MilitaryStamp -MainText "M. I. A." -SubText "STATUS: CONTACT SEVERED" `
    -TopArc "RAID TIME EXPIRED" -BottomArc "TASK FORCE DUCKOV" `
    -InkCol ([System.Drawing.Color]::FromArgb(235, 240, 130, 25)) `
    -OutPath (Join-Path $resDir "gameover_stamp_mia.png")
