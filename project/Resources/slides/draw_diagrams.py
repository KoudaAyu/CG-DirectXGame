import os
import base64
import re
import math
from PIL import Image, ImageDraw, ImageFont

# 保存先ディレクトリ (リポジトリのResources直下に保存して相対パスで埋め込めるようにする)
output_dir = r"c:\Users\k024g\OneDrive\デスクトップ\Engine_ver2026\project\Resources\slides"
os.makedirs(output_dir, exist_ok=True)

# 共通の色定義 (★暗い背景の上でも文字がはっきりと読めるように、高輝度のパステルカラーに調整)
BG_COLOR = (22, 27, 34)       # #161b22 (スライドの背景色と完全一致)
TEXT_COLOR = (240, 246, 252)  # #f0f6fc (白文字)
BLUE = (130, 200, 255)         # 明るいスカイブルー (視認性大幅アップ)
GREEN = (120, 230, 140)        # 明るいライムグリーン (視認性大幅アップ)
RED = (255, 110, 110)          # 明るい赤 (視認性大幅アップ)
ORANGE = (240, 180, 70)        # 明るいオレンジ (視認性大幅アップ)
GRAY = (48, 54, 61)           # #30363d
LIGHT_GRAY = (190, 200, 210)  # 明るいプラチナグレー (視認性大幅アップ)
WHITE = (255, 255, 255)

# フォントの読み込み (Windowsの標準フォント)
font_path = "C:\\Windows\\Fonts\\msgothic.ttc"
try:
    font_large = ImageFont.truetype(font_path, 20)
    font_medium = ImageFont.truetype(font_path, 15)
    font_small = ImageFont.truetype(font_path, 13)
except Exception:
    font_large = ImageFont.load_default()
    font_medium = ImageFont.load_default()
    font_small = ImageFont.load_default()

def draw_arrow(draw, start, end, color, width=2, head_size=7):
    # 矢印の線を引く
    draw.line([start, end], fill=color, width=width)
    # 矢印の頭を正確に計算して描画
    x1, y1 = start
    x2, y2 = end
    angle = math.atan2(y2 - y1, x2 - x1)
    # 右側のハネ
    rx = x2 - head_size * math.cos(angle - math.pi/6)
    ry = y2 - head_size * math.sin(angle - math.pi/6)
    # 左側のハネ
    lx = x2 - head_size * math.cos(angle + math.pi/6)
    ly = y2 - head_size * math.sin(angle + math.pi/6)
    draw.polygon([end, (rx, ry), (lx, ly)], fill=color)

# ----------------------------------------------------
# 1. System Architecture Diagram (slide_system.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 260), BG_COLOR)
draw = ImageDraw.Draw(img)
# Gameアプリ (幅広にしてパディング確保)
draw.rounded_rectangle([30, 90, 180, 150], radius=8, fill=BLUE)
draw.text((52, 110), "Game アプリ", fill=BG_COLOR, font=font_large)
# EngineContext (幅広にしてパディング確保)
draw.rounded_rectangle([230, 90, 410, 150], radius=8, fill=GREEN)
draw.text((252, 110), "EngineContext", fill=BG_COLOR, font=font_large)
# Subsystems
draw.rectangle([460, 20, 630, 240], fill=None, outline=GRAY, width=2)
draw.text((475, 30), "サブシステム群", fill=LIGHT_GRAY, font=font_medium)
draw.rounded_rectangle([475, 70, 615, 110], radius=4, fill=GRAY)
draw.text((485, 80), "CBアロケータ", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([475, 130, 615, 170], radius=4, fill=GRAY)
draw.text((480, 140), "衝突判定マネージャ", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([475, 190, 615, 230], radius=4, fill=GRAY)
draw.text((485, 200), "BehaviorTree AI", fill=TEXT_COLOR, font=font_medium)
# 矢印の接続座標をボックスの「外枠境界」に完全一致させる (X座標: 180➔230, 410➔460)
draw_arrow(draw, (180, 120), (230, 120), BLUE)
draw_arrow(draw, (410, 120), (460, 120), GREEN)
img.save(os.path.join(output_dir, "slide_system.png"))

# ----------------------------------------------------
# 2. Triple Buffering Diagram (slide_allocator.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# CPU & GPU Labels (高輝度の色でハッキリ表示)
draw.text((40, 30), "CPU 側 (データ書き込み)", fill=BLUE, font=font_large)
draw.text((430, 30), "GPU 側 (レンダリング)", fill=GREEN, font=font_large)
# アロケータ (幅広にしてテキストとの干渉を防ぐ)
draw.rounded_rectangle([30, 85, 190, 145], radius=6, fill=GRAY)
draw.text((45, 95), "ConstantBuffer\nAllocator", fill=TEXT_COLOR, font=font_medium)
# フレームバッファ (X座標を右に寄せて間隔を開ける)
draw.rounded_rectangle([250, 50, 380, 90], radius=4, fill=BLUE)
draw.text((265, 60), "Frame 0 (GPU)", fill=BG_COLOR, font=font_medium)
draw.rounded_rectangle([250, 110, 380, 150], radius=4, fill=GRAY)
draw.text((265, 120), "Frame 1 (CPU)", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([250, 170, 380, 210], radius=4, fill=GRAY)
draw.text((265, 180), "Frame 2 (待機)", fill=TEXT_COLOR, font=font_medium)
# GPU描画
draw.rounded_rectangle([450, 85, 610, 145], radius=6, fill=GREEN)
draw.text((470, 105), "GPU コマンド実行", fill=BG_COLOR, font=font_medium)
# 矢印接続：アロケータの右端(190)から各フレームの左端(250)へ繋ぐ (文字被りを完全に排除)
draw_arrow(draw, (190, 115), (250, 130), BLUE)
draw_arrow(draw, (380, 70), (450, 115), GREEN)
# 同期フェンス
draw.text((120, 240), "【GPUフェンス値と同期し、書き込みの衝突を防止】", fill=LIGHT_GRAY, font=font_medium)
img.save(os.path.join(output_dir, "slide_allocator.png"))

# ----------------------------------------------------
# 3. Stack Allocator Diagram (slide_stack.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 180), BG_COLOR)
draw = ImageDraw.Draw(img)
# メモリバー
draw.rectangle([50, 60, 600, 110], fill=GRAY, outline=LIGHT_GRAY, width=2)
# 使用中領域
draw.rectangle([52, 62, 350, 108], fill=BLUE)
draw.text((120, 78), "使用中領域 (O(1) アロケート)", fill=BG_COLOR, font=font_medium)
draw.text((450, 78), "未使用領域", fill=LIGHT_GRAY, font=font_medium)
# オフセットマーク (矢印を短くしテキストと被らないように下にずらす)
draw_arrow(draw, (350, 150), (350, 120), RED, width=3)
draw.text((310, 155), "現在オフセット", fill=RED, font=font_medium)
# 注記
draw.text((50, 20), "起動時に一括でプール確保 ➔ フレーム終了時に一括Reset()", fill=TEXT_COLOR, font=font_large)
img.save(os.path.join(output_dir, "slide_stack.png"))

# ----------------------------------------------------
# 4. Spatial Hash Diagram (slide_spatial_hash.png) - ★SFレーダー風のプロクオリティデザイン
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)

# グリッドのサイズ定義 (セル幅60px)
grid_size = 60
start_x, start_y = 40, 25
cols, rows = 4, 4

# ★セルA (列0, 行0) の背景を「スキャン中の近未来SF調ライトグリーン(35, 55, 45)」に！
# さらに、この区画の枠線だけ「太めのネオングリーン(100, 255, 150)」で囲ってレーダースキャン感を演出
NEON_GREEN = (100, 255, 150)
draw.rectangle([start_x, start_y, start_x + grid_size, start_y + grid_size], fill=(35, 55, 45))

# グリッドの一般枠線を描画 (控えめで美しい青暗いグレー)
GRID_LINE_COLOR = (45, 52, 62)
for i in range(cols + 1):
    draw.line([(start_x + i * grid_size, start_y), (start_x + i * grid_size, start_y + rows * grid_size)], fill=GRID_LINE_COLOR, width=1)
for i in range(rows + 1):
    draw.line([(start_x, start_y + i * grid_size), (start_x + cols * grid_size, start_y + i * grid_size)], fill=GRID_LINE_COLOR, width=1)

# セルAの枠だけネオングリーンで上書き強調
draw.rectangle([start_x, start_y, start_x + grid_size, start_y + grid_size], fill=None, outline=NEON_GREEN, width=2)

# セルラベル (セルAはネオングリーンでハイライト、セルBは薄いグレー)
draw.text((start_x + 6, start_y + 5), "セルA", fill=NEON_GREEN, font=font_small)
draw.text((start_x + grid_size + 6, start_y + 5), "セルB", fill=LIGHT_GRAY, font=font_small)

# ★オブジェクトの光る粒子化 (白枠の中にスカイブルーを塗り、中心に白いコア「核」を描く)
def draw_glow_particle(draw, center_x, center_y, color):
    r = 10
    # 外枠
    draw.ellipse([center_x - r, center_y - r, center_x + r, center_y + r], fill=color, outline=WHITE, width=1)
    # 中心コア
    draw.ellipse([center_x - 3, center_y - 3, center_x + 3, center_y + 3], fill=WHITE)

# セルA内の二つの青オブジェクト
draw_glow_particle(draw, start_x + 28, start_y + 32, BLUE)   # Obj1
draw_glow_particle(draw, start_x + 30, start_y + 90, BLUE)   # Obj2
# 遠隔セル内の緑オブジェクト
draw_glow_particle(draw, start_x + 205, start_y + 145, GREEN) # Obj3

# 衝突判定ライン (セルA内の2つの青オブジェクトを結ぶ赤いレーザー光線)
draw.line([start_x + 28, start_y + 32, start_x + 30, start_y + 90], fill=RED, width=2)

# フロー矢印と説明テキスト
draw_arrow(draw, (300, 125), (345, 125), BLUE, width=3, head_size=9)
draw.text((360, 60), "ワールド座標 / セルサイズ\n➔ セル所属判定\n➔ 同一・近隣セル内のみ判定\n➔ 計算量を O(N^2) から O(N) へ", fill=TEXT_COLOR, font=font_medium)

img.save(os.path.join(output_dir, "slide_spatial_hash.png"))

# ----------------------------------------------------
# 5. DOD vs OOP Diagram (slide_dod.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# OOP
draw.text((40, 20), "従来の設計 (OOP: オブジェクト指向)", fill=TEXT_COLOR, font=font_large)
draw.rounded_rectangle([30, 60, 150, 95], radius=4, fill=GRAY)
draw.text((42, 70), "Obj A (ポインタ)", fill=TEXT_COLOR, font=font_small)
draw.rounded_rectangle([190, 110, 310, 145], radius=4, fill=GRAY)
draw.text((202, 120), "Obj B (ポインタ)", fill=TEXT_COLOR, font=font_small)
draw.rounded_rectangle([30, 160, 150, 195], radius=4, fill=GRAY)
draw.text((42, 170), "Obj C (ポインタ)", fill=TEXT_COLOR, font=font_small)
# キャッシュミス矢印 (文字が被らないよう、枠の端と端を正確に接続)
draw_arrow(draw, (150, 77), (190, 118), RED)
draw_arrow(draw, (190, 137), (150, 177), RED)
draw.text((330, 118), "➔ メモリが散在しキャッシュミス多発", fill=RED, font=font_medium)
# DOD
draw.text((40, 210), "最適化設計 (DOD: データ指向設計)", fill=GREEN, font=font_large)

# 連続メモリ (全体をGREENで塗りつぶした一本のバー)
box_l, box_t, box_r, box_b = 40, 240, 610, 270
draw.rectangle([box_l, box_t, box_r, box_b], fill=GREEN, outline=BG_COLOR, width=1)

# 各ブロックの境界線（縦線）を等間隔で引く
draw.line([(180, 240), (180, 270)], fill=BG_COLOR, width=1)
draw.line([(320, 240), (320, 270)], fill=BG_COLOR, width=1)
draw.line([(460, 240), (460, 270)], fill=BG_COLOR, width=1)

# 文字情報を「Data A」「Data B」「Data C」とシンプルにして、
# 各ボックスの中央に配置
blocks_info = [
    (40, 180, "Data A"),
    (180, 320, "Data B"),
    (320, 460, "Data C"),
    (460, 610, "連続メモリ")
]

for left, right, text in blocks_info:
    try:
        w = draw.textlength(text, font=font_medium)
    except AttributeError:
        w = len(text.encode('utf-8')) * 7.5
    
    text_x = left + ((right - left) - w) / 2
    text_y = box_t + ((box_b - box_t) - 15) / 2
    
    draw.text((text_x, text_y), text, fill=BG_COLOR, font=font_medium)

img.save(os.path.join(output_dir, "slide_dod.png"))

# ----------------------------------------------------
# 6. Behavior Tree ＆ Blackboard Diagram (slide_bt_bb.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# ... [bt_bb のコードはそのまま維持]
draw.rounded_rectangle([180, 20, 290, 60], radius=4, fill=GRAY)
draw.text((198, 30), "Root Node", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([180, 85, 290, 125], radius=4, fill=GRAY)
draw.text((205, 95), "Selector", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([90, 160, 200, 200], radius=4, fill=GRAY)
draw.text((108, 170), "Sequence", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([240, 160, 365, 200], radius=4, fill=ORANGE)
draw.text((255, 170), "Action (攻撃)", fill=BG_COLOR, font=font_medium)
draw_arrow(draw, (235, 60), (235, 85), LIGHT_GRAY)
draw_arrow(draw, (210, 125), (160, 160), LIGHT_GRAY)
draw_arrow(draw, (260, 125), (290, 160), LIGHT_GRAY)
draw.rounded_rectangle([450, 60, 630, 200], radius=8, fill=BLUE)
draw.text((465, 75), "Blackboard\n(共有メモリ)", fill=BG_COLOR, font=font_large)
draw.text((465, 125), "・Target: Player\n・MoveSpeed: 5.0", fill=BG_COLOR, font=font_medium)
draw_arrow(draw, (365, 185), (450, 185), RED, width=2)
draw_arrow(draw, (450, 165), (365, 165), GREEN, width=2)
img.save(os.path.join(output_dir, "slide_bt_bb.png"))

# ----------------------------------------------------
# 7. BVH / AABB Tree (slide_bvh.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# ... [bvh のコードはそのまま維持]
draw.rectangle([30, 30, 570, 200], fill=None, outline=BLUE, width=3)
draw.text((45, 38), "◆ Root AABB (モデル全体を包む箱)", fill=BLUE, font=font_medium)
draw.rectangle([50, 65, 275, 185], fill=None, outline=GREEN, width=2)
draw.text((65, 73), "Left AABB (左側)", fill=GREEN, font=font_medium)
draw.rectangle([305, 65, 550, 185], fill=None, outline=ORANGE, width=2)
draw.text((320, 73), "Right AABB (右側)", fill=ORANGE, font=font_medium)
POLY_FILL = (210, 220, 235)
draw.polygon([(80, 165), (120, 95), (160, 155)], fill=POLY_FILL, outline=WHITE)
draw.polygon([(180, 175), (210, 110), (250, 165)], fill=POLY_FILL, outline=WHITE)
draw.polygon([(340, 155), (380, 95), (420, 165)], fill=POLY_FILL, outline=WHITE)
draw.polygon([(450, 175), (480, 110), (520, 160)], fill=POLY_FILL, outline=WHITE)
draw.line([10, 130, 140, 130], fill=RED, width=3)
draw_arrow(draw, (140, 130), (200, 130), RED, width=3)
draw.text((10, 105), "レーザー光線", fill=RED, font=font_medium)
box_left, box_top, box_right, box_bottom = 250, 210, 560, 260
draw.rounded_rectangle([box_left, box_top, box_right, box_bottom], radius=4, fill=(180, 50, 50))
text_content = "【右箱は非衝突】 ➔ 中身の計算を全スキップ！"
try:
    text_width = draw.textlength(text_content, font=font_small)
except AttributeError:
    text_width = len(text_content.encode('utf-8')) * 7.5
box_width = box_right - box_left
box_height = box_bottom - box_top
text_x = box_left + (box_width - text_width) / 2
text_y = box_top + (box_height - 13) / 2
draw.text((text_x, text_y), text_content, fill=WHITE, font=font_small)
img.save(os.path.join(output_dir, "slide_bvh.png"))

print("すべての画像の色とレイアウトを完璧に再生成しました！")
