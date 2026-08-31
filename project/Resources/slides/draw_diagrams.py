import os
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
    font_large = ImageFont.truetype(font_path, 22)
    font_medium = ImageFont.truetype(font_path, 16)
    font_small = ImageFont.truetype(font_path, 13)
except Exception:
    font_large = ImageFont.load_default()
    font_medium = ImageFont.load_default()
    font_small = ImageFont.load_default()

def draw_arrow(draw, start, end, color, width=2, head_size=8):
    # 矢印を描画するヘルパー関数
    draw.line([start, end], fill=color, width=width)
    # 矢印の頭
    x1, y1 = start
    x2, y2 = end
    import math
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
# Gameアプリ
draw.rounded_rectangle([30, 90, 170, 150], radius=8, fill=BLUE, outline=WHITE, width=1)
draw.text((50, 110), "Game アプリ", fill=BG_COLOR, font=font_large)
# EngineContext
draw.rounded_rectangle([230, 90, 400, 150], radius=8, fill=GREEN, outline=WHITE, width=1)
draw.text((250, 110), "EngineContext", fill=BG_COLOR, font=font_large)
# Subsystems
draw.rectangle([450, 20, 620, 240], fill=None, outline=GRAY, width=2)
draw.text((460, 30), "サブシステム群", fill=LIGHT_GRAY, font=font_medium)
draw.rounded_rectangle([470, 70, 600, 110], radius=4, fill=GRAY)
draw.text((480, 80), "CBアロケータ", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([470, 130, 600, 170], radius=4, fill=GRAY)
draw.text((480, 140), "衝突判定マネージャ", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([470, 190, 600, 230], radius=4, fill=GRAY)
draw.text((480, 200), "BehaviorTree AI", fill=TEXT_COLOR, font=font_medium)
# 矢印
draw_arrow(draw, (170, 120), (220, 120), BLUE)
draw_arrow(draw, (400, 120), (445, 120), GREEN)
img.save(os.path.join(output_dir, "slide_system.png"))

# ----------------------------------------------------
# 2. Triple Buffering Diagram (slide_allocator.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# CPU & GPU Labels
draw.text((40, 30), "CPU 側 (データ書き込み)", fill=BLUE, font=font_large)
draw.text((430, 30), "GPU 側 (レンダリング)", fill=GREEN, font=font_large)
# アロケータ
draw.rounded_rectangle([40, 80, 180, 140], radius=6, fill=GRAY)
draw.text((50, 100), "ConstantBuffer\nAllocator", fill=TEXT_COLOR, font=font_medium)
# フレームバッファ
draw.rounded_rectangle([250, 50, 370, 90], radius=4, fill=BLUE)
draw.text((270, 60), "Frame 0 (GPU)", fill=BG_COLOR, font=font_medium)
draw.rounded_rectangle([250, 110, 370, 150], radius=4, fill=GRAY)
draw.text((270, 120), "Frame 1 (CPU)", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([250, 170, 370, 210], radius=4, fill=GRAY)
draw.text((270, 180), "Frame 2 (待機)", fill=TEXT_COLOR, font=font_medium)
# GPU描画
draw.rounded_rectangle([440, 80, 580, 140], radius=6, fill=GREEN)
draw.text((460, 100), "GPU コマンド実行", fill=BG_COLOR, font=font_medium)
# 矢印
draw_arrow(draw, (180, 110), (240, 130), BLUE)
draw_arrow(draw, (370, 70), (435, 100), GREEN)
# 同期フェンス
draw.text((120, 230), "【GPUフェンス値と同期し、書き込みの衝突を防止】", fill=LIGHT_GRAY, font=font_medium)
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
draw.text((120, 75), "使用中領域 (O(1) アロケート)", fill=BG_COLOR, font=font_medium)
draw.text((450, 75), "未使用領域", fill=LIGHT_GRAY, font=font_medium)
# オフセットマーク
draw_arrow(draw, (350, 145), (350, 115), RED, width=3)
draw.text((310, 150), "現在オフセット", fill=RED, font=font_medium)
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
# グリッドセルラベル
draw.text((start_x + 10, start_y + 10), "Cell A", fill=LIGHT_GRAY, font=font_small)
draw.text((start_x + 60, start_y + 10), "Cell B", fill=LIGHT_GRAY, font=font_small)
# オブジェクト
draw.ellipse([start_x + 15, start_y + 15, start_x + 35, start_y + 35], fill=BLUE)  # Obj1 in Cell A
draw.ellipse([start_x + 20, start_y + 70, start_x + 40, start_y + 90], fill=BLUE)  # Obj2 in Cell A (下)
draw.ellipse([start_x + 165, start_y + 115, start_x + 185, start_y + 135], fill=GREEN) # Obj3 in Cell
# 衝突判定ライン
draw.line([start_x + 25, start_y + 25, start_x + 30, start_y + 80], fill=RED, width=2)
# フローテキスト
draw_arrow(draw, (270, 110), (330, 110), BLUE)
draw.text((345, 70), "ワールド座標 / セルサイズ\n➔ セル所属判定\n➔ 同一・近隣セル内のみ判定\n➔ 計算量を O(N^2) から O(N) へ", fill=TEXT_COLOR, font=font_medium)
img.save(os.path.join(output_dir, "slide_spatial_hash.png"))

# ----------------------------------------------------
# 5. DOD vs OOP Diagram (slide_dod.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# OOP
draw.text((40, 20), "従来の設計 (OOP: オブジェクト指向)", fill=LIGHT_GRAY, font=font_large)
draw.rounded_rectangle([40, 60, 130, 95], radius=4, fill=GRAY)
draw.text((50, 70), "Obj A (ポインタ)", fill=TEXT_COLOR, font=font_small)
draw.rounded_rectangle([180, 110, 270, 145], radius=4, fill=GRAY)
draw.text((190, 120), "Obj B (ポインタ)", fill=TEXT_COLOR, font=font_small)
draw.rounded_rectangle([30, 160, 120, 195], radius=4, fill=GRAY)
draw.text((40, 170), "Obj C (ポインタ)", fill=TEXT_COLOR, font=font_small)
# キャッシュミス矢印
draw_arrow(draw, (130, 77), (180, 127), RED)
draw_arrow(draw, (180, 127), (120, 177), RED)
draw.text((200, 70), "➔ メモリが散在しキャッシュミス多発", fill=RED, font=font_small)
# DOD
draw.text((40, 210), "最適化設計 (DOD: データ指向設計)", fill=GREEN, font=font_large)
# 連続メモリ
draw.rectangle([40, 240, 610, 270], fill=GRAY, outline=GREEN, width=1)
draw.rectangle([41, 241, 190, 269], fill=GREEN)
draw.text((50, 248), "Data A (位置/コライダー)", fill=BG_COLOR, font=font_small)
draw.rectangle([191, 241, 340, 269], fill=GREEN)
draw.text((200, 248), "Data B (位置/コライダー)", fill=BG_COLOR, font=font_small)
draw.rectangle([341, 241, 490, 269], fill=GREEN)
draw.text((350, 248), "Data C (位置/コライダー)", fill=BG_COLOR, font=font_small)
draw.text((500, 248), "連続メモリ", fill=TEXT_COLOR, font=font_small)
img.save(os.path.join(output_dir, "slide_dod.png"))

# ----------------------------------------------------
# 6. Behavior Tree ＆ Blackboard Diagram (slide_bt_bb.png)
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# ノード
draw.rounded_rectangle([180, 30, 280, 70], radius=4, fill=GRAY)
draw.text((195, 40), "Root Node", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([180, 100, 280, 140], radius=4, fill=GRAY)
draw.text((190, 110), "Selector", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([100, 170, 200, 210], radius=4, fill=GRAY)
draw.text((115, 180), "Sequence", fill=TEXT_COLOR, font=font_medium)
draw.rounded_rectangle([250, 170, 350, 210], radius=4, fill=ORANGE)
draw.text((260, 180), "Action (攻撃)", fill=BG_COLOR, font=font_medium)
# 接続線
draw_arrow(draw, (230, 70), (230, 95), LIGHT_GRAY)
draw_arrow(draw, (210, 140), (160, 165), LIGHT_GRAY)
draw_arrow(draw, (250, 140), (290, 165), LIGHT_GRAY)
# Blackboard
draw.rounded_rectangle([440, 90, 590, 170], radius=8, fill=BLUE, outline=WHITE, width=1)
draw.text((455, 100), "Blackboard\n(共有メモリ)", fill=BG_COLOR, font=font_large)
draw.text((450, 145), "・Target: Player\n・MoveSpeed: 5.0", fill=BG_COLOR, font=font_small)
# アクションからの読み書き線
draw_arrow(draw, (350, 190), (440, 160), RED, width=2)
draw_arrow(draw, (440, 140), (350, 180), GREEN, width=2)
img.save(os.path.join(output_dir, "slide_bt_bb.png"))

# ----------------------------------------------------
# 7. BVH / AABB Tree (slide_bvh.png) - ★参考スライド16枚目の完全再現
# ----------------------------------------------------
img = Image.new("RGB", (650, 280), BG_COLOR)
draw = ImageDraw.Draw(img)
# 1) 青い大きな箱 (Root AABB)
draw.rectangle([50, 50, 550, 230], fill=None, outline=BLUE, width=3)
draw.text((60, 55), "◆ Root AABB (モデル全体を包む箱)", fill=BLUE, font=font_medium)
# 2) 左の緑の箱 (Left AABB)
draw.rectangle([70, 80, 280, 210], fill=None, outline=GREEN, width=2)
draw.text((80, 85), "Left AABB (左側)", fill=GREEN, font=font_medium)
# 3) 右の橙の箱 (Right AABB)
draw.rectangle([320, 80, 530, 210], fill=None, outline=ORANGE, width=2)
draw.text((330, 85), "Right AABB (右側)", fill=ORANGE, font=font_medium)
# 4) 左箱の中の三角形ポリゴン
draw.polygon([(100, 180), (140, 110), (180, 170)], fill=GRAY, outline=LIGHT_GRAY)
draw.polygon([(200, 190), (220, 130), (260, 180)], fill=GRAY, outline=LIGHT_GRAY)
# 5) 右箱の中の三角形ポリゴン
draw.polygon([(360, 170), (400, 110), (440, 180)], fill=GRAY, outline=LIGHT_GRAY)
draw.polygon([(460, 190), (480, 120), (510, 175)], fill=GRAY, outline=LIGHT_GRAY)
# 6) レーザー光線
draw.line([10, 140, 160, 140], fill=RED, width=3)
draw_arrow(draw, (160, 140), (210, 140), RED, width=3)
draw.text((15, 115), "レーザー光線", fill=RED, font=font_medium)
# 7) 右箱のスキップ説明
draw.rounded_rectangle([340, 120, 510, 180], radius=4, fill=(180, 50, 50))
draw.text((350, 130), "【右箱は非衝突】\n中身の計算を完全に\nスキップ (サボる) !", fill=WHITE, font=font_medium)
img.save(os.path.join(output_dir, "slide_bvh.png"))

print("スライド用画像を正常に出力しました！")
