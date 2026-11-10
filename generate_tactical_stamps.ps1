Add-Type -AssemblyName System.Drawing

$resDir = Join-Path (Get-Location).Path "project\Resources"
if (-not (Test-Path $resDir)) {
    New-Item -ItemType Directory -Path $resDir -Force | Out-Null
}

# =============================================================================
# 1. 本格ミリタリー認印スタンプ (Authentic Military Casualty Dossier Stamp)
# =============================================================================
function New-ClassifiedMilitaryStamp {
    param(
        [string]$MainCode,      # "K . I . A ." or "M . I . A ."
        [string]$FullTitle,     # "K I L L E D   I N   A C T I O N"
        [string]$TopHeader,     # "CASUALTY DOSSIER // TOP SECRET"
        [string]$SubStatus,     # "STATUS: DECEASED // VERIFIED"
        [string]$ArchiveCode,   # "FILE #TF-DUCKOV-892 // SEC-01"
        [System.Drawing.Color]$InkCol,
        [string]$OutPath
    )

    $W = 512; $H = 340
    $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $cx = $W / 2.0; $cy = $H / 2.0

    # スタンプ全体のリアルな傾き（-7.5度）
    $state = $g.Save()
    $g.TranslateTransform($cx, $cy)
    $g.RotateTransform(-7.5)

    $boxW = 440.0; $boxH = 260.0
    $bx = -$boxW / 2.0
    $by = -$boxH / 2.0

    # 1. わずかなインク滲み下地（不透明度18%）
    $bgBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(30, $InkCol.R, $InkCol.G, $InkCol.B))
    $g.FillRectangle($bgBrush, $bx, $by, $boxW, $boxH)

    # 2. 太い外枠（ミリタリーステンシル外枠）
    $outerPen = New-Object System.Drawing.Pen $InkCol, 7.0
    $g.DrawRectangle($outerPen, $bx, $by, $boxW, $boxH)

    # 3. 内側の二重極細ヘアライン
    $innerPen = New-Object System.Drawing.Pen $InkCol, 2.0
    $g.DrawRectangle($innerPen, ($bx + 6.0), ($by + 6.0), ($boxW - 12.0), ($boxH - 12.0))

    # 四隅のミリタリー補強コーナー
    $cornerLen = 22.0
    $cornerPen = New-Object System.Drawing.Pen $InkCol, 4.0
    # 左上
    $g.DrawLine($cornerPen, ($bx + 6.0), ($by + 6.0 + $cornerLen), ($bx + 6.0 + $cornerLen), ($by + 6.0))
    # 右上
    $g.DrawLine($cornerPen, ($bx + $boxW - 6.0 - $cornerLen), ($by + 6.0), ($bx + $boxW - 6.0), ($by + 6.0 + $cornerLen))
    # 左下
    $g.DrawLine($cornerPen, ($bx + 6.0), ($by + $boxH - 6.0 - $cornerLen), ($bx + 6.0 + $cornerLen), ($by + $boxH - 6.0))
    # 右下
    $g.DrawLine($cornerPen, ($bx + $boxW - 6.0 - $cornerLen), ($by + $boxH - 6.0), ($bx + $boxW - 6.0), ($by + $boxH - 6.0 - $cornerLen))

    $brushInk = New-Object System.Drawing.SolidBrush $InkCol
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = [System.Drawing.StringAlignment]::Center
    $sf.LineAlignment = [System.Drawing.StringAlignment]::Center

    # 4. 上部ヘッダー（TOP SECRET // CASUALTY DOSSIER）
    $fntTop = New-Object System.Drawing.Font "Impact", 13.0, ([System.Drawing.FontStyle]::Regular)
    $g.DrawString($TopHeader, $fntTop, $brushInk, 0.0, ($by + 24.0), $sf)

    # 上部区切りライン（太線 ＋ 細線）
    $divPen1 = New-Object System.Drawing.Pen $InkCol, 3.0
    $g.DrawLine($divPen1, ($bx + 18.0), ($by + 38.0), ($bx + $boxW - 18.0), ($by + 38.0))
    $divPen2 = New-Object System.Drawing.Pen $InkCol, 1.2
    $g.DrawLine($divPen2, ($bx + 18.0), ($by + 43.0), ($bx + $boxW - 18.0), ($by + 43.0))

    # 5. メイン巨大英字 "K . I . A ." または "M . I . A ."（超ド迫力ステンシル風インパクト）
    # Dela Gothic One があれば使用、なければ Impact
    $mainFontName = "Impact"
    try {
        $testFf = New-Object System.Drawing.FontFamily "Dela Gothic One"
        $mainFontName = "Dela Gothic One"
    } catch {}

    $fntMain = New-Object System.Drawing.Font $mainFontName, 76.0, ([System.Drawing.FontStyle]::Regular)
    $mainRect = New-Object System.Drawing.RectangleF (-$boxW / 2.0), ($by + 38.0), $boxW, 118.0
    $g.DrawString($MainCode, $fntMain, $brushInk, $mainRect, $sf)

    # ステンシルの切れ込みスリット（ミリタリーステンシル特有の横スリット）
    $slitPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 30, $InkCol.R, $InkCol.G), 4.0)
    # 文字の中央を横切るステンシルブリッジ
    $slitY = $by + 97.0
    $g.DrawLine((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(240, 255, 255, 255)), 3.5), ($bx + 35.0), $slitY, ($bx + $boxW - 35.0), $slitY)

    # 下部区切りライン
    $g.DrawLine($divPen2, ($bx + 18.0), ($by + 162.0), ($bx + $boxW - 18.0), ($by + 162.0))
    $g.DrawLine($divPen1, ($bx + 18.0), ($by + 167.0), ($bx + $boxW - 18.0), ($by + 167.0))

    # 6. フルタイトル "K I L L E D   I N   A C T I O N"
    $fntFull = New-Object System.Drawing.Font "Impact", 16.0, ([System.Drawing.FontStyle]::Regular)
    $g.DrawString($FullTitle, $fntFull, $brushInk, 0.0, ($by + 186.0), $sf)

    # 7. 最下部サブステータス ＆ アーカイブコード
    $fntSub = New-Object System.Drawing.Font "Consolas", 10.5, ([System.Drawing.FontStyle]::Bold)
    $g.DrawString($SubStatus, $fntSub, $brushInk, 0.0, ($by + 212.0), $sf)

    $fntCode = New-Object System.Drawing.Font "Consolas", 9.0, ([System.Drawing.FontStyle]::Regular)
    $g.DrawString($ArchiveCode, $fntCode, $brushInk, 0.0, ($by + 234.0), $sf)

    $g.Restore($state)

    # 8. 本格ラバースタンプの風化・掠れ・インク飛沫（Gritty Ink Distress）
    $rnd = New-Object System.Random 74829
    # (a) インクの微細な掠れ・飛び散り（有機的ノイズ）
    for ($i = 0; $i -lt 450; $i++) {
        $fx = $rnd.Next(15, $W - 15)
        $fy = $rnd.Next(15, $H - 15)
        $sz = [float]($rnd.Next(1, 4))
        $a = $rnd.Next(40, 180)
        $col = [System.Drawing.Color]::FromArgb($a, $InkCol.R, $InkCol.G, $InkCol.B)
        $g.FillEllipse((New-Object System.Drawing.SolidBrush $col), [float]$fx, [float]$fy, $sz, $sz)
    }

    # (b) スタンプ特有のエッジインク溜まり・乾燥掠れ
    for ($i = 0; $i -lt 30; $i++) {
        $vx = $rnd.Next(40, $W - 40)
        $vy = $rnd.Next(30, $H - 30)
        $vw = $rnd.Next(10, 45)
        $vh = $rnd.Next(2, 6)
        $g.FillEllipse((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(25, 0, 0, 0))), [float]$vx, [float]$vy, [float]$vw, [float]$vh)
    }

    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Generated Stamp: $OutPath"
}

# =============================================================================
# 2. タクティカル・ミリタリーロゴ (Heavy Industrial Titanium Title Logo)
# =============================================================================
function New-TacticalMilitaryLogo {
    param(
        [string]$MainJpText,    # e.g. "作 戦 失 敗"
        [string]$SubEnText,     # e.g. "K I L L E D   I N   A C T I O N"
        [string]$TopCodeTag,    # e.g. "/// TACTICAL CASUALTY REPORT /// OP-04"
        [System.Drawing.Color]$AccentCol,
        [System.Drawing.Color]$TopGradCol,
        [System.Drawing.Color]$BotGradCol,
        [string]$OutPath
    )

    $W = 1000; $H = 260
    $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    # フォント選定：Dela Gothic One が最優先（超極太ミリタリー調）、無ければ Impact / Meiryo Bold
    $fontName = "Impact"
    try {
        $testFf = New-Object System.Drawing.FontFamily "Dela Gothic One"
        $fontName = "Dela Gothic One"
    } catch {
        try {
            $testFf = New-Object System.Drawing.FontFamily "BIZ UDPGothic"
            $fontName = "BIZ UDPGothic"
        } catch {}
    }

    $ff = New-Object System.Drawing.FontFamily $fontName
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $sf = [System.Drawing.StringFormat]::GenericTypographic

    # メイン日本語テキストのパス生成
    $path.AddString($MainJpText, $ff, [int][System.Drawing.FontStyle]::Bold, 110.0, (New-Object System.Drawing.PointF 0, 0), $sf)
    $b = $path.GetBounds()

    # スケール＆センタリング配置
    $targetW = 820.0
    $s = $targetW / $b.Width
    if ($b.Height * $s -gt 135.0) {
        $s = 135.0 / $b.Height
    }
    $m = New-Object System.Drawing.Drawing2D.Matrix
    $m.Translate(-$b.X, -$b.Y)
    $m.Scale($s, $s, [System.Drawing.Drawing2D.MatrixOrder]::Append)
    $m.Shear(-0.10, 0, [System.Drawing.Drawing2D.MatrixOrder]::Append) # わずかなミリタリー傾斜（-10%）
    $path.Transform($m)
    $nb = $path.GetBounds()

    $cx = ($W - $nb.Width) / 2.0 - $nb.X
    $cy = 40.0 - $nb.Y
    $m2 = New-Object System.Drawing.Drawing2D.Matrix
    $m2.Translate($cx, $cy)
    $path.Transform($m2)
    $nb = $path.GetBounds()

    # (1) 上部アイブロウコードバー（/// TACTICAL CASUALTY REPORT ///）
    $fntTop = New-Object System.Drawing.Font "Consolas", 12.0, ([System.Drawing.FontStyle]::Bold)
    $sfCenter = New-Object System.Drawing.StringFormat
    $sfCenter.Alignment = [System.Drawing.StringAlignment]::Center
    $sfCenter.LineAlignment = [System.Drawing.StringAlignment]::Center
    $topBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(210, $AccentCol.R, $AccentCol.G, $AccentCol.B))
    $g.DrawString($TopCodeTag, $fntTop, $topBrush, ($W / 2.0), 20.0, $sfCenter)

    # (2) 重厚なブラックカーボン・ドロップシャドウ
    $sh = $path.Clone()
    $mSh = New-Object System.Drawing.Drawing2D.Matrix
    $mSh.Translate(0, 10)
    $sh.Transform($mSh)
    $shPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(230, 8, 6, 10)), 28
    $shPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($shPen, $sh)
    $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(240, 8, 6, 10))), $sh)

    # (3) チタン外枠ライン（3層のミリタリーアウトライン）
    # 外側アクセントライン
    $pOuter = New-Object System.Drawing.Pen $AccentCol, 18
    $pOuter.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pOuter, $path)

    # 黒ベゼルライン
    $pMid = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 18, 12, 16)), 10
    $pMid.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pMid, $path)

    # 内側ハイライトヘアライン
    $pInner = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(180, 255, 255, 255)), 2.5
    $pInner.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pInner, $path)

    # (4) グラデーション塗り（ブラッシュド・ミリタリーチタン）
    $rect = New-Object System.Drawing.RectangleF $nb.X, $nb.Y, $nb.Width, $nb.Height
    $lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $TopGradCol, $BotGradCol, 90.0
    $cb = New-Object System.Drawing.Drawing2D.ColorBlend 3
    $cb.Colors = @($TopGradCol, $AccentCol, $BotGradCol)
    $cb.Positions = @(0.0, 0.40, 1.0)
    $lg.InterpolationColors = $cb
    $g.FillPath($lg, $path)

    # (5) 上半分メタルスペキュラ（重厚な金属反射）
    $stateClip = $g.Save()
    $g.SetClip($path)
    $specRect = New-Object System.Drawing.RectangleF $nb.X, $nb.Y, $nb.Width, ($nb.Height * 0.45)
    $specBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $specRect, `
        ([System.Drawing.Color]::FromArgb(140, 255, 255, 255)), `
        ([System.Drawing.Color]::FromArgb(15, 255, 255, 255)), 90.0
    $g.FillRectangle($specBrush, $specRect)
    $g.Restore($stateClip)

    # (6) 下部ミリタリーリボンバー（角型タクティカルプレート ＋ 警告ハザードライン）
    $ribY = 192.0; $ribH = 46.0
    $ribX1 = 120.0; $ribX2 = 880.0
    $ribW = $ribX2 - $ribX1

    # プレート背景
    $plateBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(245, 14, 10, 16))
    $g.FillRectangle($plateBrush, $ribX1, $ribY, $ribW, $ribH)

    # プレート外枠（アクセントカラー）
    $platePen = New-Object System.Drawing.Pen $AccentCol, 2.5
    $g.DrawRectangle($platePen, $ribX1, $ribY, $ribW, $ribH)

    # 左右のミリタリーハザード警告ストライプ（///）
    $hPen = New-Object System.Drawing.Pen $AccentCol, 3.0
    for ($i = 0; $i -lt 4; $i++) {
        $hxL = $ribX1 + 14.0 + ($i * 12.0)
        $g.DrawLine($hPen, [float]$hxL, [float]($ribY + 8.0), [float]($hxL + 8.0), [float]($ribY + $ribH - 8.0))

        $hxR = $ribX2 - 50.0 + ($i * 12.0)
        $g.DrawLine($hPen, [float]$hxR, [float]($ribY + 8.0), [float]($hxR + 8.0), [float]($ribY + $ribH - 8.0))
    }

    # サブテキスト "K I L L E D   I N   A C T I O N"（洗練されたDIN/Impactボールド）
    $fntSub = New-Object System.Drawing.Font "Impact", 21.0, ([System.Drawing.FontStyle]::Regular)
    $subBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 255, 255))
    $subShadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(180, 0, 0, 0))
    $g.DrawString($SubEnText, $fntSub, $subShadow, ($W / 2.0 + 1.5), ($ribY + $ribH / 2.0 + 1.5), $sfCenter)
    $g.DrawString($SubEnText, $fntSub, $subBrush, ($W / 2.0), ($ribY + $ribH / 2.0), $sfCenter)

    $g.Dispose()
    $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Generated Logo: $OutPath"
}

# =============================================================================
# 全アセット生成実行
# =============================================================================

# 1. K.I.A. ミリタリースタンプ（深紅の血文字・機密解読スタンプ）
New-ClassifiedMilitaryStamp `
    -MainCode "K . I . A ." `
    -FullTitle "K I L L E D   I N   A C T I O N" `
    -TopHeader "CASUALTY DOSSIER // TOP SECRET" `
    -SubStatus "STATUS: CONFIRMED DECEASED // VERIFIED" `
    -ArchiveCode "RECORD ID: TF-DUCKOV-0892 // SEC-01" `
    -InkCol ([System.Drawing.Color]::FromArgb(245, 205, 28, 38)) `
    -OutPath (Join-Path $resDir "gameover_stamp_kia.png")

# 2. M.I.A. ミリタリースタンプ（ハザードアンバー・通信途絶スタンプ）
New-ClassifiedMilitaryStamp `
    -MainCode "M . I . A ." `
    -FullTitle "M I S S I N G   I N   A C T I O N" `
    -TopHeader "TACTICAL REPORT // EXTRACTION FAILED" `
    -SubStatus "STATUS: UNCONFIRMED // CONTACT SEVERED" `
    -ArchiveCode "RECORD ID: TF-DUCKOV-0414 // TIMEOUT" `
    -InkCol ([System.Drawing.Color]::FromArgb(245, 240, 130, 20)) `
    -OutPath (Join-Path $resDir "gameover_stamp_mia.png")

# 3. メイン作戦失敗ロゴ (KILLED IN ACTION)
New-TacticalMilitaryLogo `
    -MainJpText "作 戦 失 敗" `
    -SubEnText "K I L L E D   I N   A C T I O N" `
    -TopCodeTag "/// TASK FORCE DUCKOV /// TACTICAL CASUALTY REPORT ///" `
    -AccentCol ([System.Drawing.Color]::FromArgb(255, 225, 36, 48)) `
    -TopGradCol ([System.Drawing.Color]::FromArgb(255, 255, 215, 220)) `
    -BotGradCol ([System.Drawing.Color]::FromArgb(255, 110, 12, 20)) `
    -OutPath (Join-Path $resDir "gameover_logo.png")

# 4. 作戦時間切れロゴ (MISSING IN ACTION)
New-TacticalMilitaryLogo `
    -MainJpText "作 戦 時 間 切 れ" `
    -SubEnText "M I S S I N G   I N   A C T I O N" `
    -TopCodeTag "/// TASK FORCE DUCKOV /// EXTRACTION WINDOW EXPIRED ///" `
    -AccentCol ([System.Drawing.Color]::FromArgb(255, 245, 135, 25)) `
    -TopGradCol ([System.Drawing.Color]::FromArgb(255, 255, 235, 195)) `
    -BotGradCol ([System.Drawing.Color]::FromArgb(255, 130, 50, 10)) `
    -OutPath (Join-Path $resDir "gameover_logo_mia.png")

# 5. 訓練失敗ロゴ (TRAINING FAILED)
New-TacticalMilitaryLogo `
    -MainJpText "訓 練 未 達 成" `
    -SubEnText "T R A I N I N G   F A I L E D" `
    -TopCodeTag "/// BOOT CAMP CADET TRAINING /// QUALIFICATION FAILED ///" `
    -AccentCol ([System.Drawing.Color]::FromArgb(255, 190, 60, 80)) `
    -TopGradCol ([System.Drawing.Color]::FromArgb(255, 245, 235, 245)) `
    -BotGradCol ([System.Drawing.Color]::FromArgb(255, 85, 20, 35)) `
    -OutPath (Join-Path $resDir "gameover_logo_tutorial.png")
