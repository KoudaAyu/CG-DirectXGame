using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;

public class DiagramRenderer
{
    public static void Main()
    {
        Render4P("docs/slides/images/4P_original_diagram.png");
        Render5P("docs/slides/images/5P_original_diagram.png");
        Console.WriteLine("Render completed successfully!");
    }

    public static void Render4P(string path)
    {
        int w = 960;
        int h = 480;
        using (Bitmap bmp = new Bitmap(w, h))
        using (Graphics g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            // 背景
            using (Brush bg = new SolidBrush(Color.FromArgb(20, 23, 28)))
            {
                g.FillRectangle(bg, 0, 0, w, h);
            }

            // 外枠青
            using (Pen borderPen = new Pen(Color.FromArgb(0, 136, 255), 2))
            {
                g.DrawRectangle(borderPen, 10, 10, w - 20, h - 20);
            }

            using (Font titleFont = new Font("Meiryo", 14, FontStyle.Bold))
            using (Font labelFont = new Font("Meiryo", 11, FontStyle.Regular))
            using (Font boldFont = new Font("Meiryo", 11, FontStyle.Bold))
            using (Font badgeFont = new Font("Meiryo", 12, FontStyle.Bold))
            using (Brush cyanBrush = new SolidBrush(Color.FromArgb(0, 180, 216)))
            using (Brush whiteBrush = new SolidBrush(Color.White))
            using (Brush grayBrush = new SolidBrush(Color.FromArgb(140, 155, 170)))
            using (Brush redBrush = new SolidBrush(Color.FromArgb(255, 100, 100)))
            using (Brush lightRedBrush = new SolidBrush(Color.FromArgb(255, 204, 204)))
            {
                // タイトル
                g.DrawString("◆ 従来の離散判定（球 / 現在位置判定）", titleFont, cyanBrush, 25, 25);

                // 中央の木のフェンス
                int wallX = 480;
                int wallY = 80;
                int wallW = 44;
                int wallH = 250;

                using (Brush wallBg = new SolidBrush(Color.FromArgb(43, 31, 22)))
                using (Pen wallPen = new Pen(Color.FromArgb(201, 122, 62), 2))
                {
                    g.FillRectangle(wallBg, wallX - wallW / 2, wallY, wallW, wallH);
                    g.DrawRectangle(wallPen, wallX - wallW / 2, wallY, wallW, wallH);
                }

                // 壁ラベル
                using (Brush woodBrush = new SolidBrush(Color.FromArgb(224, 159, 103)))
                {
                    StringFormat sf = new StringFormat { Alignment = StringAlignment.Center };
                    g.DrawString("木のフェンス", boldFont, woodBrush, wallX, wallY - 26, sf);
                    g.DrawString("厚み: 40cm", labelFont, grayBrush, wallX, wallY + wallH + 6, sf);
                }

                // 弾丸1 (Frame 1)
                int b1X = 160;
                int bY = 200;
                int r = 16;

                using (Brush bBrush = new SolidBrush(Color.FromArgb(0, 180, 216)))
                using (Pen bPen = new Pen(Color.White, 2))
                {
                    g.FillEllipse(bBrush, b1X - r, bY - r, r * 2, r * 2);
                    g.DrawEllipse(bPen, b1X - r, bY - r, r * 2, r * 2);
                }

                // 弾丸2 (Frame 2)
                int b2X = 800;
                using (Brush bBrush = new SolidBrush(Color.FromArgb(0, 180, 216)))
                using (Pen bPen = new Pen(Color.White, 2))
                {
                    g.FillEllipse(bBrush, b2X - r, bY - r, r * 2, r * 2);
                    g.DrawEllipse(bPen, b2X - r, bY - r, r * 2, r * 2);
                }

                // ラベル
                StringFormat sfCenter = new StringFormat { Alignment = StringAlignment.Center };
                g.DrawString("判定: 接触なし", labelFont, grayBrush, b1X, bY - 38, sfCenter);
                g.DrawString("Frame 1\n壁の手前に位置", boldFont, cyanBrush, b1X, bY + 22, sfCenter);

                g.DrawString("判定: 接触なし", labelFont, grayBrush, b2X, bY - 38, sfCenter);
                g.DrawString("Frame 2\n壁の向こうへワープ", boldFont, cyanBrush, b2X, bY + 22, sfCenter);

                // 赤い破線矢印
                using (Pen arrowPen = new Pen(Color.FromArgb(255, 77, 77), 2))
                {
                    arrowPen.DashStyle = DashStyle.Dash;
                    arrowPen.CustomEndCap = new AdjustableArrowCap(6, 6, true);
                    g.DrawLine(arrowPen, b1X + r + 4, bY, b2X - r - 4, bY);
                }

                g.DrawString("1フレームで約80cm移動（板を飛び越える）", labelFont, redBrush, wallX - 80, bY - 26, sfCenter);

                // 赤い吹き出しバッジ
                int badgeW = 460;
                int badgeH = 65;
                int badgeX = wallX - badgeW / 2;
                int badgeY = 380;

                using (Brush badgeBg = new SolidBrush(Color.FromArgb(179, 36, 36)))
                using (Pen badgePen = new Pen(Color.FromArgb(255, 102, 102), 1))
                {
                    g.FillRectangle(badgeBg, badgeX, badgeY, badgeW, badgeH);
                    g.DrawRectangle(badgePen, badgeX, badgeY, badgeW, badgeH);
                }

                g.DrawString("【すり抜け（トンネリング）発生！】", badgeFont, whiteBrush, wallX, badgeY + 10, sfCenter);
                g.DrawString("板が存在する空間を一度も踏まないため、壁を完全に素通りする", labelFont, lightRedBrush, wallX, badgeY + 36, sfCenter);
            }

            bmp.Save(path, ImageFormat.Png);
        }
    }

    public static void Render5P(string path)
    {
        int w = 960;
        int h = 480;
        using (Bitmap bmp = new Bitmap(w, h))
        using (Graphics g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            // 背景
            using (Brush bg = new SolidBrush(Color.FromArgb(20, 23, 28)))
            {
                g.FillRectangle(bg, 0, 0, w, h);
            }

            // 外枠緑
            using (Pen borderPen = new Pen(Color.FromArgb(0, 187, 102), 2))
            {
                g.DrawRectangle(borderPen, 10, 10, w - 20, h - 20);
            }

            using (Font titleFont = new Font("Meiryo", 14, FontStyle.Bold))
            using (Font labelFont = new Font("Meiryo", 11, FontStyle.Regular))
            using (Font boldFont = new Font("Meiryo", 11, FontStyle.Bold))
            using (Font badgeFont = new Font("Meiryo", 12, FontStyle.Bold))
            using (Brush greenBrush = new SolidBrush(Color.FromArgb(0, 221, 136)))
            using (Brush cyanBrush = new SolidBrush(Color.FromArgb(0, 180, 216)))
            using (Brush whiteBrush = new SolidBrush(Color.White))
            using (Brush grayBrush = new SolidBrush(Color.FromArgb(140, 155, 170)))
            using (Brush lightGreenBrush = new SolidBrush(Color.FromArgb(204, 255, 221)))
            {
                StringFormat sfCenter = new StringFormat { Alignment = StringAlignment.Center };

                // タイトル
                g.DrawString("◆ CCD（連続衝突判定）：線分スイープによる完全交差判定", titleFont, greenBrush, 25, 25);

                // 中央の木のフェンス
                int wallX = 480;
                int wallY = 80;
                int wallW = 44;
                int wallH = 250;

                using (Brush wallBg = new SolidBrush(Color.FromArgb(43, 31, 22)))
                using (Pen wallPen = new Pen(Color.FromArgb(201, 122, 62), 2))
                {
                    g.FillRectangle(wallBg, wallX - wallW / 2, wallY, wallW, wallH);
                    g.DrawRectangle(wallPen, wallX - wallW / 2, wallY, wallW, wallH);
                }

                // 壁ラベル
                using (Brush woodBrush = new SolidBrush(Color.FromArgb(224, 159, 103)))
                {
                    g.DrawString("木のフェンス", boldFont, woodBrush, wallX, wallY - 26, sfCenter);
                    g.DrawString("厚み: 40cm", labelFont, grayBrush, wallX, wallY + wallH + 6, sfCenter);
                }

                int b1X = 160;
                int bY = 200;
                int r = 16;
                int b2X = 800;
                int hitX = wallX - wallW / 2;

                // スイープ線分（緑太線）
                using (Pen sweepPen = new Pen(Color.FromArgb(0, 221, 136), 4))
                {
                    g.DrawLine(sweepPen, b1X, bY, hitX, bY);
                }

                // 仮想延長線（点線）
                using (Pen dotPen = new Pen(Color.FromArgb(85, 102, 119), 2))
                {
                    dotPen.DashStyle = DashStyle.Dot;
                    g.DrawLine(dotPen, hitX, bY, b2X, bY);
                }

                g.DrawString("移動線分 P(t) = P_prev + t * v", labelFont, greenBrush, (b1X + hitX) / 2, bY - 24, sfCenter);

                // 弾丸1 (Frame 1)
                using (Brush bBrush = new SolidBrush(Color.FromArgb(0, 180, 216)))
                using (Pen bPen = new Pen(Color.White, 2))
                {
                    g.FillEllipse(bBrush, b1X - r, bY - r, r * 2, r * 2);
                    g.DrawEllipse(bPen, b1X - r, bY - r, r * 2, r * 2);
                }

                // 仮想終点 (Frame 2)
                using (Pen ghostPen = new Pen(Color.FromArgb(102, 119, 136), 2))
                {
                    ghostPen.DashStyle = DashStyle.Dash;
                    g.DrawEllipse(ghostPen, b2X - r, bY - r, r * 2, r * 2);
                }

                // 衝突点（ヒットマーカー）
                using (Brush hitBrush = new SolidBrush(Color.FromArgb(255, 234, 0)))
                using (Pen hitPen = new Pen(Color.White, 2))
                {
                    g.FillEllipse(hitBrush, hitX - 10, bY - 10, 20, 20);
                    g.DrawEllipse(hitPen, hitX - 10, bY - 10, 20, 20);
                }

                // 衝突点ラベル（左上にオフセット）
                int tagW = 180;
                int tagH = 46;
                int tagX = hitX - tagW - 15;
                int tagY = bY - 70;
                using (Brush tagBg = new SolidBrush(Color.FromArgb(43, 40, 16)))
                using (Pen tagPen = new Pen(Color.FromArgb(255, 215, 0), 1))
                {
                    g.FillRectangle(tagBg, tagX, tagY, tagW, tagH);
                    g.DrawRectangle(tagPen, tagX, tagY, tagW, tagH);
                }
                using (Brush yellowBrush = new SolidBrush(Color.FromArgb(255, 230, 0)))
                {
                    g.DrawString("★ 表面交点 P_hit", boldFont, yellowBrush, tagX + tagW / 2, tagY + 5, sfCenter);
                    g.DrawString("(媒介変数 t_min を算出)", labelFont, yellowBrush, tagX + tagW / 2, tagY + 24, sfCenter);
                }

                // ラベル
                g.DrawString("始点 P_prev\n(Frame 1)", boldFont, cyanBrush, b1X, bY + 22, sfCenter);
                g.DrawString("終点 P_curr\n(仮想到達点)", labelFont, grayBrush, b2X, bY + 22, sfCenter);

                // 緑の吹き出しバッジ
                int badgeW = 540;
                int badgeH = 65;
                int badgeX = wallX - badgeW / 2;
                int badgeY = 380;

                using (Brush badgeBg = new SolidBrush(Color.FromArgb(27, 99, 59)))
                using (Pen badgePen = new Pen(Color.FromArgb(51, 204, 119), 1))
                {
                    g.FillRectangle(badgeBg, badgeX, badgeY, badgeW, badgeH);
                    g.DrawRectangle(badgePen, badgeX, badgeY, badgeW, badgeH);
                }

                g.DrawString("【表面で100%確実にキャッチ！】", badgeFont, whiteBrush, wallX, badgeY + 10, sfCenter);
                g.DrawString("線分と板の交差方程式を解き、表面で即座に弾丸破棄＆木片スパーク発生", labelFont, lightGreenBrush, wallX, badgeY + 36, sfCenter);
            }

            bmp.Save(path, ImageFormat.Png);
        }
    }
}
