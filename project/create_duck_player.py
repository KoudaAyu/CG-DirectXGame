import bpy
import bmesh
import math
import os
from mathutils import Vector, Matrix

# 1. シーンの初期化
bpy.ops.wm.read_factory_settings(use_empty=True)

# 2. テクスチャ画像の生成 (低ポリ・カートゥーン調のカラーパレット PNG)
# 4x4 パレット
# (0,0): Yellow (Body)      #FFD21E -> (1.0, 0.82, 0.12)
# (1,0): Orange (Beak/Feet) #FF7A00 -> (1.0, 0.48, 0.0)
# (2,0): Black (Eyes)       #1A1A1A -> (0.1, 0.1, 0.1)
# (3,0): White (Eye highlight/Belly) #FFFFFF -> (1.0, 1.0, 1.0)
# (0,1): Dark Yellow        #E5B50A -> (0.9, 0.71, 0.04)
# (1,1): Dark Orange        #D45500 -> (0.83, 0.33, 0.0)
# (2,1): Cheek Pink         #FF8099 -> (1.0, 0.5, 0.6)
# (3,1): Blue (Cap / Acc)   #2B88D9 -> (0.17, 0.53, 0.85)

width, height = 64, 64
image = bpy.data.images.new("duck_texture", width=width, height=height)
pixels = [1.0] * (4 * width * height)

def get_color(x, y):
    # x: 0..3, y: 0..3
    palette = [
        # row 0 (y=0)
        [(1.0, 0.84, 0.14, 1.0), (1.0, 0.50, 0.05, 1.0), (0.12, 0.12, 0.12, 1.0), (0.98, 0.98, 0.98, 1.0)],
        # row 1 (y=1)
        [(0.92, 0.72, 0.08, 1.0), (0.85, 0.38, 0.02, 1.0), (1.0, 0.55, 0.65, 1.0), (0.18, 0.58, 0.92, 1.0)],
        # row 2 (y=2)
        [(1.0, 0.92, 0.50, 1.0), (1.0, 0.68, 0.20, 1.0), (0.25, 0.25, 0.28, 1.0), (0.90, 0.95, 1.0, 1.0)],
        # row 3 (y=3)
        [(0.80, 0.60, 0.05, 1.0), (0.70, 0.28, 0.00, 1.0), (0.95, 0.35, 0.45, 1.0), (0.12, 0.40, 0.75, 1.0)],
    ]
    px = min(max(int(x), 0), 3)
    py = min(max(int(y), 0), 3)
    return palette[py][px]

for y in range(height):
    for x in range(width):
        cell_x = int(x / (width / 4))
        cell_y = int(y / (height / 4))
        col = get_color(cell_x, cell_y)
        idx = (y * width + x) * 4
        pixels[idx] = col[0]
        pixels[idx+1] = col[1]
        pixels[idx+2] = col[2]
        pixels[idx+3] = col[3]

image.pixels = pixels

# テクスチャ画像の保存先
output_dir = r"C:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources"
os.makedirs(output_dir, exist_ok=True)
tex_path = os.path.join(output_dir, "duck.png")
image.filepath_raw = tex_path
image.file_format = 'PNG'
image.save()
print(f"Texture saved to {tex_path}")

# UV座標ヘルパー (4x4グリッドの中央を指定)
def uv_for_cell(col_x, col_y):
    # col_x: 0..3, col_y: 0..3 (0が下)
    u = (col_x + 0.5) / 4.0
    v = (col_y + 0.5) / 4.0
    return Vector((u, v))

UV_YELLOW = uv_for_cell(0, 0)
UV_ORANGE = uv_for_cell(1, 0)
UV_BLACK = uv_for_cell(2, 0)
UV_WHITE = uv_for_cell(3, 0)
UV_DARK_YELLOW = uv_for_cell(0, 1)
UV_CHEEK = uv_for_cell(2, 1)
UV_BLUE = uv_for_cell(3, 1)
UV_LIGHT_YELLOW = uv_for_cell(0, 2)

# マテリアルの作成
mat = bpy.data.materials.new(name="DuckMaterial")
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get("Principled BSDF")
tex_node = mat.node_tree.nodes.new('ShaderNodeTexImage')
tex_node.image = image
mat.node_tree.links.new(bsdf.inputs['Base Color'], tex_node.outputs['Color'])

created_objects = []

def assign_uv_and_material(obj, target_uv):
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)
    
    # 全面のUVを特定のセル中央にセット
    mesh = obj.data
    if not mesh.uv_layers:
        mesh.uv_layers.new(name="UVMap")
    uv_layer = mesh.uv_layers.active.data
    for poly in mesh.polygons:
        for loop_idx in poly.loop_indices:
            uv_layer[loop_idx].uv = target_uv

# --- 1. アヒルの胴体 (Body) ---
# ふっくらした丸い体。少し楕円形、後ろにお尻・尻尾がツンと上がっている
bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=14, radius=0.38, location=(0, 0, 0.40))
body = bpy.context.active_object
body.name = "Body"
# スケール調整: X少し横幅、Y前後、Z高さ
body.scale = (1.05, 1.25, 0.95)
bpy.ops.object.transform_apply(scale=True)

# 尻尾を作るために頂点を少し後上方 (+Y/+Z) に変形
# ゲーム内では +Z が前方なので、Blenderの座標系 (+Y前、+Z上) -> エクスポート時に適切にマッピング
# ここでは「Blender基準で +Y が後、-Y が前」または「+Yが前、-Yが後」を意識
# エクスポート時に Forward: Z, Up: Y で出す場合:
# Blender内: +Zが上、+Yが前方、-Yが後方とする
bm = bmesh.new()
bm.from_mesh(body.data)
for v in bm.verts:
    # -Y方向（後方）の頂点を少し持ち上げて尻尾の形状にする
    if v.co.y < -0.15:
        factor = (-v.co.y - 0.15) / 0.35
        v.co.z += 0.22 * (factor ** 1.5)
        v.co.y -= 0.08 * factor
        v.co.x *= (1.0 - 0.3 * factor) # 少し窄ませる
    # お腹の下側を少し平らにして安定感を出す
    if v.co.z < 0.25:
        v.co.z = max(v.co.z, 0.15)
bm.to_mesh(body.data)
bm.free()

assign_uv_and_material(body, UV_YELLOW)
created_objects.append(body)

# --- 2. アヒルの頭部 (Head) ---
# 少し前上がり (+Y前、+Z上) に配置
bpy.ops.mesh.primitive_uv_sphere_add(segments=18, ring_count=12, radius=0.28, location=(0, 0.22, 0.72))
head = bpy.context.active_object
head.name = "Head"
head.scale = (1.02, 1.08, 1.0)
bpy.ops.object.transform_apply(scale=True)
assign_uv_and_material(head, UV_YELLOW)
created_objects.append(head)

# --- 3. くちばし (Beak) ---
# 前方 (+Y) に突き出たくちばし
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0.48, 0.68))
beak = bpy.context.active_object
beak.name = "Beak"
beak.scale = (0.24, 0.22, 0.09)
bpy.ops.object.transform_apply(scale=True)

# くちばしに丸みと先端の窄まりを適用
bm = bmesh.new()
bm.from_mesh(beak.data)
for v in bm.verts:
    if v.co.y > 0.48: # 先端
        v.co.x *= 0.75
        v.co.z -= 0.02
bm.to_mesh(beak.data)
bm.free()

assign_uv_and_material(beak, UV_ORANGE)
created_objects.append(beak)

# --- 4. 目 (Eyes) & ハイライト ---
for side, sign in [("L", 1), ("R", -1)]:
    # 瞳（黒）
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=0.06, location=(sign * 0.21, 0.36, 0.78))
    eye = bpy.context.active_object
    eye.name = f"Eye_{side}"
    eye.scale = (0.6, 0.9, 1.0)
    eye.rotation_euler = (0, 0, math.radians(sign * 15))
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    assign_uv_and_material(eye, UV_BLACK)
    created_objects.append(eye)
    
    # ハイライト（白）
    bpy.ops.mesh.primitive_uv_sphere_add(segments=8, ring_count=6, radius=0.022, location=(sign * 0.23, 0.39, 0.81))
    hi = bpy.context.active_object
    hi.name = f"EyeHi_{side}"
    assign_uv_and_material(hi, UV_WHITE)
    created_objects.append(hi)
    
    # ほっぺ（ピンクのチーク）
    bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6, radius=0.045, location=(sign * 0.23, 0.30, 0.69))
    cheek = bpy.context.active_object
    cheek.name = f"Cheek_{side}"
    cheek.scale = (0.4, 0.8, 0.6)
    cheek.rotation_euler = (0, 0, math.radians(sign * 10))
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    assign_uv_and_material(cheek, UV_CHEEK)
    created_objects.append(cheek)

# --- 5. 翼 (Wings) ---
for side, sign in [("L", 1), ("R", -1)]:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=14, ring_count=10, radius=0.20, location=(sign * 0.36, -0.02, 0.42))
    wing = bpy.context.active_object
    wing.name = f"Wing_{side}"
    wing.scale = (0.35, 1.3, 0.7)
    wing.rotation_euler = (math.radians(-12), math.radians(sign * 15), math.radians(sign * 8))
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    assign_uv_and_material(wing, UV_DARK_YELLOW)
    created_objects.append(wing)

# --- 6. 足・水かき (Feet) ---
for side, sign in [("L", 1), ("R", -1)]:
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(sign * 0.16, 0.08, 0.03))
    foot = bpy.context.active_object
    foot.name = f"Foot_{side}"
    foot.scale = (0.16, 0.26, 0.05)
    foot.rotation_euler = (0, 0, math.radians(sign * -12))
    bpy.ops.object.transform_apply(scale=True, rotation=True)
    
    bm = bmesh.new()
    bm.from_mesh(foot.data)
    for v in bm.verts:
        # 前方に広がる水かき形状
        if v.co.y > 0.08:
            v.co.x *= 1.35
            v.co.z *= 0.5
    bm.to_mesh(foot.data)
    bm.free()
    
    assign_uv_and_material(foot, UV_ORANGE)
    created_objects.append(foot)

# --- 7. かわいい頭のちょんまげ/羽毛 (Cute Head Feather) ---
bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6, radius=0.06, location=(0, 0.16, 0.98))
tuft = bpy.context.active_object
tuft.name = "HeadTuft"
tuft.scale = (0.6, 0.8, 1.6)
tuft.rotation_euler = (math.radians(-25), 0, 0)
bpy.ops.object.transform_apply(scale=True, rotation=True)
assign_uv_and_material(tuft, UV_LIGHT_YELLOW)
created_objects.append(tuft)

# --- 8. 全パーツを1つのメッシュに統合 ---
bpy.ops.object.select_all(action='DESELECT')
for obj in created_objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()

duck_model = bpy.context.active_object
duck_model.name = "PlayerDuck"

# プレイヤーのゲーム内スケール（高さ約1.65m、横幅約1.25m）に拡大調整
SCALE_FACTOR = 1.65
duck_model.scale = (SCALE_FACTOR, SCALE_FACTOR, SCALE_FACTOR)
bpy.ops.object.transform_apply(scale=True)

# 足元（最下部）を正確に Z=0 (Blenderの上方向) に接地
min_z = min(v.co.z for v in duck_model.data.vertices)
for v in duck_model.data.vertices:
    v.co.z -= min_z

# スムーズシェーディング
for poly in duck_model.data.polygons:
    poly.use_smooth = True

# 全ポリゴンを三角形化 (Triangulate) - エンジンのOBJローダー必須条件
bm = bmesh.new()
bm.from_mesh(duck_model.data)
bmesh.ops.triangulate(bm, faces=bm.faces[:])
bm.to_mesh(duck_model.data)
bm.free()

# --- 9. OBJ エクスポート ---
# エンジン仕様:
# Y-up, +Z Forward (ゲーム内で前方が+Z)
# Blender座標 (+Y前, +Z上) -> エクスポートで Forward: +Z, Up: +Y
obj_out_path = os.path.join(output_dir, "player.obj")

# Blender 5.x / 4.x の obj export
try:
    bpy.ops.wm.obj_export(
        filepath=obj_out_path,
        export_selected_objects=True,
        forward_axis='Z',
        up_axis='Y',
        apply_modifiers=True,
        export_uv=True,
        export_normals=True,
        export_materials=True,
        export_triangulated_mesh=True
    )
except Exception as e:
    # 従来の bpy.ops.export_scene.obj へのフォールバック
    print(f"wm.obj_export failed ({e}), falling back to export_scene.obj")
    bpy.ops.export_scene.obj(
        filepath=obj_out_path,
        use_selection=True,
        axis_forward='Z',
        axis_up='Y',
        use_mesh_modifiers=True,
        use_uvs=True,
        use_normals=True,
        use_materials=True,
        use_triangles=True
    )

print(f"Successfully generated Player Duck model: {obj_out_path}")
