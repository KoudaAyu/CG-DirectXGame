import os
import sys

# 必要ライブラリの自動インストール
try:
    from reportlab.pdfgen import canvas
    from reportlab.lib.pagesizes import letter, landscape, A4
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont
    from reportlab.lib import colors
except ImportError:
    print("Installing reportlab...")
    import subprocess
    import sys
    subprocess.check_call([sys.executable, "-m", "pip", "install", "reportlab"])
    from reportlab.pdfgen import canvas
    from reportlab.lib.pagesizes import letter, landscape, A4
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont
    from reportlab.lib import colors

# 日本語フォント（MS Gothic）の登録
font_path = r"C:\Windows\Fonts\msgothic.ttc"
if not os.path.exists(font_path):
    font_path = r"C:\Windows\Fonts\meiryo.ttc" # 代替

pdfmetrics.registerFont(TTFont("MSGothic", font_path))

# --- 図面描画ヘルパー関数群 (AI専用) ---

def draw_state_machine_diagram(c, width, height):
    cx, cy = width - 160, height * 0.45
    
    # 背景枠
    c.setFillColor(colors.HexColor("#F7FAFC"))
    c.setStrokeColor(colors.HexColor("#CBD5E0"))
    c.setLineWidth(1)
    c.rect(cx - 120, cy - 110, 240, 220, fill=True, stroke=True)
    
    # 3つの状態の円 (Patrol, Investigate, Chase)
    # 1. Patrol (巡回)
    c.setFillColor(colors.HexColor("#3182CE")) # 青
    c.circle(cx, cy + 45, 25, fill=True, stroke=False)
    c.setFillColor(colors.white)
    c.setFont("MSGothic", 9)
    c.drawCentredString(cx, cy + 42, "Patrol")
    c.drawCentredString(cx, cy + 32, "(巡回)")
    
    # 2. Investigate (捜索)
    c.setFillColor(colors.HexColor("#DD6B20")) # オレンジ
    c.circle(cx + 55, cy - 35, 25, fill=True, stroke=False)
    c.setFillColor(colors.white)
    c.drawCentredString(cx + 55, cy - 38, "Investigate")
    c.drawCentredString(cx + 55, cy - 48, "(捜索)")
    
    # 3. Chase (戦闘)
    c.setFillColor(colors.HexColor("#E53E3E")) # 赤
    c.circle(cx - 55, cy - 35, 25, fill=True, stroke=False)
    c.setFillColor(colors.white)
    c.drawCentredString(cx - 55, cy - 38, "Chase")
    c.drawCentredString(cx - 55, cy - 48, "(戦闘)")
    
    # 遷移線の描画
    c.setStrokeColor(colors.HexColor("#718096"))
    c.setLineWidth(1.5)
    
    # Patrol -> Investigate (音検知)
    c.line(cx + 15, cy + 25, cx + 45, cy - 10)
    c.setFillColor(colors.HexColor("#718096"))
    c.circle(cx + 45, cy - 10, 2.5, fill=True, stroke=False)
    c.setFont("MSGothic", 8)
    c.drawString(cx + 34, cy + 12, "音を検知")
    
    # Investigate -> Patrol (タイムアップ)
    c.line(cx + 35, cy - 15, cx + 5, cy + 20)
    c.circle(cx + 5, cy + 20, 2.5, fill=True, stroke=False)
    c.drawString(cx + 4, cy - 2, "3〜4秒経過")
    
    # Patrol -> Chase (視認発見)
    c.line(cx - 10, cy + 25, cx - 45, cy - 10)
    c.circle(cx - 45, cy - 10, 2.5, fill=True, stroke=False)
    c.drawCentredString(cx - 48, cy + 12, "発見 (メーター1.0)")
    
    # Chase -> Investigate (見失う)
    c.line(cx - 30, cy - 35, cx + 30, cy - 35)
    c.circle(cx + 30, cy - 35, 2.5, fill=True, stroke=False)
    c.drawCentredString(cx, cy - 30, "視界から見失う")
    
    # Investigate -> Chase (捜索中に視認)
    c.line(cx + 30, cy - 30, cx - 30, cy - 30)
    c.circle(cx - 30, cy - 30, 2.5, fill=True, stroke=False)
    c.drawCentredString(cx, cy - 25, "捜索中に視認")
    
    # タイトル
    c.setFillColor(colors.HexColor("#1A365D"))
    c.setFont("MSGothic", 12)
    c.drawCentredString(cx, cy - 95, "敵AIのステート遷移モデル (FSM)")

def draw_ai_diagram(c, width, height):
    cx, cy = width - 160, height * 0.45
    
    # 背景枠
    c.setFillColor(colors.HexColor("#F7FAFC"))
    c.setStrokeColor(colors.HexColor("#CBD5E0"))
    c.setLineWidth(1)
    c.rect(cx - 120, cy - 110, 240, 220, fill=True, stroke=True)
    
    # 障害物
    c.setFillColor(colors.HexColor("#718096"))
    c.rect(cx - 30, cy - 40, 60, 80, fill=True, stroke=False)
    c.setFillColor(colors.white)
    c.setFont("MSGothic", 8)
    c.drawCentredString(cx, cy - 4, "障害物 (壁)")
    
    # 敵キャラ (赤)
    c.setFillColor(colors.HexColor("#E53E3E"))
    c.circle(cx - 80, cy + 40, 15, fill=True, stroke=False)
    c.setFillColor(colors.white)
    c.setFont("MSGothic", 8)
    c.drawCentredString(cx - 80, cy + 37, "敵AI")
    
    # 視界コーン (薄い赤/黄の扇形)
    c.saveState()
    c.translate(cx - 80, cy + 40)
    c.rotate(-45) # プレイヤー方向を向く
    c.setFillColor(colors.HexColor("#20E53E3E"))
    c.setStrokeColor(colors.HexColor("#E53E3E"))
    c.setLineWidth(1)
    p = c.beginPath()
    p.moveTo(0, 0)
    p.lineTo(90, -40)
    p.lineTo(90, 40)
    p.close()
    c.drawPath(p, fill=True, stroke=True)
    c.restoreState()
    
    # プレイヤーキャラ (青) - 障害物の陰
    c.setFillColor(colors.HexColor("#3182CE"))
    c.circle(cx + 60, cy - 40, 15, fill=True, stroke=False)
    c.setFillColor(colors.white)
    c.setFont("MSGothic", 8)
    c.drawCentredString(cx + 60, cy - 43, "Player")
    
    # プレイヤーの足音波紋 (青い同心円)
    c.setStrokeColor(colors.HexColor("#303182CE"))
    c.setLineWidth(1.5)
    c.setDash(3, 3)
    c.circle(cx + 60, cy - 40, 35, fill=False, stroke=True)
    c.circle(cx + 60, cy - 40, 60, fill=False, stroke=True)
    c.circle(cx + 60, cy - 40, 85, fill=False, stroke=True)
    c.setDash()
    
    # 視線レイキャスト（障害物に遮られる）
    c.setStrokeColor(colors.HexColor("#A0AEC0"))
    c.setLineWidth(1.5)
    c.line(cx - 80, cy + 40, cx - 10, cy + 5)
    c.setStrokeColor(colors.HexColor("#E53E3E"))
    c.setDash(2, 2)
    c.line(cx - 10, cy + 5, cx + 60, cy - 40)
    c.setDash()
    
    # テキスト説明
    c.setFillColor(colors.HexColor("#E53E3E"))
    c.setFont("MSGothic", 9)
    c.drawString(cx - 110, cy - 85, "・視覚: 障害物で遮断 (見えない)")
    c.setFillColor(colors.HexColor("#3182CE"))
    c.drawString(cx - 110, cy - 97, "・聴覚: 壁越しでも足音を検知")
    
    c.setFillColor(colors.HexColor("#1A365D"))
    c.setFont("MSGothic", 12)
    c.drawCentredString(cx, cy - 130, "AIのセンサーシステム概念")

# --- スライドPDF生成メイン処理 ---

def create_slide_pdf(filename):
    width, height = landscape(A4)
    c = canvas.Canvas(filename, pagesize=landscape(A4))
    
    primary_color = colors.HexColor("#1A365D")  # ダークブルー
    accent_color = colors.HexColor("#DD6B20")   # オレンジ
    text_color = colors.HexColor("#2D3748")     # ダークグレー
    bg_color = colors.HexColor("#F7FAFC")       # ライトグレー背景
    
    slides = [
        # スライド1: 表紙
        {
            "type": "title",
            "title": "3Dアクションゲームにおける\n敵AIの「自律索敵」と「行動制御」システムの実装",
            "subtitle": "意思決定ステートマシン、センサー検知、および追従・射撃制御",
            "author": "技術研究ポートフォリオ"
        },
        # スライド2: 作成するゲーム
        {
            "type": "content",
            "title": "1. 作成するゲームと敵AIの役割",
            "bullets": [
                "【ジャンル】",
                "  - 3Dシューティングゲーム",
                "【コンセプト: 自律的な敵AIとの駆け引き】",
                "  - プレイヤーと敵AIが遮蔽物を駆使して戦い合う緊張感のあるゲーム性",
                "【特徴: 敵AIの行動要求】",
                "  - 単純な突撃ではなく、状況に応じて索敵し、追いかけてくる知的なAI",
                "  - （※物資回収ではなく、AIとの純粋な戦闘をメインに設計）"
            ]
        },
        # スライド3: 完成しているもの：敵AIの基本システム
        {
            "type": "content",
            "title": "2. 完成しているもの：敵AIの基本システム",
            "bullets": [
                "【索敵の基礎システム】",
                "  - 敵キャラクターが自立して周囲の脅威を検知する基本ルーチン",
                "【警戒から戦闘への遷移】",
                "  - プレイヤーの接近や怪しい挙動（足音など）を検知して警戒度上昇",
                "  - 発見時には即座に攻撃（射撃）フェーズへ移行するステート制御",
                "【現在の実装範囲】",
                "  - 視覚コーンによる扇形索敵と、レイキャストによる遮蔽判定",
                "  - 音（足音・銃声）に反応して発生源を調べる基礎システム"
            ]
        },
        # スライド4: 敵AIのシステム (1)
        {
            "type": "content",
            "title": "3. 敵AIのシステム (1)：意思決定ステートマシン (FSM)",
            "diagram": "fsm",
            "bullets": [
                "【Patrol（巡回・監視状態）】",
                "  - 固定敵：その場で左右にスイング（首振り）して広範囲をスキャン",
                "  - 移動敵：指定されたPatrol A点・B点を往復移動しながら索敵",
                "【Investigate（捜索状態）】",
                "  - 銃声や足音を検知した場所へ移動し、数秒間その方向を注視して捜索",
                "  - プレイヤーを見失った際、最後に目撃したラストポジションを調べに向かう",
                "【Chase（戦闘状態）】",
                "  - プレイヤーを発見した状態。足を止めて射撃を行い、クールダウン毎に連射"
            ]
        },
        # スライド5: 敵AIのシステム (2)
        {
            "type": "content",
            "title": "4. 敵AIのシステム (2)：視覚・聴覚センサーの仕組み",
            "diagram": "ai",
            "bullets": [
                "【視認判定（Line of Sight）】",
                "  - 正面（視野角 65度・距離 10〜12m）の扇形エリアをリアルタイム監視",
                "  - 視界コーンに入ったプレイヤーと敵の間に遮蔽物があるかを判定",
                "【音響検知（HearNoise）】",
                "  - 障害物の裏など、視線が通らなくても一定範囲の足音や銃声を検知",
                "【警戒度（Detection Meter）の管理】",
                "  - プレイヤーとの距離に応じて警戒メーターが上昇（約0.6秒で最大に）",
                "  - メーター蓄積中は頭上に「？」、最大で「！」となりChaseへ遷移"
            ]
        },
        # スライド6: 敵AIのシステム (3)
        {
            "type": "content",
            "title": "5. 敵AIのシステム (3)：自律的な巡回と捜索移動",
            "bullets": [
                "【巡回（Patrol）の自律移動】",
                "  - `patrolA_` / `patrolB_` の座標間（距離10m）を往復歩行移動するロジック",
                "  - 目標地点へ接近（0.2m以内）すると、自動的にターゲット地点を反転",
                "【捜索（Investigate）のターゲット移動】",
                "  - 検知した音源座標や見失い座標（`investigateTarget_`）へ移動",
                "  - 通常のパトロールよりも少し早い「早歩き（速度1.1倍）」で移動",
                "  - 目的地に到達、または3〜4秒経過すると自動的にPatrol状態へ復帰"
            ]
        },
        # スライド7: 敵AIのシステム (4)
        {
            "type": "content",
            "title": "6. 敵AIのシステム (4)：旋回・追従と攻撃制御の仕組み",
            "bullets": [
                "【滑らかな旋回制御（FaceTarget）】",
                "  - ターゲット方向への角度（atan2）を求め、毎フレーム滑らかに補間",
                "  - 首振りのガタつきやジタバタを防ぐタイブレーカー（180度ターン処理）",
                "  - パトロール時は低速（3.0）、警戒・戦闘時は高速（12.0）で振り向く制御",
                "【距離を維持した追従（Chase）】",
                "  - プレイヤーと一定距離（4.5m）を保ちながら追従移動する追跡ロジック",
                "【射撃のクールダウン管理】",
                "  - 射撃間隔（1.8〜2.0秒）をフレーム非依存のタイマー（deltaTime）で制御"
            ]
        },
        # スライド8: 敵AIのシステム (5)
        {
            "type": "content",
            "title": "7. 敵AIのシステム (5)：カバーAIに向けた「視線判定の精密化」",
            "bullets": [
                "【AIから見たオブジェクトの『隙間』】",
                "  - 木のフェンスなど、見た目に隙間があるオブジェクトの裏に隠れる際、",
                "    「隙間からプレイヤーが見えるか」の視線判定が非常に重要になる",
                "【大雑把な判定によるAI認識のバグ】",
                "  - フェンス全体を巨大な箱（AABB）で判定すると、AIは「隙間からプレイヤーが",
                "    見えているはずなのに、壁の向こうだから見えない」と誤認識するバグが発生",
                "【解決策: 視角レイキャストへの対角OBBマルチコライダーの適用】",
                "  - フェンスの板の傾き（45° / -45°）に合わせた薄いコライダーを配置し、",
                "    AIの視認用レイキャストもこのコライダーと交差判定を行うことで隙間を検出"
            ]
        },
        # スライド9: 敵AIのシステム (6)
        {
            "type": "content",
            "title": "8. 敵AIのシステム (6)：AIのカバー防御を保証する判定同期",
            "bullets": [
                "【課題: 高速弾のすり抜けによるAI被弾の理不尽さ】",
                "  - 弾が高速な際、AIが遮蔽物に隠れている（カバー中）にもかかわらず、",
                "    1フレームで壁をワープして通り抜け、AIにダメージが当たってしまう問題",
                "【解決策: 連続衝突判定（CCD）の導入】",
                "  - 移動前後の点同士を結ぶ「線分」と遮蔽物の交差判定を行うシステム",
                "【効果】",
                "  - 遮蔽物による弾丸の防護判定が確実に機能し、AIが安全にカバーに隠れられる"
            ]
        },
        # スライド10: 今後の展望とロードマップ
        {
            "type": "content",
            "title": "9. 敵AIの課題（改善点）と今後のロードマップ",
            "diagram": "bvh",
            "bullets": [
                "【現状の課題・改善点（AIの弱み）】",
                "  - 遮蔽物を利用せず棒立ちのまま撃ち合うため、戦闘の戦術性が低い",
                "  - 壁やフェンスを自動で迂回できず、障害物に引っかかってしまう",
                "【今後どう改善していくか（段階的な開発計画）】",
                "  - ① カバーAIの実装：周囲の障害物を自動検知し、安全に隠れて射撃する",
                "  - ② 経路探索（NavMesh）：障害物を賢く迂回・回り込んでプレイヤーを追跡",
                "  - ③ グループ連携無線：1体が発見したら周囲の仲間に無線で位置を伝達",
                "  - ④ 計算最適化（BVH）：多数のAIが動作しても処理が重くならない負荷対策"
            ]
        }
    ]
    
    for i, slide in enumerate(slides):
        # 背景塗りつぶし
        c.setFillColor(bg_color)
        c.rect(0, 0, width, height, fill=True, stroke=False)
        
        if slide["type"] == "title":
            # タイトルスライドのデザイン
            c.setFillColor(primary_color)
            c.rect(0, height * 0.4, width, height * 0.6, fill=True, stroke=False)
            
            c.setFillColor(colors.white)
            c.setFont("MSGothic", 32)
            y = height * 0.75
            for line in slide["title"].split("\n"):
                c.drawCentredString(width * 0.5, y, line)
                y -= 45
                
            c.setFillColor(colors.HexColor("#CBD5E0"))
            c.setFont("MSGothic", 18)
            c.drawCentredString(width * 0.5, height * 0.48, slide["subtitle"])
            
            c.setFillColor(primary_color)
            c.setFont("MSGothic", 16)
            c.drawCentredString(width * 0.5, height * 0.25, slide["author"])
            
        else:
            # コンテンツスライドのデザイン
            # ヘッダーバー
            c.setFillColor(primary_color)
            c.rect(0, height - 70, width, 70, fill=True, stroke=False)
            
            # 左端のアクセント図形
            c.setFillColor(colors.HexColor("#3182CE"))
            p = c.beginPath()
            p.moveTo(0, height - 70)
            p.lineTo(25, height - 70)
            p.lineTo(55, height)
            p.lineTo(30, height)
            p.close()
            c.drawPath(p, fill=True, stroke=False)
            
            c.setFillColor(colors.HexColor("#48BB78"))
            p = c.beginPath()
            p.moveTo(15, height - 70)
            p.lineTo(40, height - 70)
            p.lineTo(70, height)
            p.lineTo(45, height)
            p.close()
            c.drawPath(p, fill=True, stroke=False)
            
            # ヘッダータイトル
            c.setFillColor(colors.white)
            c.setFont("MSGothic", 24)
            c.drawString(85, height - 46, slide["title"])
            
            # コンテンツ（箇条書き）
            c.setFillColor(text_color)
            c.setFont("MSGothic", 13.5)
            y = height - 120
            
            has_diagram = "diagram" in slide
            
            for bullet in slide["bullets"]:
                if bullet.startswith("【"):
                    c.setFillColor(accent_color)
                    c.setFont("MSGothic", 14.5)
                    y -= 10
                    c.drawString(50, y, bullet)
                    c.setFillColor(text_color)
                    c.setFont("MSGothic", 13)
                else:
                    c.drawString(70, y, bullet)
                y -= 25
                
            # 各スライドの図面を描画
            if has_diagram:
                if slide["diagram"] == "ai":
                    draw_ai_diagram(c, width, height)
                elif slide["diagram"] == "fsm":
                    draw_state_machine_diagram(c, width, height)
            
            # フッター
            c.setFillColor(colors.HexColor("#A0AEC0"))
            c.setFont("MSGothic", 10)
            c.drawString(40, 20, "自作ゲームエンジン 技術研究発表会 (第1回)")
            c.drawRightString(width - 40, 20, f"Page {i+1} / {len(slides)}")
            
        c.showPage()
        
    c.save()
    print(f"Successfully generated {filename}")

if __name__ == "__main__":
    create_slide_pdf("technical_research_presentation.pdf")
