import bpy
import bmesh
import os
import math

output_dir = r"C:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources"
os.makedirs(output_dir, exist_ok=True)

# -------------------------------------------------------------
# 1. テクスチャの生成
# -------------------------------------------------------------

def save_image(img, filename):
    path = os.path.join(output_dir, filename)
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    print(f"Saved: {path}")

# (A) target.png (射撃標的: 木製ベース + 赤白黒の同心円ブルズアイ + アヒルのシルエット)
width, height = 256, 256
img_target = bpy.data.images.new("target_tex", width=width, height=height)
pixels_target = [1.0] * (4 * width * height)

for y in range(height):
    for x in range(width):
        idx = (y * width + x) * 4
        # 中心からの距離
        dx = x - 128
        dy = y - 128
        dist = math.sqrt(dx*dx + dy*dy)
        
        # 背景: 木目調 (オリーブ・ウッド)
        wood_grain = 0.5 + 0.1 * math.sin(y * 0.15 + math.sin(x * 0.05) * 2.0)
        r, g, b = 0.55 * wood_grain, 0.40 * wood_grain, 0.25 * wood_grain

        # ターゲット円 (半径 105 以内)
        if dist < 105:
            # 外枠 (黒)
            if dist > 98:
                r, g, b = 0.1, 0.1, 0.1
            # リング1 (白)
            elif dist > 75:
                r, g, b = 0.92, 0.92, 0.92
            # リング2 (赤)
            elif dist > 52:
                r, g, b = 0.85, 0.12, 0.12
            # リング3 (白)
            elif dist > 30:
                r, g, b = 0.92, 0.92, 0.92
            # ブルズアイ中心 (赤)
            else:
                r, g, b = 0.95, 0.08, 0.08

        # 十字レティクルライン
        if (abs(dx) <= 2 or abs(dy) <= 2) and dist < 105 and dist > 15:
            r, g, b = 0.1, 0.1, 0.1

        pixels_target[idx] = r
        pixels_target[idx+1] = g
        pixels_target[idx+2] = b
        pixels_target[idx+3] = 1.0

img_target.pixels = pixels_target
save_image(img_target, "target.png")


# (B) signpost.png (案内看板: 木板・ステンシルミリタリープレート)
img_sign = bpy.data.images.new("signpost_tex", width=256, height=128)
pixels_sign = [1.0] * (4 * 256 * 128)

for y in range(128):
    for x in range(256):
        idx = (y * 256 + x) * 4
        # 木板テクスチャ + ダークミリタリー調
        wood = 0.7 + 0.15 * math.sin(y * 0.2 + math.sin(x * 0.04) * 3.0)
        
        # ボード外枠 (鉄フレーム)
        if x < 10 or x > 245 or y < 8 or y > 119:
            r, g, b = 0.25, 0.28, 0.30  # ダークスチール
            # 四隅のリベット
            if ((x in range(12, 20) or x in range(235, 243)) and (y in range(12, 20) or y in range(107, 115))):
                r, g, b = 0.7, 0.7, 0.75
        else:
            # 内部ウッドプレート (オリーブドラブ調ウッド)
            r = 0.35 * wood
            g = 0.42 * wood
            b = 0.28 * wood

            # 中央のミリタリーコーションストライプ (黄色と黒の注意帯)
            if 15 <= y <= 25 or 102 <= y <= 112:
                stripe = ((x + y) // 16) % 2
                if stripe == 0:
                    r, g, b = 0.95, 0.80, 0.05
                else:
                    r, g, b = 0.12, 0.12, 0.12

        pixels_sign[idx] = r
        pixels_sign[idx+1] = g
        pixels_sign[idx+2] = b
        pixels_sign[idx+3] = 1.0

img_sign.pixels = pixels_sign
save_image(img_sign, "signpost.png")


# (C) extraction_pad.png (脱出ヘリパッド: コンクリート + ハザード帯 + Hマーク)
img_extract = bpy.data.images.new("extract_tex", width=256, height=256)
pixels_extract = [1.0] * (4 * 256 * 256)

for y in range(256):
    for x in range(256):
        idx = (y * 256 + x) * 4
        dx = x - 128
        dy = y - 128
        dist = math.sqrt(dx*dx + dy*dy)

        # コンクリートベース
        noise = 0.45 + 0.05 * math.sin(x * 0.3 + y * 0.4)
        r, g, b = noise, noise, noise * 1.02

        # 外周ハザードストライプ (リング状)
        if 95 <= dist <= 120:
            angle = math.atan2(dy, dx)
            stripe = int((angle + math.pi) / (math.pi / 8)) % 2
            if stripe == 0:
                r, g, b = 0.95, 0.82, 0.05  # 鮮やかな安全黄色
            else:
                r, g, b = 0.12, 0.12, 0.12  # ブラック
        elif dist < 95:
            # 円形ライン
            if 88 <= dist <= 93:
                r, g, b = 0.95, 0.95, 0.95  # 白ライン
            # 中央の「H」マークまたは「ESCAPE」
            # Hマークの描画 (左右2本の縦線 + 中央1本の横線)
            in_h_left = (-45 <= dx <= -25) and (-50 <= dy <= 50)
            in_h_right = (25 <= dx <= 45) and (-50 <= dy <= 50)
            in_h_mid = (-25 <= dx <= 25) and (-12 <= dy <= 12)
            if in_h_left or in_h_right or in_h_mid:
                r, g, b = 0.15, 0.90, 0.40  # エメラルドグリーン (脱出エリア)

        pixels_extract[idx] = r
        pixels_extract[idx+1] = g
        pixels_extract[idx+2] = b
        pixels_extract[idx+3] = 1.0

img_extract.pixels = pixels_extract
save_image(img_extract, "extraction_pad.png")


# (D) duckov_crate.png (ミリタリー補給木箱)
img_crate = bpy.data.images.new("crate_tex", width=256, height=256)
pixels_crate = [1.0] * (4 * 256 * 256)

for y in range(256):
    for x in range(256):
        idx = (y * 256 + x) * 4
        # 木目板ベース
        plank_idx = y // 64
        wood = 0.65 + 0.12 * math.sin(x * 0.15 + (y % 64) * 0.05)
        # 板と板の隙間
        if y % 64 <= 2:
            wood *= 0.4

        # ミリタリーオリーブ・ウッド
        r = 0.45 * wood
        g = 0.50 * wood
        b = 0.32 * wood

        # 外枠スチール補強フレーム & 筋交い (X字補強)
        is_edge = (x < 18 or x > 237 or y < 18 or y > 237)
        is_cross1 = abs(x - y) < 14
        is_cross2 = abs((255 - x) - y) < 14

        if is_edge or is_cross1 or is_cross2:
            # ダークスチール
            r, g, b = 0.22, 0.25, 0.28
            # リベット
            if ((x in range(6, 14) or x in range(241, 249)) and (y in range(6, 14) or y in range(241, 249))):
                r, g, b = 0.75, 0.75, 0.80

        # ステンシル星マーク / マーク (中央)
        dx = x - 128
        dy = y - 128
        if (abs(dx) < 22 and abs(dy) < 6) or (abs(dy) < 22 and abs(dx) < 6):
            if not is_cross1 and not is_cross2:
                r, g, b = 0.90, 0.85, 0.20  # イエローミリタリーステンシル

        pixels_crate[idx] = r
        pixels_crate[idx+1] = g
        pixels_crate[idx+2] = b
        pixels_crate[idx+3] = 1.0

img_crate.pixels = pixels_crate
save_image(img_crate, "duckov_crate.png")


# (E) container_military.png (ミリタリー輸送コンテナ)
img_cont = bpy.data.images.new("container_tex", width=256, height=256)
pixels_cont = [1.0] * (4 * 256 * 256)

for y in range(256):
    for x in range(256):
        idx = (y * 256 + x) * 4
        # コルゲート波板リブ (X方向の周期的な凸凹)
        rib = 0.7 + 0.3 * math.sin(x * (math.pi / 8.0)) ** 2
        
        # ミリタリーブルーグレー / ネイビースチール
        r = 0.22 * rib
        g = 0.35 * rib
        b = 0.48 * rib

        # 上下と左右の太い補強フレーム
        if x < 12 or x > 243 or y < 12 or y > 243:
            r, g, b = 0.16, 0.24, 0.32

        # コーションマーク・ハザード帯 (右下)
        if 180 <= x <= 235 and 25 <= y <= 50:
            stripe = (x + y) // 10 % 2
            if stripe == 0:
                r, g, b = 0.95, 0.80, 0.05
            else:
                r, g, b = 0.1, 0.1, 0.1

        pixels_cont[idx] = r
        pixels_cont[idx+1] = g
        pixels_cont[idx+2] = b
        pixels_cont[idx+3] = 1.0

img_cont.pixels = pixels_cont
save_image(img_cont, "container_military.png")


# (F) duckov_barrel.png (ミリタリードラム缶)
img_barrel = bpy.data.images.new("barrel_tex", width=256, height=256)
pixels_barrel = [1.0] * (4 * 256 * 256)

for y in range(256):
    for x in range(256):
        idx = (y * 256 + x) * 4
        # ドラム缶のリブバンド (横方向の帯)
        rib = 1.0
        if 65 <= y <= 85 or 170 <= y <= 190:
            rib = 1.35
        elif 70 <= y <= 80 or 175 <= y <= 185:
            rib = 1.5

        # オリーブドラブ金属
        r = 0.30 * rib
        g = 0.38 * rib
        b = 0.22 * rib

        # 中央のイエローハザードバンド
        if 115 <= y <= 140:
            stripe = (x // 16) % 2
            if stripe == 0:
                r, g, b = 0.92, 0.78, 0.08
            else:
                r, g, b = 0.15, 0.15, 0.15

        pixels_barrel[idx] = min(r, 1.0)
        pixels_barrel[idx+1] = min(g, 1.0)
        pixels_barrel[idx+2] = min(b, 1.0)
        pixels_barrel[idx+3] = 1.0

img_barrel.pixels = pixels_barrel
save_image(img_barrel, "duckov_barrel.png")


# (G) duckov_sandbag.png (土嚢・麻袋)
img_sandbag = bpy.data.images.new("sandbag_tex", width=256, height=256)
pixels_sandbag = [1.0] * (4 * 256 * 256)

for y in range(256):
    for x in range(256):
        idx = (y * 256 + x) * 4
        # 麻袋・キャンバス織り目テクスチャ
        weave = 0.8 + 0.2 * (math.sin(x * 0.8) * math.cos(y * 0.8))
        r = 0.65 * weave
        g = 0.58 * weave
        b = 0.44 * weave
        
        # 袋の縫い目・シーム
        if y % 64 <= 3 or x % 128 <= 3:
            r *= 0.5
            g *= 0.5
            b *= 0.5

        pixels_sandbag[idx] = r
        pixels_sandbag[idx+1] = g
        pixels_sandbag[idx+2] = b
        pixels_sandbag[idx+3] = 1.0

img_sandbag.pixels = pixels_sandbag
save_image(img_sandbag, "duckov_sandbag.png")


# -------------------------------------------------------------
# 2. 3Dモデルの生成 (OBJ + MTL)
# -------------------------------------------------------------

def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()

def export_active_object(filename_base, texture_filename):
    obj_path = os.path.join(output_dir, f"{filename_base}.obj")
    mtl_path = os.path.join(output_dir, f"{filename_base}.mtl")
    
    # MTL 手動書き出し (DirectXエンジン完全準拠)
    with open(mtl_path, "w") as f:
        f.write(f"# Material for {filename_base}\n")
        f.write(f"newmtl {filename_base}_mat\n")
        f.write("Ka 1.0 1.0 1.0\n")
        f.write("Kd 1.0 1.0 1.0\n")
        f.write("Ks 0.1 0.1 0.1\n")
        f.write("Ns 10.0\n")
        f.write(f"map_Kd {texture_filename}\n")

    # OBJ 書き出し
    bpy.ops.wm.obj_export(
        filepath=obj_path,
        export_selected_objects=True,
        export_materials=True,
        export_uv=True,
        export_normals=True,
        forward_axis='Z',
        up_axis='Y'
    )
    print(f"Exported: {obj_path}")


# (1) shooting_target.obj (木脚スタンド付き 立体射撃標的)
clear_scene()

# 標的プレート (薄い円柱または厚みのある板)
bpy.ops.mesh.primitive_cylinder_add(radius=0.75, depth=0.1, vertices=24, location=(0, 0, 1.45))
plate = bpy.context.active_object
plate.rotation_euler = (math.radians(90), 0, 0) # 直立

# 支柱 (木製ポスト)
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, -0.08, 0.75))
pole = bpy.context.active_object
pole.scale = (0.12, 0.12, 1.5)

# 脚フレーム (三脚・A型フレームスタンド)
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(-0.4, -0.08, 0.25))
leg_l = bpy.context.active_object
leg_l.scale = (0.1, 0.1, 0.6)
leg_l.rotation_euler = (0, 0, math.radians(25))

bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.4, -0.08, 0.25))
leg_r = bpy.context.active_object
leg_r.scale = (0.1, 0.1, 0.6)
leg_r.rotation_euler = (0, 0, math.radians(-25))

# 統合
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.join()
target_obj = bpy.context.active_object
target_obj.name = "ShootingTarget"

# UV 展開
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(island_margin=0.02)
bpy.ops.object.mode_set(mode='OBJECT')

export_active_object("shooting_target", "target.png")


# (2) signpost.obj (2本脚の立体木製案内看板)
clear_scene()

# サインボード本体 (木製プレート)
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 1.4))
board = bpy.context.active_object
board.scale = (1.8, 0.12, 0.9)

# 左支柱
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(-0.75, 0, 0.8))
pole_l = bpy.context.active_object
pole_l.scale = (0.14, 0.16, 1.6)

# 右支柱
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.75, 0, 0.8))
pole_r = bpy.context.active_object
pole_r.scale = (0.14, 0.16, 1.6)

# 屋根・笠木 (雨除けの傾斜板)
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 1.9))
roof = bpy.context.active_object
roof.scale = (2.0, 0.28, 0.08)

# 統合
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.join()
sign_obj = bpy.context.active_object
sign_obj.name = "SignPost"

bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(island_margin=0.02)
bpy.ops.object.mode_set(mode='OBJECT')

export_active_object("signpost", "signpost.png")


# (3) extraction_pad.obj (立体脱出ヘリパッド)
clear_scene()

# コンクリート土台 (八角形 / 円柱状)
bpy.ops.mesh.primitive_cylinder_add(radius=2.5, depth=0.15, vertices=16, location=(0, 0, 0.075))
pad_base = bpy.context.active_object

# 外周ライトビーコン (4隅の誘導灯)
beacons = []
for i in range(4):
    angle = i * (math.pi / 2) + math.pi / 4
    bx = math.cos(angle) * 2.2
    bz = math.sin(angle) * 2.2
    bpy.ops.mesh.primitive_cylinder_add(radius=0.15, depth=0.35, vertices=8, location=(bx, bz, 0.2))
    beacons.append(bpy.context.active_object)

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.join()
pad_obj = bpy.context.active_object
pad_obj.name = "ExtractionPad"

bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(island_margin=0.02)
bpy.ops.object.mode_set(mode='OBJECT')

export_active_object("extraction_pad", "extraction_pad.png")


# -------------------------------------------------------------
# 3. 既存の障害物 MTL ファイルのテクスチャ紐付け更新
# -------------------------------------------------------------

def update_mtl(mtl_name, new_tex_name):
    mtl_path = os.path.join(output_dir, mtl_name)
    with open(mtl_path, "w") as f:
        f.write(f"# Material for {mtl_name}\n")
        f.write(f"newmtl Material_{mtl_name}\n")
        f.write("Ka 1.0 1.0 1.0\n")
        f.write("Kd 1.0 1.0 1.0\n")
        f.write("Ks 0.15 0.15 0.15\n")
        f.write("Ns 15.0\n")
        f.write(f"map_Kd {new_tex_name}\n")
    print(f"Updated MTL: {mtl_path} -> {new_tex_name}")

update_mtl("container.mtl", "container_military.png")
update_mtl("duckov_crate.mtl", "duckov_crate.png")
update_mtl("duckov_barrel_stack.mtl", "duckov_barrel.png")
update_mtl("duckov_sandbag_wall.mtl", "duckov_sandbag.png")
update_mtl("watchtower.mtl", "duckov_crate.png")
update_mtl("bridge.mtl", "duckov_crate.png")

print("All duckov assets generated successfully!")
