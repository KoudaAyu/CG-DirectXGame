import os
import base64
import re
import math
from PIL import Image, ImageDraw, ImageFont

# 保存先ディレクトリ (リポジトリのResources直下に保存して相対パスで埋め込めるようにする)
output_dir = r"c:\Users\k024g\OneDrive\デスクトップ\Engine_ver2026\project\Resources\slides"
os.makedirs(output_dir, exist_ok=True)

# 共通の色定義 (GitHubダークテーマ準拠のおしゃれなフラット配色)
BG_COLOR = (22, 27, 34)       # #161b22 (スライドの背景色と完全一致)
TEXT_COLOR = (240, 246, 252)  # #f0f6fc (白文字)
BLUE = (88, 166, 255)         # #58a6ff
GREEN = (63, 185, 80)         # #3fb950
RED = (248, 81, 73)           # #f85149
ORANGE = (210, 153, 34)       # #d29922
GRAY = (48, 54, 61)           # #30363d
LIGHT_GRAY = (139, 148, 158)  # #8b949e
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
# CPU & GPU Labels
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
# 4. Spatial Hash Diagram (slide_spatial_hash.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# グリッドの描画
grid_size = 50
start_x, start_y = 50, 30
for i in range(5):
    draw.line([(start_x + i * grid_size, start_y), (start_x + i * grid_size, start_y + 4 * grid_size)], fill=GRAY)
    draw.line([(start_x, start_y + i * grid_size), (start_x + 4 * grid_size, start_y + i * grid_size)], fill=GRAY)
# グリッドセルラベル (グリッド枠に被らないよう配置)
draw.text((start_x + 8, start_y + 8), "Cell A", fill=LIGHT_GRAY, font=font_small)
draw.text((start_x + 58, start_y + 8), "Cell B", fill=LIGHT_GRAY, font=font_small)
# オブジェクト
draw.ellipse([start_x + 15, start_y + 15, start_x + 35, start_y + 35], fill=BLUE)  # Obj1 in Cell A
draw.ellipse([start_x + 20, start_y + 70, start_x + 40, start_y + 90], fill=BLUE)  # Obj2 in Cell A (下)
draw.ellipse([start_x + 165, start_y + 115, start_x + 185, start_y + 135], fill=GREEN) # Obj3 in Cell
# 衝突判定ライン
draw.line([start_x + 25, start_y + 35, start_x + 30, start_y + 70], fill=RED, width=2)
# フローテキスト (矢印の始点をX=270から離す)
draw_arrow(draw, (275, 125), (330, 125), BLUE)
draw.text((345, 60), "ワールド座標 / セルサイズ\n➔ セル所属判定\n➔ 同一・近隣セル内のみ判定\n➔ 計算量を O(N^2) から O(N) へ", fill=TEXT_COLOR, font=font_medium)
img.save(os.path.join(output_dir, "slide_spatial_hash.png"))

# ----------------------------------------------------
# 5. DOD vs OOP Diagram (slide_dod.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# OOP
draw.text((40, 20), "従来の設計 (OOP: オブジェクト指向)", fill=LIGHT_GRAY, font=font_large)
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
# 連続メモリ
draw.rectangle([40, 240, 610, 270], fill=GRAY, outline=GREEN, width=1)
draw.rectangle([41, 241, 200, 269], fill=GREEN)
draw.text((50, 248), "Data A (位置/コライダー)", fill=BG_COLOR, font=font_small)
draw.rectangle([201, 241, 360, 269], fill=GREEN)
draw.text((210, 248), "Data B (位置/コライダー)", fill=BG_COLOR, font=font_small)
draw.rectangle([361, 241, 520, 269], fill=GREEN)
draw.text((370, 248), "Data C (位置/コライダー)", fill=BG_COLOR, font=font_small)
draw.text((530, 248), "連続メモリ", fill=TEXT_COLOR, font=font_small)
img.save(os.path.join(output_dir, "slide_dod.png"))

# ----------------------------------------------------
# 6. Behavior Tree ＆ Blackboard Diagram (slide_bt_bb.png) - ★文字被りを完全に修正
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# ノード配置
draw.rounded_rectangle([180, 20, 290, 60], radius=4, fill=GRAY)
draw.text((198, 30), "Root Node", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([180, 85, 290, 125], radius=4, fill=GRAY)
draw.text((205, 95), "Selector", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([90, 160, 200, 200], radius=4, fill=GRAY)
draw.text((108, 170), "Sequence", fill=TEXT_COLOR, font=font_medium)
# Action(攻撃)の幅を120ピクセル(240➔360)に拡張して、文字『撃』が枠からはみ出さないように配置
draw.rounded_rectangle([240, 160, 365, 200], radius=4, fill=ORANGE)
draw.text((255, 170), "Action (攻撃)", fill=BG_COLOR, font=font_medium)
# 接続線 (X座標をジャストに設定して枠と枠を直接繋ぐ)
draw_arrow(draw, (235, 60), (235, 85), LIGHT_GRAY)
draw_arrow(draw, (210, 125), (160, 160), LIGHT_GRAY)
draw_arrow(draw, (260, 125), (290, 160), LIGHT_GRAY)
# Blackboard (箱を幅広180pxにしてパディング確保、X=440➔450に右シフトして余白確保)
draw.rounded_rectangle([450, 60, 630, 200], radius=8, fill=BLUE)
draw.text((465, 75), "Blackboard\n(共有メモリ)", fill=BG_COLOR, font=font_large)
# テキスト描画位置を矢印接続部分より上(Y=125)にずらして被りを完全排除
draw.text((465, 125), "・Target: Player\n・MoveSpeed: 5.0", fill=BG_COLOR, font=font_medium)
# 接続矢印：Actionの右端(365)からBlackboardの左端(450)を正確に接続 (箱の内部に侵入させない)
draw_arrow(draw, (365, 185), (450, 185), RED, width=2)
draw_arrow(draw, (450, 165), (365, 165), GREEN, width=2)
img.save(os.path.join(output_dir, "slide_bt_bb.png"))

# ----------------------------------------------------
# 7. BVH / AABB Tree (slide_bvh.png) - ★文字被りを修正
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# 1) 青い大きな箱 (Root AABB)
draw.rectangle([30, 40, 570, 240], fill=None, outline=BLUE, width=3)
draw.text((45, 48), "◆ Root AABB (モデル全体を包む箱)", fill=BLUE, font=font_medium)
# 2) 左の緑の箱 (Left AABB)
draw.rectangle([50, 75, 275, 220], fill=None, outline=GREEN, width=2)
draw.text((65, 83), "Left AABB (左側)", fill=GREEN, font=font_medium)
# 3) 右の橙の箱 (Right AABB)
draw.rectangle([305, 75, 550, 220], fill=None, outline=ORANGE, width=2)
draw.text((320, 83), "Right AABB (右側)", fill=ORANGE, font=font_medium)
# 4) 左箱の中の三角形ポリゴン
draw.polygon([(80, 190), (120, 110), (160, 180)], fill=GRAY, outline=LIGHT_GRAY)
draw.polygon([(180, 200), (210, 130), (250, 190)], fill=GRAY, outline=LIGHT_GRAY)
# 5) 右箱の中の三角形ポリゴン
draw.polygon([(340, 180), (380, 110), (420, 190)], fill=GRAY, outline=LIGHT_GRAY)
draw.polygon([(450, 200), (480, 120), (520, 185)], fill=GRAY, outline=LIGHT_GRAY)
# 6) レーザー光線 (テキストと線をスライド中央に配置)
draw.line([10, 140, 140, 140], fill=RED, width=3)
draw_arrow(draw, (140, 140), (200, 140), RED, width=3)
draw.text((10, 115), "レーザー光線", fill=RED, font=font_medium)
# 7) 右箱のスキップ説明 (箱の内側で文字がはみ出さないようにサイズ調整)
draw.rounded_rectangle([325, 115, 530, 185], radius=4, fill=(180, 50, 50))
draw.text((335, 125), "【右箱は非衝突】\n中身の計算を完全に\nスキップ (サボる) !", fill=WHITE, font=font_medium)
img.save(os.path.join(output_dir, "slide_bvh.png"))

print("すべての画像から文字被りを解消して再生成しました！")
