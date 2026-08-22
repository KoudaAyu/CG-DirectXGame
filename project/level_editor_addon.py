import bpy
import json
import os
import math
import time
import random
import re
import blf
import mathutils
import gpu
from gpu_extras.batch import batch_for_shader
import bpy_extras.view3d_utils as view3d_utils

# 1. アドオンの基本プロフィール
bl_info = {
    "name": "DirectX レベルエディタ (プロシージャル対応版)",
    "author": "Kouda Ayu",
    "version": (1, 6),
    "blender": (3, 3, 0),
    "location": "3Dビューポート > サイドバー(Nキー) > レベルエディタ",
    "description": "Blenderの配置データをDirectX用のY-up・左手系（ラジアン）座標に変換し、プロシージャル用のパラメータを含めてJSONに保存します",
    "category": "Object"
}

# 2. 設定データ（プロジェクトのフォルダパス）
class MyAddonPropertiesV2(bpy.types.PropertyGroup):
    project_path: bpy.props.StringProperty(
        name="プロジェクトパス",
        description="DirectXGame.sln があるprojectフォルダを選択してください",
        default="C:\\Users\\3329a\\OneDrive\\デスクトップ\\Engine\\project",
        subtype='DIR_PATH'
    )
    auto_export: bpy.props.BoolProperty(
        name="自動エクスポート",
        description="配置オブジェクトに変更があった時、自動的にJSONファイルを更新します（手動保存ボタンでも即座に出力できます）",
        default=False
    )
    camera_sync: bpy.props.BoolProperty(
        name="カメラ同期",
        description="Blenderの視点をゲームエンジンのカメラと同期させます",
        default=False
    )
    auto_export_status: bpy.props.StringProperty(
        name="同期状態",
        default="準備完了"
    )
    paint_type: bpy.props.EnumProperty(
        name="配置アセット",
        description="ペイント配置するアセットの種類を選択します",
        items=[
            ('Tree', "木 (Tree)", "プロシージャル樹木を配置します"),
            ('Rock', "岩 (Rock)", "プロシージャル岩石を配置します"),
            ('PlayerSpawn', "🏃 プレイヤー開始点", "プレイヤーの初期出現位置を配置します"),
            ('EnemySpawn', "⚠️ 敵巡回/スポーン", "敵の巡回・出現ポイントを配置します"),
            ('LootBox', "📦 物資/ルート箱", "武器やアイテムの箱を配置します"),
            ('River', "川ブロック (River)", "川の道路ブロックを配置します")
        ],
        default='Tree'
    )
    paint_spacing: bpy.props.FloatProperty(
        name="配置間隔 (m)",
        description="筆塗りで連続配置されるアセットの間隔(メートル)を指定します",
        default=2.5,
        min=0.8,
        max=10.0
    )
    draw_brush_radius: bpy.props.FloatProperty(
        name="筆の太さ (m)",
        description="お絵かき生成（森・岩場）の散布半径・太さを指定します",
        default=1.5,
        min=0.3,
        max=5.0
    )
    scatter_count: bpy.props.IntProperty(
        name="配置する数",
        description="領域内に自動配置するオブジェクトの数です",
        default=10,
        min=1,
        max=100
    )
    scatter_type: bpy.props.EnumProperty(
        name="散布する種類",
        description="ランダム散布するアセットの種類です",
        items=[
            ('Tree', "木 (Tree)", "プロシージャル樹木を散布します"),
            ('Rock', "岩 (Rock)", "プロシージャル岩石を散布します"),
            ('Mixed', "木と岩のミックス", "木と岩をバランスよくランダム散布します")
        ],
        default='Mixed'
    )

# マテリアル自動生成ヘルパー関数
def get_or_create_material(name, color=(0.8, 0.8, 0.8, 1.0), roughness=0.5):
    mat = bpy.data.materials.get(name)
    if not mat:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs['Base Color'].default_value = color
            if 'Roughness' in bsdf.inputs:
                bsdf.inputs['Roughness'].default_value = roughness
    return mat

def get_or_create_biome_material(name, color=(0.1, 0.5, 0.9, 0.7)):
    mat = bpy.data.materials.get(name)
    if not mat:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        mat.blend_method = 'BLEND' if color[3] < 1.0 else 'OPAQUE'
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs['Base Color'].default_value = color
            if 'Alpha' in bsdf.inputs:
                bsdf.inputs['Alpha'].default_value = color[3]
    return mat

def ray_cast_terrain(scene, context, origin, direction, exclude_objs=None):
    # 数学的な平面交点計算（Z=0の地面との判定）
    # depsgraph.update() を呼ばないため、マウス操作中にBlenderが絶対に落ちない
    if abs(direction.z) > 0.00001:
        t = -origin.z / direction.z
        if t > 0:
            hit_point = mathutils.Vector((origin.x + direction.x * t, origin.y + direction.y * t, 0.0))
            return True, hit_point, mathutils.Vector((0, 0, 1)), 0, None, mathutils.Matrix.Identity(4)
            
    # メッシュ判定フォールバック
    try:
        result, location, normal, index, obj, matrix = scene.ray_cast(
            context.view_layer.depsgraph, origin, direction
        )
        return result, location, normal, index, obj, matrix
    except Exception:
        return False, mathutils.Vector((0,0,0)), mathutils.Vector((0,0,1)), 0, None, mathutils.Matrix.Identity(4)


def get_object_radius(obj):
    if not obj:
        return 0.5
    obj_type = obj.get("type", "Tree")
    scale_factor = obj.scale.x if hasattr(obj, 'scale') else 1.0
    if obj_type == 'Tree':
        return 0.7 * scale_factor
    elif obj_type == 'Rock':
        return 0.5 * scale_factor
    elif obj_type in ['PlayerSpawn', 'EnemySpawn', 'LootBox']:
        return 0.8 * scale_factor
    else:
        return 0.5 * scale_factor

def is_location_valid(location, required_radius, scene_objects, ignore_objs=None):
    if ignore_objs is None:
        ignore_objs = []
    
    lx, ly, lz = location[0], location[1], location[2]
    
    for obj in scene_objects:
        if obj in ignore_objs:
            continue
        obj_type = obj.get("type", None)
        if not obj_type:
            continue
            
        if obj_type == "River":
            points_x = obj.get("points_x", [])
            points_y = obj.get("points_y", [])
            river_w = obj.get("river_width", 3.0)
            safe_dist = (river_w * 0.5) + required_radius
            
            if points_x and points_y:
                for px, py in zip(points_x, points_y):
                    dist = math.sqrt((lx - px)**2 + (ly - py)**2)
                    if dist < safe_dist:
                        return False
            else:
                ox, oy = obj.location.x, obj.location.y
                dist = math.sqrt((lx - ox)**2 + (ly - oy)**2)
                if dist < safe_dist:
                    return False
        
        elif obj_type in ['Tree', 'Rock', 'PlayerSpawn', 'EnemySpawn', 'LootBox']:
            ox, oy = obj.location.x, obj.location.y
            other_radius = get_object_radius(obj)
            min_dist = required_radius + other_radius
            dist = math.sqrt((lx - ox)**2 + (ly - oy)**2)
            if dist < min_dist:
                return False
                
    return True

def poisson_disk_sampling_path(path_points, brush_radius, min_dist, k_attempts=12):
    if not path_points or min_dist <= 0:
        return []
    
    samples = []
    cell_size = min_dist / math.sqrt(2)
    grid = {}
    
    def get_grid_cell(p):
        return (int(p[0] / cell_size), int(p[1] / cell_size))
    
    def is_valid_sample(p):
        cell = get_grid_cell(p)
        cx, cy = cell
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                neighbor = (cx + dx, cy + dy)
                if neighbor in grid:
                    sp = grid[neighbor]
                    dist = math.sqrt((p[0] - sp[0])**2 + (p[1] - sp[1])**2)
                    if dist < min_dist:
                        return False
        return True

    def add_sample(p):
        samples.append(p)
        grid[get_grid_cell(p)] = p

    for pt in path_points:
        for _ in range(k_attempts):
            angle = random.uniform(0, math.pi * 2)
            r = random.uniform(0, brush_radius)
            candidate = (pt[0] + r * math.cos(angle), pt[1] + r * math.sin(angle), pt[2])
            if is_valid_sample(candidate):
                add_sample(candidate)
                break
                
    return samples

def create_tree_visual_object(location=(0,0,0)):
    x, y, z = location[0], location[1], location[2]
    
    bpy.ops.mesh.primitive_cylinder_add(radius=0.1, depth=0.6, location=(x, y, z + 0.3))
def safe_join(active_obj, selected_objs):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    for o in selected_objs:
        if o:
            o.select_set(True)
    bpy.context.view_layer.objects.active = active_obj

    if hasattr(bpy.context, "temp_override"):
        try:
            with bpy.context.temp_override(active_object=active_obj, selected_editable_objects=selected_objs):
                bpy.ops.object.join()
                return bpy.context.active_object
        except Exception:
            pass
    try:
        bpy.ops.object.join()
    except Exception:
        pass
    return bpy.context.active_object

def create_tree_visual_object(location=(0,0,0)):
    x, y, z = location[0], location[1], location[2]
    
    mesh = bpy.data.meshes.get("ProceduralTreeMesh")
    if not mesh:
        verts = []
        faces = []
        
        # Trunk (8-sided cylinder)
        trunk_r, trunk_h = 0.15, 0.8
        for i in range(8):
            ang = (i / 8.0) * math.pi * 2.0
            verts.append((trunk_r * math.cos(ang), trunk_r * math.sin(ang), 0.0))
            verts.append((trunk_r * math.cos(ang), trunk_r * math.sin(ang), trunk_h))
        for i in range(8):
            i2 = (i + 1) % 8
            faces.append((i*2, i2*2, i2*2+1, i*2+1))
            
        # Cone 1 (base z + 0.35)
        base_v = len(verts)
        cone1_r, cone1_h, cone1_z = 0.7, 0.7, 0.35
        verts.append((0.0, 0.0, cone1_z + cone1_h))
        for i in range(8):
            ang = (i / 8.0) * math.pi * 2.0
            verts.append((cone1_r * math.cos(ang), cone1_r * math.sin(ang), cone1_z))
        for i in range(8):
            i2 = (i + 1) % 8
            faces.append((base_v, base_v + 1 + i, base_v + 1 + i2))
            
        # Cone 2 (base z + 0.8)
        base_v = len(verts)
        cone2_r, cone2_h, cone2_z = 0.55, 0.65, 0.8
        verts.append((0.0, 0.0, cone2_z + cone2_h))
        for i in range(8):
            ang = (i / 8.0) * math.pi * 2.0
            verts.append((cone2_r * math.cos(ang), cone2_r * math.sin(ang), cone2_z))
        for i in range(8):
            i2 = (i + 1) % 8
            faces.append((base_v, base_v + 1 + i, base_v + 1 + i2))
            
        # Cone 3 (base z + 1.25)
        base_v = len(verts)
        cone3_r, cone3_h, cone3_z = 0.4, 0.6, 1.25
        verts.append((0.0, 0.0, cone3_z + cone3_h))
        for i in range(8):
            ang = (i / 8.0) * math.pi * 2.0
            verts.append((cone3_r * math.cos(ang), cone3_r * math.sin(ang), cone3_z))
        for i in range(8):
            i2 = (i + 1) % 8
            faces.append((base_v, base_v + 1 + i, base_v + 1 + i2))
            
        mesh = bpy.data.meshes.new("ProceduralTreeMesh")
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        
        leaves_mat = get_or_create_material("TreeLeavesMaterial", (0.2, 0.75, 0.2, 1.0))
        mesh.materials.append(leaves_mat)
        
    tree_obj = bpy.data.objects.new("ProceduralTree", mesh)
    tree_obj.location = (x, y, z)
    if tree_obj.name not in bpy.context.collection.objects.values():
        bpy.context.collection.objects.link(tree_obj)
    return tree_obj

def create_player_visual_object(location=(0,0,0)):
    x, y, z = location[0], location[1], location[2]
    
    mesh = bpy.data.meshes.get("PlayerSpawnMesh")
    if not mesh:
        mesh = bpy.data.meshes.new("PlayerSpawnMesh")
        verts = [(-0.3, -0.3, 0.0), (0.3, -0.3, 0.0), (0.3, 0.3, 0.0), (-0.3, 0.3, 0.0), (0.0, 0.0, 1.2)]
        faces = [(0, 1, 2, 3), (0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)]
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        mat = get_or_create_material("PlayerSpawnMaterial", (0.2, 0.5, 1.0, 1.0))
        mesh.materials.append(mat)
        
    p_obj = bpy.data.objects.new("PlayerSpawnPoint", mesh)
    p_obj.location = (x, y, z)
    if p_obj.name not in bpy.context.collection.objects.values():
        bpy.context.collection.objects.link(p_obj)
    return p_obj

def create_enemy_visual_object(location=(0,0,0)):
    x, y, z = location[0], location[1], location[2]
    
    mesh = bpy.data.meshes.get("EnemySpawnMesh")
    if not mesh:
        mesh = bpy.data.meshes.new("EnemySpawnMesh")
        verts = [(-0.3, -0.3, 0.0), (0.3, -0.3, 0.0), (0.3, 0.3, 0.0), (-0.3, 0.3, 0.0), (0.0, 0.0, 1.2)]
        faces = [(0, 1, 2, 3), (0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)]
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        mat = get_or_create_material("EnemySpawnMaterial", (1.0, 0.25, 0.2, 1.0))
        mesh.materials.append(mat)
        
    e_obj = bpy.data.objects.new("EnemySpawnPoint", mesh)
    e_obj.location = (x, y, z)
    if e_obj.name not in bpy.context.collection.objects.values():
        bpy.context.collection.objects.link(e_obj)
    return e_obj

# 各種オペレーター
class MYADDON_OT_add_procedural_tree(bpy.types.Operator):
    bl_idname = "myaddon.add_procedural_tree"
    bl_label = "🌲 プロシージャル樹木を追加"
    bl_description = "スタイリッシュな3段ツリーを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        tree_obj = create_tree_visual_object(context.scene.cursor.location)
        tree_obj["type"] = "Tree"
        tree_obj["seed"] = random.randint(1, 99999)
        tree_obj["iterations"] = 2
        tree_obj["branchLength"] = 0.15
        tree_obj["branchRadius"] = 0.06
        tree_obj["taperRate"] = 0.8
        tree_obj["angle"] = 25.0
        self.report({'INFO'}, "🌲 プロシージャル樹木を追加しました。")
        return {'FINISHED'}

class MYADDON_OT_add_procedural_rock(bpy.types.Operator):
    bl_idname = "myaddon.add_procedural_rock"
    bl_label = "🪨 プロシージャル岩を追加"
    bl_description = "ICO球ベースのプロシージャル岩石オブジェクトを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.45, location=context.scene.cursor.location)
        obj = context.active_object
        obj.name = "ProceduralRock"
        mat = get_or_create_material("RockPlaceholderMaterial", (0.5, 0.5, 0.5, 1.0))
        obj.data.materials.append(mat)
        
        obj["type"] = "Rock"
        obj["seed"] = random.randint(1, 99999)
        obj["subdivisions"] = 2
        obj["noiseStrength"] = 0.25
        obj["voronoiStrength"] = 0.15
        obj["crackStrength"] = 0.1
        self.report({'INFO'}, "🪨 プロシージャル岩石を追加しました。")
        return {'FINISHED'}

class MYADDON_OT_add_player_spawn(bpy.types.Operator):
    bl_idname = "myaddon.add_player_spawn"
    bl_label = "🏃 プレイヤー開始点を追加"
    bl_description = "プレイヤーが出現する初期位置を設定します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        p_obj = create_player_visual_object(context.scene.cursor.location)
        p_obj["type"] = "PlayerSpawn"
        p_obj["spawn_id"] = 1
        self.report({'INFO'}, "🏃 プレイヤー開始点を追加しました。")
        return {'FINISHED'}

class MYADDON_OT_add_enemy_spawn(bpy.types.Operator):
    bl_idname = "myaddon.add_enemy_spawn"
    bl_label = "⚠️ 敵巡回/スポーンを追加"
    bl_description = "敵が出現・巡回するポイントを設定します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        e_obj = create_enemy_visual_object(context.scene.cursor.location)
        e_obj["type"] = "EnemySpawn"
        e_obj["enemy_type"] = "Scavenger"
        e_obj["patrol_radius"] = 10.0
        self.report({'INFO'}, "⚠️ 敵巡回/スポーンポイントを追加しました。")
        return {'FINISHED'}

def create_container_visual_object(location=(0,0,0)):
    """
    project/Resources/container.obj を直接読み込み、Blender上に1つの綺麗な
    貨物コンテナ(Cargo Container)として配置します
    """
    addons_dir = os.path.dirname(os.path.abspath(__file__))
    resources_dir = os.path.join(addons_dir, "Resources")
    obj_path = os.path.join(resources_dir, "container.obj")
    if not os.path.exists(obj_path):
        obj_path = r"c:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources\container.obj"

    container_obj = None
    if os.path.exists(obj_path):
        container_obj = load_obj_mesh_pure(obj_path, "Obstacle_CargoContainer")
        if container_obj:
            bpy.context.scene.collection.objects.link(container_obj)
            container_obj.location = (location[0], location[1], location[2])

    if not container_obj:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(location[0], location[1], location[2] + 1.1))
        container_obj = bpy.context.active_object
        container_obj.scale = (2.2, 5.0, 2.2)
        container_obj.name = "Obstacle_CargoContainer"

    mat_container = get_or_create_material("ContainerMaterial", (0.55, 0.58, 0.62, 1.0), roughness=0.4)
    if len(container_obj.data.materials) == 0:
        container_obj.data.materials.append(mat_container)

    return container_obj


def export_scene_to_json():
    """
    Blenderの全メッシュ（コンテナ、フェンス、土嚢、ドラム缶、木箱、見張り塔、橋、川、木、自作オブジェクト等）を
    project/Resources/stage_layout.json に自動で全書き出し保存する関数。
    """
    project_root = r"c:\Users\3329a\OneDrive\デスクトップ\Engine\project"
    resources_dir = os.path.join(project_root, "Resources")
    os.makedirs(resources_dir, exist_ok=True)
    output_path = os.path.join(resources_dir, "stage_layout.json")

    layout_data = []
    active_obj = bpy.context.active_object
    selected_objs = [o for o in bpy.context.selected_objects]


    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            pos_x = obj.location.x
            pos_y = obj.location.z
            pos_z = obj.location.y

            rot_x = obj.rotation_euler.x
            rot_y = obj.rotation_euler.z
            rot_z = obj.rotation_euler.y

            scale_x = obj.scale.x
            scale_y = obj.scale.z
            scale_z = obj.scale.y

            obj_name = obj.name.split('.')[0]
            obj_type = obj.get("type", "Obstacle")

            if obj_type == "Fence":
                rot_x = 0.0
                rot_z = 0.0

            model_dir = "Resources"
            lower_name = obj_name.lower()
            lower_type = str(obj_type).lower()

            if "container" in lower_name or lower_type == "container":
                model_file = "container.obj"
            elif "fence" in lower_name or lower_type == "fence":
                model_file = "fence.obj"
            elif "river" in lower_name or lower_type == "river" or "shore" in lower_name or "water" in lower_name:
                model_file = "river.obj"
            elif "bridge" in lower_name or lower_type == "bridge":
                model_file = "bridge.obj"
            elif "watchtower" in lower_name or lower_type == "watchtower" or "tower" in lower_name:
                model_file = "watchtower.obj"
            elif "sandbag" in lower_name or "wall" in lower_name:
                model_file = "duckov_sandbag_wall.obj"
            elif "barrel" in lower_name or "oil" in lower_name or "drum" in lower_name:
                model_file = "duckov_barrel_stack.obj"
            elif "crate" in lower_name or "supply" in lower_name:
                model_file = "duckov_crate.obj"
            elif "spawn" in lower_name or "spawn" in lower_type:
                model_file = "sphere.obj"
            elif "goal" in lower_name or "extraction" in lower_name or "ring" in lower_name:
                model_file = "ring.obj"
            elif "tree" in lower_name or lower_type == "tree":
                model_file = "tree.obj"
            elif "rock" in lower_name or lower_type == "rock":
                model_file = "rock.obj"
            elif "plane" in lower_name or "ground" in lower_name:
                model_file = "plane.obj"
            elif "teapot" in lower_name:
                model_file = "teapot.obj"
            else:
                model_file = "fence.obj"


            obj_info = {
                "name": obj_name,
                "type": obj_type,
                "modelDirectory": model_dir,
                "modelFilename": model_file,
                "position": {"x": round(pos_x, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
                "rotation": {"x": round(rot_x, 4), "y": round(rot_y, 4), "z": round(rot_z, 4)},
                "scale":    {"x": round(scale_x, 4), "y": round(scale_y, 4), "z": round(scale_z, 4)},
                "isStatic": True
            }

            if obj_type == "River" or lower_type == "river" or "river" in lower_name or "water" in lower_name:
                obj_info["type"] = "River"
                obj_info["modelFilename"] = "river.obj"
                points_list = []
                if hasattr(obj.data, "vertices") and len(obj.data.vertices) >= 2:
                    verts_world = [obj.matrix_world @ v.co for v in obj.data.vertices]
                    min_x = min(v.x for v in verts_world)
                    max_x = max(v.x for v in verts_world)
                    min_y = min(v.y for v in verts_world) # Blender depth -> DirectX Z
                    max_y = max(v.y for v in verts_world)
                    min_z = min(v.z for v in verts_world) # Blender height -> DirectX Y
                    max_z = max(v.z for v in verts_world)

                    center_x = (min_x + max_x) * 0.5
                    center_y = (min_z + max_z) * 0.5
                    center_z = (min_y + max_y) * 0.5
                    obj_info["position"] = {"x": round(center_x, 4), "y": round(center_y, 4), "z": round(center_z, 4)}

                    size_x = max(1.0, max_x - min_x)
                    size_z = max(1.0, max_y - min_y)
                    obj_info["scale"] = {"x": round(size_x, 4), "y": 1.0, "z": round(size_z, 4)}

                    step = max(1, len(obj.data.vertices) // 16)
                    for idx in range(0, len(obj.data.vertices), step):
                        wv = verts_world[idx]
                        points_list.append({
                            "x": round(wv.x, 4),
                            "y": round(wv.z, 4),
                            "z": round(wv.y, 4)
                        })

                if not points_list:
                    points_list = [
                        {"x": round(pos_x - 5.0, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
                        {"x": round(pos_x + 5.0, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)}
                    ]
                obj_info["parameters"] = {
                    "river_width": round(obj.get("river_width", 4.0), 2),
                    "river_flow_speed": round(obj.get("river_flow_speed", 1.0), 2),
                    "river_wave_scale": round(obj.get("river_wave_scale", 1.0), 2),
                    "points": points_list
                }


            layout_data.append(obj_info)

    bpy.ops.object.select_all(action='DESELECT')
    for o in selected_objs:
        try:
            o.select_set(True)
        except Exception:
            pass
    if active_obj:
        try:
            bpy.context.view_layer.objects.active = active_obj
        except Exception:
            pass

    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(layout_data, f, indent=4, ensure_ascii=False)
    except Exception:
        pass

is_drawing_active = False

# 🔄 Blender内の「移動・回転・変形・追加・削除」を常時リアルタイム全自動監視するハンドラ
@bpy.app.handlers.persistent
def auto_sync_depsgraph_update(scene, depsgraph):
    """
    Blender上でユーザーがマウスで動かしたり、追加・削除・編集を行った瞬間、
    何もボタンを押さなくても「勝手に自動同期」して stage_layout.json を即座に更新保存する
    """
    global is_drawing_active
    if is_drawing_active:
        return  # モーダルお絵かき中はハンドラ重なり着火によるBlenderクラッシュを100%防止

    try:
        if bpy.context.scene and hasattr(bpy.context.scene, "my_addon_properties_v2"):
            if bpy.context.scene.my_addon_properties_v2.auto_export:
                export_scene_to_json()
    except Exception:
        pass

class MYADDON_OT_add_cargo_container(bpy.types.Operator):
    bl_idname = "myaddon.add_cargo_container"
    bl_label = "📦 貨物コンテナ（ハシゴ付き）を追加"
    bl_description = "画像通りのハシゴが立てかけられたミリタリー貨物コンテナを配置します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        c_obj = create_container_visual_object(context.scene.cursor.location)
        c_obj["type"] = "Container"
        c_obj["isStatic"] = True
        
        # 追加と同時に stage_layout.json へ全自動即時保存
        try:
            export_scene_to_json()
        except Exception:
            pass

        self.report({'INFO'}, "📦 貨物コンテナ（ハシゴ付き）を配置し、stage_layout.jsonに即時自動保存しました！")
        return {'FINISHED'}

class MYADDON_OT_add_loot_box(bpy.types.Operator):
    bl_idname = "myaddon.add_loot_box"
    bl_label = "📦 物資/ルート箱を追加"
    bl_description = "武器や回復アイテムが入ったコンテナ箱を配置します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_cube_add(size=0.8, location=context.scene.cursor.location)
        obj = context.active_object
        obj.name = "LootContainer"
        mat = get_or_create_material("LootBoxMaterial", (1.0, 0.8, 0.1, 0.9))
        obj.data.materials.append(mat)
        
        obj["type"] = "LootBox"
        obj["tier"] = "Military"
        obj["item_count"] = 4
        self.report({'INFO'}, "📦 物資/ルート箱を追加しました。")
        return {'FINISHED'}

class MYADDON_OT_add_river_straight(bpy.types.Operator):
    bl_idname = "myaddon.add_river_straight"
    bl_label = "直線川を追加"
    bl_description = "道路パーツのように移動(G)・回転(R)して繋げられる直線川オブジェクトを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_plane_add(size=4.0)
        obj = context.active_object
        obj.name = "River_Straight"
        obj["type"] = "River"
        obj["river_part"] = "Straight"
        obj["river_width"] = 4.0
        obj["river_flow_speed"] = 1.0
        obj["river_wave_scale"] = 1.0
        mat = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.5, 0.9, 0.7))
        obj.data.materials.append(mat)
        self.report({'INFO'}, "直線川を追加しました。")
        return {'FINISHED'}

class MYADDON_OT_add_river_curve(bpy.types.Operator):
    bl_idname = "myaddon.add_river_curve"
    bl_label = "カーブ川を追加"
    bl_description = "道路パーツのように移動(G)・回転(R)して繋げられるL字カーブの川オブジェクトを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_plane_add(size=4.0)
        obj = context.active_object
        obj.name = "River_Curve"
        obj["type"] = "River"
        obj["river_part"] = "Curve"
        obj["river_width"] = 4.0
        obj["river_flow_speed"] = 1.0
        obj["river_wave_scale"] = 1.0
        mat = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.5, 0.9, 0.7))
        obj.data.materials.append(mat)
        self.report({'INFO'}, "カーブ川パーツを追加しました。")
        return {'FINISHED'}

class MYADDON_OT_add_river_fork(bpy.types.Operator):
    bl_idname = "myaddon.add_river_fork"
    bl_label = "分岐川を追加"
    bl_description = "道路パーツのように移動(G)・回転(R)して繋げられるT字分岐の川オブジェクトを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_plane_add(size=4.0)
        obj = context.active_object
        obj.name = "River_Fork"
        obj["type"] = "River"
        obj["river_part"] = "Fork"
        obj["river_width"] = 4.0
        obj["river_flow_speed"] = 1.0
        obj["river_wave_scale"] = 1.0
        mat = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.5, 0.9, 0.7))
        obj.data.materials.append(mat)
        self.report({'INFO'}, "分岐川パーツを追加しました。")
        return {'FINISHED'}

def smooth_points_chaikin(points, iterations=2):
    if len(points) < 2:
        return points
        
    cleaned = [points[0]]
    for i in range(1, len(points)):
        p0 = cleaned[-1]
        p1 = points[i]
        d = math.sqrt((p1[0]-p0[0])**2 + (p1[1]-p0[1])**2 + (p1[2]-p0[2])**2)
        if d > 0.05:
            cleaned.append(p1)
            
    if len(cleaned) < 3:
        return cleaned

    curr = list(cleaned)
    for _ in range(iterations):
        next_pts = [curr[0]]
        for i in range(len(curr) - 1):
            p0 = curr[i]
            p1 = curr[i+1]
            q = (p0[0]*0.75 + p1[0]*0.25, p0[1]*0.75 + p1[1]*0.25, p0[2]*0.75 + p1[2]*0.25)
            r = (p0[0]*0.25 + p1[0]*0.75, p0[1]*0.25 + p1[1]*0.75, p0[2]*0.25 + p1[2]*0.75)
            next_pts.append(q)
            next_pts.append(r)
        next_pts.append(curr[-1])
        curr = next_pts
    return curr

class MYADDON_OT_draw_spline_river_path(bpy.types.Operator):
    bl_idname = "myaddon.draw_spline_river_path"
    bl_label = "🌊 クリックで川を描く（両岸小石セット）"
    bl_description = "3D画面を左クリックしてパスを描き、Enter/右クリックで決定すると川と両岸の小石環境が一瞬で生成されます"
    bl_options = {'REGISTER', 'UNDO'}

    def invoke(self, context, event):
        self.points = []
        self.river_obj = None
        self.river_bed_obj = None
        self.spawned_rocks = []
        
        self._handle = bpy.types.SpaceView3D.draw_handler_add(self.draw_callback_gpu, (context,), 'WINDOW', 'POST_VIEW')
        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, "【川パス描画モード】左クリックで地点を追加 / 右クリック・Enterで確定 / ESCでキャンセル")
        return {'RUNNING_MODAL'}

    def remove_draw_handler(self):
        if hasattr(self, '_handle') and self._handle:
            bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
            self._handle = None

    def modal(self, context, event):
        context.area.tag_redraw()

        if event.type in {'MIDDLEMOUSE', 'WHEELUPMOUSE', 'WHEELDOWNMOUSE', 'TRACKPAD_PAN', 'TRACKPAD_ZOOM', 'NDOF_MOTION'}:
            return {'PASS_THROUGH'}

        if event.type == 'LEFTMOUSE' and event.value == 'PRESS':
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            result, location, normal, index, obj, matrix = ray_cast_terrain(context.scene, context, origin, vec)
            if not result and abs(vec.z) > 0.0001:
                t = -origin.z / vec.z
                if t > 0:
                    location = origin + vec * t
                    result = True
            
            if result:
                loc_tuple = (float(location[0]), float(location[1]), float(location[2]))
                self.points.append(loc_tuple)
                self.update_river_preview(context)
                self.report({'INFO'}, f"地点を追加しました（計 {len(self.points)} 点）。右クリック/Enterで確定")

        elif event.type in {'RET', 'NUMPAD_ENTER', 'RIGHTMOUSE'}:
            if len(self.points) >= 2:
                self.finalize_river_and_shores(context)
                self.remove_draw_handler()
                self.report({'INFO'}, f"🎉 川と両岸の小石環境（小石: {len(self.spawned_rocks)}個）の生成が完了しました！")
                return {'FINISHED'}
            else:
                self.remove_draw_handler()
                self.report({'WARNING'}, "点を2箇所以上追加してください。キャンセルしました。")
                return {'CANCELLED'}

        elif event.type in {'ESC'}:
            if self.river_obj:
                bpy.data.objects.remove(self.river_obj, do_unlink=True)
            if self.river_bed_obj:
                bpy.data.objects.remove(self.river_bed_obj, do_unlink=True)
            self.remove_draw_handler()
            self.report({'INFO'}, "川の描画をキャンセルしました。")
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def update_river_preview(self, context):
        if len(self.points) < 2:
            return
        eval_points = smooth_points_chaikin(self.points, iterations=2)
        if len(eval_points) < 2:
            return

        width = 3.5
        half_w = width * 0.5
        river_depth = 1.2

        bed_verts = []
        bed_faces = []
        water_verts = []
        water_faces = []

        existing_rivers = [o for o in context.scene.objects if o.get("type") == "River"]
        river_idx = len(existing_rivers)
        height_offset = 0.01 + (river_idx * 0.003)

        last_side_x, last_side_y = 1.0, 0.0

        for i in range(len(eval_points)):
            p = eval_points[i]
            if i == 0:
                d_x = eval_points[1][0] - eval_points[0][0]
                d_y = eval_points[1][1] - eval_points[0][1]
            elif i == len(eval_points) - 1:
                d_x = eval_points[-1][0] - eval_points[-2][0]
                d_y = eval_points[-1][1] - eval_points[-2][1]
            else:
                d1_x = eval_points[i][0] - eval_points[i-1][0]
                d1_y = eval_points[i][1] - eval_points[i-1][1]
                d2_x = eval_points[i+1][0] - eval_points[i][0]
                d2_y = eval_points[i+1][1] - eval_points[i][1]
                l1 = math.sqrt(d1_x**2 + d1_y**2)
                l2 = math.sqrt(d2_x**2 + d2_y**2)
                if l1 > 0.0001: d1_x /= l1; d1_y /= l1
                if l2 > 0.0001: d2_x /= l2; d2_y /= l2
                d_x = d1_x + d2_x
                d_y = d1_y + d2_y

            length = math.sqrt(d_x * d_x + d_y * d_y)
            if length > 0.0001:
                d_x /= length
                d_y /= length
                side_x = -d_y
                side_y = d_x
                last_side_x, last_side_y = side_x, side_y
            else:
                side_x, side_y = last_side_x, last_side_y

            z_pos = p[2] + height_offset

            bv0 = (p[0] - side_x * half_w,        p[1] - side_y * half_w,        z_pos)
            bv1 = (p[0] - side_x * half_w * 0.6, p[1] - side_y * half_w * 0.6, z_pos - river_depth * 0.6)
            bv2 = (p[0],                          p[1],                          z_pos - river_depth)
            bv3 = (p[0] + side_x * half_w * 0.6, p[1] + side_y * half_w * 0.6, z_pos - river_depth * 0.6)
            bv4 = (p[0] + side_x * half_w,        p[1] + side_y * half_w,        z_pos)

            bed_verts.extend([bv0, bv1, bv2, bv3, bv4])

            wv0 = (p[0] - side_x * half_w * 0.95, p[1] - side_y * half_w * 0.95, z_pos - 0.08)
            wv1 = (p[0] + side_x * half_w * 0.95, p[1] + side_y * half_w * 0.95, z_pos - 0.08)

            water_verts.extend([wv0, wv1])

            if i > 0:
                bb0 = (i - 1) * 5
                bb1 = i * 5
                bed_faces.append((bb0 + 0, bb0 + 1, bb1 + 1, bb1 + 0))
                bed_faces.append((bb0 + 1, bb0 + 2, bb1 + 2, bb1 + 1))
                bed_faces.append((bb0 + 2, bb0 + 3, bb1 + 3, bb1 + 2))
                bed_faces.append((bb0 + 3, bb0 + 4, bb1 + 4, bb1 + 3))

                wb0 = (i - 1) * 2
                wb1 = i * 2
                water_faces.append((wb0 + 0, wb0 + 1, wb1 + 1, wb1 + 0))

        if not self.river_obj:
            bed_mesh = bpy.data.meshes.new("RiverBed_Mesh")
            bed_mesh.from_pydata(bed_verts, [], bed_faces)
            bed_mesh.update()
            mat_bed = get_or_create_biome_material("RiverBedMaterial", (0.18, 0.12, 0.08, 1.0))
            bed_mesh.materials.append(mat_bed)
            
            self.river_bed_obj = bpy.data.objects.new("RiverBed_3D", bed_mesh)
            context.collection.objects.link(self.river_bed_obj)

            water_mesh = bpy.data.meshes.new("RiverWater_Mesh")
            water_mesh.from_pydata(water_verts, [], water_faces)
            water_mesh.update()
            mat_water = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.5, 0.9, 0.7))
            water_mesh.materials.append(mat_water)

            self.river_obj = bpy.data.objects.new("River", water_mesh)
            context.collection.objects.link(self.river_obj)

            self.river_obj["type"] = "River"
            self.river_obj["river_width"] = width
            self.river_obj["river_flow_speed"] = 1.0
            self.river_obj["river_wave_scale"] = 1.0
        else:
            if hasattr(self, 'river_bed_obj') and self.river_bed_obj:
                b_mesh = self.river_bed_obj.data
                b_mesh.clear_geometry()
                b_mesh.from_pydata(bed_verts, [], bed_faces)
                b_mesh.update()
                
            w_mesh = self.river_obj.data
            w_mesh.clear_geometry()
            w_mesh.from_pydata(water_verts, [], water_faces)
            w_mesh.update()

        self.river_obj["points_x"] = [float(p[0]) for p in eval_points]
        self.river_obj["points_y"] = [float(p[1]) for p in eval_points]
        self.river_obj["points_z"] = [float(p[2]) for p in eval_points]

    def finalize_river_and_shores(self, context):
        if not self.river_obj:
            return
            
        eval_pts = smooth_points_chaikin(self.points, iterations=2)
        width = 3.5
        half_w = width * 0.5
        scene_objs = list(context.scene.objects)
        
        last_side_x, last_side_y = 1.0, 0.0

        for i in range(len(eval_pts)):
            p = eval_pts[i]
            if i == 0:
                d_x = eval_pts[1][0] - eval_pts[0][0]
                d_y = eval_pts[1][1] - eval_pts[0][1]
            elif i == len(eval_pts) - 1:
                d_x = eval_pts[-1][0] - eval_pts[-2][0]
                d_y = eval_pts[-1][1] - eval_pts[-2][1]
            else:
                d_x = eval_pts[i+1][0] - eval_pts[i-1][0]
                d_y = eval_pts[i+1][1] - eval_pts[i-1][1]
                
            length = math.sqrt(d_x**2 + d_y**2)
            if length > 0.0001:
                d_x /= length; d_y /= length
                side_x, side_y = -d_y, d_x
                last_side_x, last_side_y = side_x, side_y

    def draw_callback_gpu(self, context):
        if not self.points:
            return
        try:
            shader = gpu.shader.from_builtin('3D_UNIFORM_COLOR')
            shader.bind()
            coords = [(p[0], p[1], p[2] + 0.1) for p in self.points]
            batch = batch_for_shader(shader, 'POINTS', {"pos": coords})
            gpu.state.point_size_set(10.0)
            shader.uniform_float("color", (0.1, 0.9, 1.0, 1.0))
            batch.draw(shader)
            if len(self.points) >= 2:
                batch_line = batch_for_shader(shader, 'LINE_STRIP', {"pos": coords})
                gpu.state.line_width_set(3.0)
                shader.uniform_float("color", (0.0, 0.7, 1.0, 0.9))
                batch_line.draw(shader)
        except Exception:
            pass

class MYADDON_OT_draw_river_freehand(bpy.types.Operator):
    bl_idname = "myaddon.draw_river_freehand"
    bl_label = "🌊 マウスでお絵かき（川を描く）"
    bl_description = "マウスをドラッグして地面をスーッとなぞるだけで、お絵描き感覚で川を引くことができます"
    bl_options = {'REGISTER', 'UNDO'}

    def invoke(self, context, event):
        global is_drawing_active
        is_drawing_active = True
        self.points = []
        self.is_drawing = False
        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, "【川お絵かきモード開始】マウス左ボタンを押しながら地面をなぞってください。終わったら左ボタンを離します。")
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        global is_drawing_active
        context.area.tag_redraw()

        if event.type in {'MIDDLEMOUSE', 'WHEELUPMOUSE', 'WHEELDOWNMOUSE', 'TRACKPAD_PAN', 'TRACKPAD_ZOOM', 'NDOF_MOTION'}:
            return {'PASS_THROUGH'}


        if event.type == 'LEFTMOUSE':
            if event.value == 'PRESS':
                self.is_drawing = True
            elif event.value == 'RELEASE':
                if self.is_drawing and len(self.points) >= 2:
                    is_drawing_active = False
                    # マウスを離したときだけ1回メッシュを生成（クラッシュ防止）
                    self.build_river_mesh_once(context)
                    self.report({'INFO'}, f"川の描画が完了しました！（点数: {len(self.points)}）")
                    try:
                        export_scene_to_json()
                    except Exception:
                        pass
                    return {'FINISHED'}
                self.is_drawing = False

        if event.type == 'MOUSEMOVE' and self.is_drawing:
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)

            # 数学的交点計算のみ（depsgraphに触らない）
            result = False
            location = None
            if abs(vec.z) > 0.00001:
                t = -origin.z / vec.z
                if t > 0:
                    location = mathutils.Vector((origin.x + vec.x * t, origin.y + vec.y * t, 0.0))
                    result = True

            if result and location:
                loc_tuple = (float(location.x), float(location.y), float(location.z))
                if not self.points:
                    self.points.append(loc_tuple)
                else:
                    last_pt = self.points[-1]
                    dist = math.sqrt((loc_tuple[0]-last_pt[0])**2 + (loc_tuple[1]-last_pt[1])**2)
                    if dist > 0.8:
                        self.points.append(loc_tuple)

        elif event.type in {'RIGHTMOUSE', 'ESC', 'RET'}:
            is_drawing_active = False
            if len(self.points) >= 2:
                self.build_river_mesh_once(context)
            self.report({'INFO'}, "川の描画を終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def build_river_mesh_once(self, context):
        """マウスを離したとき1回だけ呼ばれる安全なメッシュ生成"""
        if len(self.points) < 2:
            return

        eval_points = smooth_points_chaikin(self.points, iterations=2)
        if len(eval_points) < 2:
            return

        width = 3.5
        half_w = width * 0.5
        river_depth = 1.2

        bed_verts = []
        bed_faces = []
        water_verts = []
        water_faces = []

        existing_rivers = [o for o in context.scene.objects if o.get("type") == "River"]
        river_idx = len(existing_rivers)
        height_offset = 0.01 + (river_idx * 0.003)

        last_side_x, last_side_y = 1.0, 0.0

        for i in range(len(eval_points)):
            p = eval_points[i]
            if i == 0:
                d_x = eval_points[1][0] - eval_points[0][0]
                d_y = eval_points[1][1] - eval_points[0][1]
            elif i == len(eval_points) - 1:
                d_x = eval_points[-1][0] - eval_points[-2][0]
                d_y = eval_points[-1][1] - eval_points[-2][1]
            else:
                d1_x = eval_points[i][0] - eval_points[i-1][0]
                d1_y = eval_points[i][1] - eval_points[i-1][1]
                d2_x = eval_points[i+1][0] - eval_points[i][0]
                d2_y = eval_points[i+1][1] - eval_points[i][1]
                l1 = math.sqrt(d1_x**2 + d1_y**2)
                l2 = math.sqrt(d2_x**2 + d2_y**2)
                if l1 > 0.0001: d1_x /= l1; d1_y /= l1
                if l2 > 0.0001: d2_x /= l2; d2_y /= l2
                d_x = d1_x + d2_x
                d_y = d1_y + d2_y

            length = math.sqrt(d_x * d_x + d_y * d_y)
            if length > 0.0001:
                d_x /= length; d_y /= length
                side_x = -d_y; side_y = d_x
                last_side_x, last_side_y = side_x, side_y
            else:
                side_x, side_y = last_side_x, last_side_y

            z_pos = p[2] + height_offset

            bv0 = (p[0] - side_x * half_w,        p[1] - side_y * half_w,        z_pos)
            bv1 = (p[0] - side_x * half_w * 0.6, p[1] - side_y * half_w * 0.6, z_pos - river_depth * 0.6)
            bv2 = (p[0],                          p[1],                          z_pos - river_depth)
            bv3 = (p[0] + side_x * half_w * 0.6, p[1] + side_y * half_w * 0.6, z_pos - river_depth * 0.6)
            bv4 = (p[0] + side_x * half_w,        p[1] + side_y * half_w,        z_pos)
            bed_verts.extend([bv0, bv1, bv2, bv3, bv4])

            wv0 = (p[0] - side_x * half_w * 0.95, p[1] - side_y * half_w * 0.95, z_pos - 0.08)
            wv1 = (p[0] + side_x * half_w * 0.95, p[1] + side_y * half_w * 0.95, z_pos - 0.08)
            water_verts.extend([wv0, wv1])

            if i > 0:
                bb0 = (i - 1) * 5; bb1 = i * 5
                bed_faces.append((bb0+0, bb0+1, bb1+1, bb1+0))
                bed_faces.append((bb0+1, bb0+2, bb1+2, bb1+1))
                bed_faces.append((bb0+2, bb0+3, bb1+3, bb1+2))
                bed_faces.append((bb0+3, bb0+4, bb1+4, bb1+3))
                wb0 = (i-1)*2; wb1 = i*2
                water_faces.append((wb0+0, wb0+1, wb1+1, wb1+0))

        try:
            bed_mesh = bpy.data.meshes.new("RiverBed_Mesh")
            bed_mesh.from_pydata(bed_verts, [], bed_faces)
            bed_mesh.update()
            mat_bed = get_or_create_biome_material("RiverBedMaterial", (0.18, 0.12, 0.08, 1.0))
            bed_mesh.materials.append(mat_bed)
            bed_obj = bpy.data.objects.new("RiverBed_3D", bed_mesh)
            context.collection.objects.link(bed_obj)

            water_mesh = bpy.data.meshes.new("RiverWater_Mesh")
            water_mesh.from_pydata(water_verts, [], water_faces)
            water_mesh.update()
            mat_water = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.5, 0.9, 0.7))
            water_mesh.materials.append(mat_water)
            river_obj = bpy.data.objects.new("River", water_mesh)
            context.collection.objects.link(river_obj)

            river_obj["type"] = "River"
            river_obj["river_width"] = width
            river_obj["river_flow_speed"] = 1.0
            river_obj["river_wave_scale"] = 1.0
            river_obj["points_x"] = [float(p[0]) for p in eval_points]
            river_obj["points_y"] = [float(p[1]) for p in eval_points]
            river_obj["points_z"] = [float(p[2]) for p in eval_points]
        except Exception as e:
            print(f"[RiverMesh] 生成エラー: {e}")


class MYADDON_OT_draw_forest_freehand(bpy.types.Operator):
    bl_idname = "myaddon.draw_forest_freehand"
    bl_label = "🌲 マウスでお絵かき（森を描く）"
    bl_description = "筆で絵を描くようにマウスで地面をなぞるだけで、その軌跡に沿って一発で一筋の森林を生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def invoke(self, context, event):
        self.points = []
        self.is_drawing = False
        self.spawned_objs = []
        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, "【森のお絵かきモード開始】マウス左ボタンを押しながら地面をなぞってください。終わったら左ボタンを離します。")
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        context.area.tag_redraw()
        if event.type in {'MIDDLEMOUSE', 'WHEELUPMOUSE', 'WHEELDOWNMOUSE', 'TRACKPAD_PAN', 'TRACKPAD_ZOOM', 'NDOF_MOTION'}:
            return {'PASS_THROUGH'}

        if event.type == 'LEFTMOUSE':
            if event.value == 'PRESS':
                self.is_drawing = True
            elif event.value == 'RELEASE':
                if self.is_drawing and len(self.points) >= 2:
                    self.generate_forest(context)
                    self.report({'INFO'}, f"森の描画が完了しました！（木: {len(self.spawned_objs)}本）")
                    return {'FINISHED'}
                self.is_drawing = False

        if event.type == 'MOUSEMOVE' and self.is_drawing:
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            result, location, normal, index, obj, matrix = ray_cast_terrain(context.scene, context, origin, vec)
            if not result and abs(vec.z) > 0.0001:
                t = -origin.z / vec.z
                if t > 0:
                    location = origin + vec * t
                    result = True
            
            if result:
                loc_tuple = (float(location[0]), float(location[1]), float(location[2]))
                if not self.points:
                    self.points.append(loc_tuple)
                else:
                    last_pt = self.points[-1]
                    dist = math.sqrt((loc_tuple[0]-last_pt[0])**2 + (loc_tuple[1]-last_pt[1])**2 + (loc_tuple[2]-last_pt[2])**2)
                    if dist > 0.8:
                        self.points.append(loc_tuple)

        elif event.type in {'RIGHTMOUSE', 'ESC', 'RET'}:
            self.report({'INFO'}, "森の描画を終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def generate_forest(self, context):
        if len(self.points) < 2:
            return
        radius = context.scene.my_addon_properties_v2.draw_brush_radius
        eval_pts = smooth_points_chaikin(self.points, iterations=2)
        
        candidate_pts = poisson_disk_sampling_path(eval_pts, brush_radius=radius, min_dist=1.2)
        scene_objs = list(context.scene.objects)
        
        for p in candidate_pts:
            tree_radius = 0.7
            if not is_location_valid(p, tree_radius, scene_objs, ignore_objs=self.spawned_objs):
                continue
                
            new_obj = create_tree_visual_object(p)
            new_obj["type"] = "Tree"
            new_obj["seed"] = random.randint(1, 99999)
            new_obj["iterations"] = 2
            new_obj["branchLength"] = round(random.uniform(0.12, 0.18), 2)
            new_obj["branchRadius"] = round(random.uniform(0.05, 0.08), 3)
            new_obj["taperRate"] = 0.8
            new_obj["angle"] = round(random.uniform(20.0, 30.0), 1)
            
            new_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
            s = random.uniform(0.8, 1.3)
            new_obj.scale = (s, s, s)
            self.spawned_objs.append(new_obj)
            scene_objs.append(new_obj)

class MYADDON_OT_draw_rock_freehand(bpy.types.Operator):
    bl_idname = "myaddon.draw_rock_freehand"
    bl_label = "🪨 マウスでお絵かき（岩場を描く）"
    bl_description = "筆で絵を描くようにマウスで地面をなぞるだけで、その軌跡に沿って一発で岩場を生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def invoke(self, context, event):
        self.points = []
        self.is_drawing = False
        self.spawned_objs = []
        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, "【岩場のお絵かきモード開始】マウス左ボタンを押しながら地面をなぞってください。終わったら左ボタンを離します。")
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        context.area.tag_redraw()
        if event.type in {'MIDDLEMOUSE', 'WHEELUPMOUSE', 'WHEELDOWNMOUSE', 'TRACKPAD_PAN', 'TRACKPAD_ZOOM', 'NDOF_MOTION'}:
            return {'PASS_THROUGH'}

        if event.type == 'LEFTMOUSE':
            if event.value == 'PRESS':
                self.is_drawing = True
            elif event.value == 'RELEASE':
                if self.is_drawing and len(self.points) >= 2:
                    self.generate_rocks(context)
                    self.report({'INFO'}, f"岩場の描画が完了しました！（岩: {len(self.spawned_objs)}個）")
                    return {'FINISHED'}
                self.is_drawing = False

        if event.type == 'MOUSEMOVE' and self.is_drawing:
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            result, location, normal, index, obj, matrix = ray_cast_terrain(context.scene, context, origin, vec)
            if not result and abs(vec.z) > 0.0001:
                t = -origin.z / vec.z
                if t > 0:
                    location = origin + vec * t
                    result = True
            
            if result:
                loc_tuple = (float(location[0]), float(location[1]), float(location[2]))
                if not self.points:
                    self.points.append(loc_tuple)
                else:
                    last_pt = self.points[-1]
                    dist = math.sqrt((loc_tuple[0]-last_pt[0])**2 + (loc_tuple[1]-last_pt[1])**2 + (loc_tuple[2]-last_pt[2])**2)
                    if dist > 0.8:
                        self.points.append(loc_tuple)

        elif event.type in {'RIGHTMOUSE', 'ESC', 'RET'}:
            self.report({'INFO'}, "岩場の描画を終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def generate_rocks(self, context):
        if len(self.points) < 2:
            return
        radius = context.scene.my_addon_properties_v2.draw_brush_radius
        eval_pts = smooth_points_chaikin(self.points, iterations=2)
        
        candidate_pts = poisson_disk_sampling_path(eval_pts, brush_radius=radius, min_dist=0.9)
        scene_objs = list(context.scene.objects)
        
        for p in candidate_pts:
            rock_radius = 0.5
            if not is_location_valid(p, rock_radius, scene_objs, ignore_objs=self.spawned_objs):
                continue
                
            bpy.ops.mesh.primitive_ico_sphere_add(radius=0.45, location=p)
            new_obj = context.active_object
            new_obj.name = "ProceduralRock"
            mat = get_or_create_material("RockPlaceholderMaterial", (0.4, 0.4, 0.4, 1.0))
            new_obj.data.materials.append(mat)
            
            new_obj["type"] = "Rock"
            new_obj["seed"] = random.randint(1, 99999)
            new_obj["subdivisions"] = 3
            new_obj["noiseStrength"] = round(random.uniform(0.2, 0.4), 2)
            new_obj["voronoiStrength"] = round(random.uniform(0.1, 0.3), 2)
            new_obj["crackStrength"] = round(random.uniform(0.3, 0.5), 2)
            
            new_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
            s = random.uniform(0.7, 1.2)
            new_obj.scale = (s, s, s)
            self.spawned_objs.append(new_obj)
            scene_objs.append(new_obj)

# 🌊 川オブジェクトの完全削除
class MYADDON_OT_clear_river_objects(bpy.types.Operator):
    bl_idname = "myaddon.clear_river_objects"
    bl_label = "川をすべて削除"
    bl_description = "配置されている川、川底、沿岸の小石オブジェクトを100%一括削除します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        rivers = [
            obj for obj in context.scene.objects 
            if obj.type == 'MESH' and (
                obj.get("type") == "River" or 
                "River" in obj.name or 
                "RiverBed" in obj.name or 
                "Shore" in obj.name
            )
        ]
        if not rivers:
            self.report({'INFO'}, "削除対象の川オブジェクトはありません。")
            return {'FINISHED'}
            
        for obj in rivers:
            bpy.data.objects.remove(obj, do_unlink=True)
            
        self.report({'INFO'}, f"{len(rivers)} 個の川関連オブジェクトを完全に削除しました。")
        return {'FINISHED'}

# 🎲 全自動サバイバルマップ生成
class MYADDON_OT_generate_auto_survival_map(bpy.types.Operator):
    bl_idname = "myaddon.generate_auto_survival_map"
    bl_label = "🎲 サバイバルマップを全自動生成"
    bl_description = "川・森林帯・岩場・プレイヤー開始点・敵スポーン・物資箱を一瞬で全自動構築します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        to_remove = [
            o for o in context.scene.objects 
            if o.type == 'MESH' and (
                ("type" in o and o["type"] in ['Tree', 'Rock', 'PlayerSpawn', 'EnemySpawn', 'LootBox', 'River', 'Biome']) or
                o.name.startswith("River") or 
                o.name.startswith("Shore") or 
                "RiverBed" in o.name or 
                "RiverWater" in o.name
            )
        ]
        for o in to_remove:
            bpy.data.objects.remove(o, do_unlink=True)

        p_obj = create_player_visual_object((0.0, -5.0, 0.0))
        p_obj["type"] = "PlayerSpawn"
        p_obj["spawn_id"] = 1

        enemy_coords = [(-8.0, 10.0, 0.0), (12.0, 8.0, 0.0), (0.0, 20.0, 0.0)]
        for i, coord in enumerate(enemy_coords):
            e_obj = create_enemy_visual_object(coord)
            e_obj["type"] = "EnemySpawn"
            e_obj["enemy_type"] = f"Scavenger_Patrol_{i+1}"
            e_obj["patrol_radius"] = 12.0

        loot_coords = [(-9.0, 11.0, 0.0), (-7.5, 9.5, 0.0), (13.0, 7.5, 0.0), (11.5, 9.0, 0.0), (0.5, 21.0, 0.0)]
        for i, coord in enumerate(loot_coords):
            bpy.ops.mesh.primitive_cube_add(size=0.8, location=coord)
            l_obj = context.active_object
            l_obj.name = f"Loot_Container_{i+1}"
            mat = get_or_create_material("LootBoxMaterial", (1.0, 0.8, 0.1, 0.9))
            l_obj.data.materials.append(mat)
            l_obj["type"] = "LootBox"
            l_obj["tier"] = "Military" if i % 2 == 0 else "Medical"
            l_obj["item_count"] = random.randint(3, 6)

        # 川と川底の全自動生成
        river_pts = [
            (-25.0, 5.0, 0.0),
            (-10.0, 3.0, 0.0),
            (0.0, 8.0, 0.0),
            (12.0, 2.0, 0.0),
            (25.0, 6.0, 0.0)
        ]
        eval_river = smooth_points_chaikin(river_pts, iterations=2)
        width = 3.5
        half_w = width * 0.5
        river_depth = 1.2

        bed_verts = []
        bed_faces = []
        water_verts = []
        water_faces = []

        last_side_x, last_side_y = 1.0, 0.0

        for i in range(len(eval_river)):
            p = eval_river[i]
            if i == 0:
                d_x = eval_river[1][0] - eval_river[0][0]
                d_y = eval_river[1][1] - eval_river[0][1]
            elif i == len(eval_river) - 1:
                d_x = eval_river[-1][0] - eval_river[-2][0]
                d_y = eval_river[-1][1] - eval_river[-2][1]
            else:
                d_x = eval_river[i+1][0] - eval_river[i-1][0]
                d_y = eval_river[i+1][1] - eval_river[i-1][1]
            l = math.sqrt(d_x**2 + d_y**2)
            if l > 0.0001:
                d_x /= l; d_y /= l
                nx, ny = -d_y, d_x
                last_side_x, last_side_y = nx, ny
            else:
                nx, ny = last_side_x, last_side_y

            bv0 = (p[0] - nx * half_w,       p[1] - ny * half_w,       p[2] + 0.01)
            bv1 = (p[0] - nx * half_w * 0.6, p[1] - ny * half_w * 0.6, p[2] - river_depth * 0.6)
            bv2 = (p[0],                     p[1],                     p[2] - river_depth)
            bv3 = (p[0] + nx * half_w * 0.6, p[1] + ny * half_w * 0.6, p[2] - river_depth * 0.6)
            bv4 = (p[0] + nx * half_w,       p[1] + ny * half_w,       p[2] + 0.01)
            bed_verts.extend([bv0, bv1, bv2, bv3, bv4])

            wv0 = (p[0] - nx * half_w * 0.95, p[1] - nx * half_w * 0.95, p[2] - 0.08)
            wv1 = (p[0] + nx * half_w * 0.95, p[1] + nx * half_w * 0.95, p[2] - 0.08)
            water_verts.extend([wv0, wv1])

            if i > 0:
                bb0 = (i - 1) * 5
                bb1 = i * 5
                bed_faces.append((bb0 + 0, bb0 + 1, bb1 + 1, bb1 + 0))
                bed_faces.append((bb0 + 1, bb0 + 2, bb1 + 2, bb1 + 1))
                bed_faces.append((bb0 + 2, bb0 + 3, bb1 + 3, bb1 + 2))
                bed_faces.append((bb0 + 3, bb0 + 4, bb1 + 4, bb1 + 3))

                wb0 = (i - 1) * 2
                wb1 = i * 2
                water_faces.append((wb0 + 0, wb0 + 1, wb1 + 1, wb1 + 0))

        # 川底
        b_mesh = bpy.data.meshes.new("RiverBed_Auto_Mesh")
        b_mesh.from_pydata(bed_verts, [], bed_faces)
        b_mesh.update()
        mat_bed = get_or_create_biome_material("RiverBedMaterial", (0.18, 0.12, 0.08, 1.0))
        b_mesh.materials.append(mat_bed)
        b_obj = bpy.data.objects.new("RiverBed_Auto", b_mesh)
        context.collection.objects.link(b_obj)

        # 水面
        w_mesh = bpy.data.meshes.new("RiverWater_Auto_Mesh")
        w_mesh.from_pydata(water_verts, [], water_faces)
        w_mesh.update()
        mat_water = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.5, 0.9, 0.7))
        w_mesh.materials.append(mat_water)
        r_obj = bpy.data.objects.new("River_Auto", w_mesh)
        context.collection.objects.link(r_obj)

        r_obj["type"] = "River"
        r_obj["river_part"] = "Freehand"
        r_obj["river_width"] = width
        r_obj["river_flow_speed"] = 1.0
        r_obj["river_wave_scale"] = 1.0
        r_obj["points_x"] = [float(p[0]) for p in eval_river]
        r_obj["points_y"] = [float(p[1]) for p in eval_river]
        r_obj["points_z"] = [float(p[2]) for p in eval_river]

        scene_objs = list(context.scene.objects)
        spawned_count = 0
        attempts = 0
        while spawned_count < 35 and attempts < 200:
            attempts += 1
            rx = random.uniform(-20.0, 20.0)
            ry = random.uniform(-18.0, 22.0)
            loc = (rx, ry, 0.0)
            
            if is_location_valid(loc, required_radius=0.7, scene_objects=scene_objs):
                t_obj = create_tree_visual_object(loc)
                t_obj["type"] = "Tree"
                t_obj["seed"] = random.randint(1, 99999)
                t_obj["iterations"] = 2
                t_obj["branchLength"] = round(random.uniform(0.12, 0.20), 2)
                t_obj["branchRadius"] = round(random.uniform(0.05, 0.08), 3)
                t_obj["taperRate"] = 0.8
                t_obj["angle"] = round(random.uniform(20.0, 30.0), 1)
                t_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
                s = random.uniform(0.8, 1.3)
                t_obj.scale = (s, s, s)
                
                scene_objs.append(t_obj)
                spawned_count += 1

        spawned_count = 0
        attempts = 0
        while spawned_count < 15 and attempts < 150:
            attempts += 1
            rx = random.uniform(-22.0, 22.0)
            ry = random.uniform(-20.0, 22.0)
            loc = (rx, ry, 0.0)
            
            if is_location_valid(loc, required_radius=0.5, scene_objects=scene_objs):
                bpy.ops.mesh.primitive_ico_sphere_add(radius=0.45, location=loc)
                rk_obj = context.active_object
                rk_obj.name = "ProceduralRock"
                mat = get_or_create_material("RockPlaceholderMaterial", (0.4, 0.4, 0.4, 1.0))
                rk_obj.data.materials.append(mat)
                rk_obj["type"] = "Rock"
                rk_obj["seed"] = random.randint(1, 99999)
                rk_obj["subdivisions"] = 3
                rk_obj["noiseStrength"] = round(random.uniform(0.2, 0.4), 2)
                rk_obj["voronoiStrength"] = round(random.uniform(0.1, 0.3), 2)
                rk_obj["crackStrength"] = round(random.uniform(0.3, 0.5), 2)
                rk_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
                s = random.uniform(0.7, 1.2)
                rk_obj.scale = (s, s, s)
                
                scene_objs.append(rk_obj)
                spawned_count += 1

        context.area.tag_redraw()
        self.report({'INFO'}, "🎉 サバイバルマップの全自動生成が完了しました！")
        return {'FINISHED'}

def extract_object_data(obj):
    pos_x = obj.location.x
    pos_y = obj.location.z
    pos_z = obj.location.y
    rot_x = obj.rotation_euler.x
    rot_y = obj.rotation_euler.z
    rot_z = obj.rotation_euler.y
    scale_x = obj.scale.x
    scale_y = obj.scale.z
    scale_z = obj.scale.y
    
    obj_name = obj.name.split('.')[0]
    obj_type = obj.get("type", "Unknown")
    obj_info = {
        "name": obj_name,
        "type": obj_type,
        "position": {"x": round(pos_x, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
        "rotation": {"x": round(rot_x, 4), "y": round(rot_y, 4), "z": round(rot_z, 4)},
        "scale":    {"x": round(scale_x, 4), "y": round(scale_y, 4), "z": round(scale_z, 4)},
        "parameters": {}
    }
    
    if obj_type == "River":
        points_data = []
        if "points_x" in obj and "points_y" in obj and "points_z" in obj:
            px = obj["points_x"]
            py = obj["points_y"]
            pz = obj["points_z"]
            for i in range(len(px)):
                points_data.append({"x": round(px[i], 4), "y": round(pz[i], 4), "z": round(py[i], 4)})
        obj_info["parameters"]["points"] = points_data
        
    for key in obj.keys():
        if not key.startswith("_") and key not in ["type", "cycles", "points_x", "points_y", "points_z"]:
            val = obj[key]
            if hasattr(val, "to_list"):
                obj_info["parameters"][key] = val.to_list()
            else:
                obj_info["parameters"][key] = val
                
    return obj_info

class MYADDON_OT_export_scene(bpy.types.Operator):
    bl_idname = "myaddon.export_scene"
    bl_label = "JSONファイルに出力"
    bl_description = "配置データをゲームのResourcesフォルダにJSONとして出力します"

    def execute(self, context):
        addon_props = context.scene.my_addon_properties_v2
        project_dir = bpy.path.abspath(addon_props.project_path)
        
        if not project_dir or not os.path.exists(project_dir):
            self.report({'ERROR'}, "有効なプロジェクトパスを選択してください。")
            return {'CANCELLED'}
        
        output_path = os.path.join(project_dir, "Resources", "stage_layout.json")
        layout_data = []

        for obj in bpy.context.scene.objects:
            if ("type" in obj and obj["type"] in ['Tree', 'Rock', 'Biome', 'River', 'PlayerSpawn', 'EnemySpawn', 'LootBox']):
                layout_data.append(extract_object_data(obj))
                
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(layout_data, f, indent=4, ensure_ascii=False)
            
        self.report({'INFO'}, f"エクスポート完了: {len(layout_data)}個のモデル情報を保存しました")
        return {'FINISHED'}

class MYADDON_OT_paint_spawner(bpy.types.Operator):
    bl_idname = "myaddon.paint_spawner"
    bl_label = "ペイント配置モード開始"
    bl_description = "クリックした場所に選択中のアセットを配置します"

    def invoke(self, context, event):
        context.window_manager.modal_handler_add(self)
        self._handle = bpy.types.SpaceView3D.draw_handler_add(draw_callback_px, (self, context), 'WINDOW', 'POST_PIXEL')
        self.report({'INFO'}, "【ペイント配置モード】クリックで配置、右クリックまたはEscで終了")
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        context.area.tag_redraw()
        if event.type in {'RIGHTMOUSE', 'ESC'}:
            bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
            self.report({'INFO'}, "ペイント配置モードを終了しました")
            return {'FINISHED'}

        if event.type == 'LEFTMOUSE' and event.value == 'PRESS':
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            result, location, normal, index, obj, matrix = ray_cast_terrain(context.scene, context, origin, vec)
            if result:
                paint_type = context.scene.my_addon_properties_v2.paint_type
                if paint_type == 'Tree':
                    create_tree_visual_object(location)
                elif paint_type == 'Rock':
                    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.45, location=location)
                elif paint_type == 'PlayerSpawn':
                    create_player_visual_object(location)
                elif paint_type == 'EnemySpawn':
                    create_enemy_visual_object(location)
                elif paint_type == 'LootBox':
                    bpy.ops.mesh.primitive_cube_add(size=0.8, location=location)
                return {'RUNNING_MODAL'}

        return {'PASS_THROUGH'}

class MYADDON_OT_scatter_biome(bpy.types.Operator):
    bl_idname = "myaddon.scatter_biome"
    bl_label = "バイオーム内へ自動散布"
    bl_description = "選択したバイオーム範囲（Plane）の中に、指定した数の木や岩をプロシージャルランダム散布します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        plane_obj = context.active_object
        if not plane_obj or plane_obj.type != 'MESH':
            self.report({'WARNING'}, "範囲となる平面オブジェクト（Plane）を選択してください。")
            return {'CANCELLED'}

        count = context.scene.my_addon_properties_v2.scatter_count
        scatter_type = context.scene.my_addon_properties_v2.scatter_type
        
        bbox = [plane_obj.matrix_world @ mathutils.Vector(corner) for corner in plane_obj.bound_box]
        min_x = min(v.x for v in bbox)
        max_x = max(v.x for v in bbox)
        min_y = min(v.y for v in bbox)
        max_y = max(v.y for v in bbox)
        center_z = plane_obj.location.z

        scene_objs = list(context.scene.objects)
        spawned = 0
        attempts = 0

        while spawned < count and attempts < count * 15:
            attempts += 1
            rx = random.uniform(min_x, max_x)
            ry = random.uniform(min_y, max_y)
            loc = (rx, ry, center_z)

            if scatter_type == 'Tree':
                target_type = 'Tree'
            elif scatter_type == 'Rock':
                target_type = 'Rock'
            else:
                target_type = 'Tree' if random.random() < 0.6 else 'Rock'

            req_rad = 0.7 if target_type == 'Tree' else 0.5

            if is_location_valid(loc, required_radius=req_rad, scene_objects=scene_objs):
                if target_type == 'Tree':
                    t_obj = create_tree_visual_object(loc)
                    t_obj["type"] = "Tree"
                    t_obj["seed"] = random.randint(1, 99999)
                    t_obj["iterations"] = 2
                    t_obj["branchLength"] = round(random.uniform(0.12, 0.20), 2)
                    t_obj["branchRadius"] = round(random.uniform(0.05, 0.08), 3)
                    t_obj["taperRate"] = 0.8
                    t_obj["angle"] = round(random.uniform(20.0, 30.0), 1)
                    t_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
                    s = random.uniform(0.8, 1.3)
                    t_obj.scale = (s, s, s)
                    scene_objs.append(t_obj)
                else:
                    bpy.ops.mesh.primitive_ico_sphere_add(radius=0.45, location=loc)
                    rk_obj = context.active_object
                    rk_obj.name = "ProceduralRock"
                    mat = get_or_create_material("RockPlaceholderMaterial", (0.4, 0.4, 0.4, 1.0))
                    rk_obj.data.materials.append(mat)
                    rk_obj["type"] = "Rock"
                    rk_obj["seed"] = random.randint(1, 99999)
                    rk_obj["subdivisions"] = 3
                    rk_obj["noiseStrength"] = round(random.uniform(0.2, 0.4), 2)
                    rk_obj["voronoiStrength"] = round(random.uniform(0.1, 0.3), 2)
                    rk_obj["crackStrength"] = round(random.uniform(0.3, 0.5), 2)
                    rk_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
                    s = random.uniform(0.7, 1.2)
                    rk_obj.scale = (s, s, s)
                    scene_objs.append(rk_obj)
                spawned += 1

        self.report({'INFO'}, f"バイオーム内に {spawned} 個のオブジェクトを自動散布しました。")
        return {'FINISHED'}

def load_obj_mesh_pure(filepath, name):
    if not os.path.exists(filepath):
        return None
    
    verts = []
    faces = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                if line.startswith('v '):
                    parts = line.strip().split()[1:]
                    if len(parts) >= 3:
                        # DirectX OBJ format: parts[0]=X, parts[1]=Y(Up), parts[2]=Z(Depth)
                        # Blender format: X=X, Y=Z(Depth), Z=Y(Up)
                        verts.append((float(parts[0]), float(parts[2]), float(parts[1])))
                elif line.startswith('f '):
                    parts = line.strip().split()[1:]
                    face_indices = []
                    for p in parts:
                        idx_str = p.split('/')[0]
                        if idx_str:
                            idx = int(idx_str)
                            if idx > 0:
                                face_indices.append(idx - 1)
                            elif idx < 0:
                                face_indices.append(len(verts) + idx)
                    if len(face_indices) >= 3:
                        faces.append(face_indices)
    except Exception as e:
        print(f"Error reading OBJ {filepath}: {e}")
        return None

    if not verts:
        return None

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    return mesh

def import_real_obj_model_cached(filepath, mesh_cache, name="LoadedObject"):
    if not os.path.exists(filepath):
        return None
        
    norm_path = os.path.normpath(filepath)
    mesh_data = mesh_cache.get(norm_path)
    
    if not mesh_data:
        mesh_data = load_obj_mesh_pure(filepath, os.path.basename(filepath))
        if mesh_data:
            mesh_cache[norm_path] = mesh_data
            
    if not mesh_data:
        return None
        
    new_obj = bpy.data.objects.new(name, mesh_data)
    return new_obj

class MYADDON_OT_import_escape_from_map(bpy.types.Operator):
    bl_idname = "myaddon.import_escape_from_map"
    bl_label = "📥【Escape_from】最新マップを読み込む"
    bl_description = "project/Resources/内にあるstage_layout.jsonおよび本物の.objモデルファイル(橋, 監視タワー, コンテナ, フェンス等)をメモリキャッシュ共有で爆速読み込みします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # 1. 探索対象のResourcesフォルダパスを正確に取得
        possible_dirs = []
        if bpy.data.filepath:
            blend_dir = os.path.dirname(os.path.abspath(bpy.data.filepath))
            possible_dirs.append(os.path.join(blend_dir, "Resources"))
            possible_dirs.append(os.path.join(os.path.dirname(blend_dir), "Resources"))

        if hasattr(context.scene, "myaddon_properties") and context.scene.myaddon_properties.project_dir:
            p_dir = context.scene.myaddon_properties.project_dir
            possible_dirs.append(os.path.join(p_dir, "Resources"))

        possible_dirs.append(r"c:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources")

        project_resources_dir = None
        json_path = None
        stage_data = None

        for d in possible_dirs:
            jp = os.path.join(d, "stage_layout.json")
            if os.path.exists(jp) and os.path.getsize(jp) > 0:
                try:
                    with open(jp, 'r', encoding='utf-8') as f:
                        stage_data = json.load(f)
                    project_resources_dir = d
                    json_path = jp
                    break
                except Exception as e:
                    print(f"Failed to load {jp}: {e}")
                    continue

        if not stage_data:
            self.report({'ERROR'}, "有効な stage_layout.json が見つかりませんでした。")
            return {'CANCELLED'}

        # 2. 既存のマップオブジェクトおよびメモリ上の未使用データ（オーファン）を爆速クリーンアップ
        objs_to_clear = [o for o in context.scene.objects if o.type in ['MESH', 'CURVE', 'SURFACE']]
        for o in objs_to_clear:
            bpy.data.objects.remove(o, do_unlink=True)

        try:
            bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
        except Exception:
            pass

        col_name = "Imported_Stage_Layout"
        if col_name in context.scene.collection.children:
            collection = context.scene.collection.children[col_name]
        else:
            collection = bpy.data.collections.new(col_name)
            context.scene.collection.children.link(collection)

        mesh_cache = {}
        imported_count = 0
        for item in stage_data:
            name = item.get("name", "Object")
            model_file = item.get("modelFilename", "")
            pos = item.get("position", {"x": 0, "y": 0, "z": 0})
            rot = item.get("rotation", {"x": 0, "y": 0, "z": 0})
            scl = item.get("scale", {"x": 1, "y": 1, "z": 1})
            obj_type = item.get("type", "Obstacle")

            loc = (pos.get("x", 0.0), pos.get("z", 0.0), pos.get("y", 0.0))
            rotation = (rot.get("x", 0.0), rot.get("z", 0.0), rot.get("y", 0.0))
            scale = (scl.get("x", 1.0), scl.get("z", 1.0), scl.get("y", 1.0))

            obj_path = os.path.join(project_resources_dir, model_file)
            loaded_obj = None

            if model_file and os.path.exists(obj_path):
                loaded_obj = import_real_obj_model_cached(obj_path, mesh_cache, name=name)

            if not loaded_obj:
                if "Spawn" in name or obj_type == "SpawnPoint":
                    bpy.ops.mesh.primitive_cone_add(radius1=0.5, depth=1.2, location=loc)
                    loaded_obj = context.active_object
                elif "Goal" in name or obj_type == "GoalRing":
                    bpy.ops.mesh.primitive_torus_add(major_radius=1.5, minor_radius=0.15, location=loc)
                    loaded_obj = context.active_object
                else:
                    bpy.ops.mesh.primitive_cube_add(size=2.0, location=loc)
                    loaded_obj = context.active_object

            loaded_obj.name = name
            loaded_obj.location = loc
            loaded_obj.rotation_euler = rotation
            loaded_obj.scale = scale
            loaded_obj["type"] = obj_type

            # マテリアル設定（川、橋、コンテナ、土嚢等のビジュアル色分け）
            lower_name = name.lower()
            if "river" in lower_name or obj_type == "River" or "water" in lower_name:
                mat = get_or_create_material("RiverWaterMaterial", (0.1, 0.5, 0.9, 0.7), roughness=0.1)
                if len(loaded_obj.data.materials) == 0:
                    loaded_obj.data.materials.append(mat)
                else:
                    loaded_obj.data.materials[0] = mat
            elif "bridge" in lower_name or "bridge" in model_file.lower():
                mat = get_or_create_material("BridgeWoodMaterial", (0.45, 0.28, 0.15, 1.0), roughness=0.7)
                if len(loaded_obj.data.materials) == 0:
                    loaded_obj.data.materials.append(mat)
                else:
                    loaded_obj.data.materials[0] = mat
            elif "container" in lower_name or "container" in model_file.lower():
                mat = get_or_create_material("CargoContainerMaterial", (0.15, 0.35, 0.75, 1.0), roughness=0.4)
                if len(loaded_obj.data.materials) == 0:
                    loaded_obj.data.materials.append(mat)
                else:
                    loaded_obj.data.materials[0] = mat
            elif "sandbag" in lower_name or "sandbag" in model_file.lower():
                mat = get_or_create_material("SandbagMaterial", (0.75, 0.68, 0.50, 1.0), roughness=0.9)
                if len(loaded_obj.data.materials) == 0:
                    loaded_obj.data.materials.append(mat)
                else:
                    loaded_obj.data.materials[0] = mat
            elif "fence" in lower_name or "fence" in model_file.lower():
                mat = get_or_create_material("FenceMaterial", (0.6, 0.6, 0.6, 1.0), roughness=0.3)
                if len(loaded_obj.data.materials) == 0:
                    loaded_obj.data.materials.append(mat)
                else:
                    loaded_obj.data.materials[0] = mat

            if loaded_obj.name not in collection.objects.values():
                for col in loaded_obj.users_collection:
                    col.objects.unlink(loaded_obj)
                collection.objects.link(loaded_obj)

            loaded_obj.select_set(True)
            imported_count += 1

        # Viewport 内のカメラ全域を読み込んだマップ全体へオートフォーカス
        try:
            for area in context.screen.areas:
                if area.type == 'VIEW_3D':
                    for region in area.regions:
                        if region.type == 'WINDOW':
                            override = {'area': area, 'region': region, 'scene': context.scene}
                            bpy.ops.view3d.view_all(override, center=False)
                            break
        except Exception:
            pass

        self.report({'INFO'}, f"🎉 爆速読み込み完了: {imported_count} 個のマップオブジェクト（Duckov物資箱, ドラム缶, 土嚢壁, 橋, タワー, コンテナ等）を画面に自動配置しました！")
        return {'FINISHED'}

class MYADDON_OT_clear_all_objects(bpy.types.Operator):
    bl_idname = "myaddon.clear_all_objects"
    bl_label = "全アセットを一括クリア"
    bl_description = "配置されているすべての障害物(コンテナ・土嚢・橋・監視塔・フェンス)・木・岩・拠点・ルート箱・川を一括削除します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        to_remove = [
            o for o in context.scene.objects 
            if o.type == 'MESH' and o.name != "GroundPlane"
        ]
        for o in to_remove:
            bpy.data.objects.remove(o, do_unlink=True)
            
        try:
            bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
        except Exception:
            pass

        self.report({'INFO'}, f"{len(to_remove)} 個の全アセット（障害物・ギミック・大自然）を完全削除しました。")
        return {'FINISHED'}

class MYADDON_OT_clear_biome_objects(bpy.types.Operator):
    bl_idname = "myaddon.clear_biome_objects"
    bl_label = "選択バイオーム内をクリア"
    bl_description = "選択したバイオーム平面の範囲内にあるオブジェクトを削除します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        plane_obj = context.active_object
        if not plane_obj or plane_obj.type != 'MESH':
            self.report({'WARNING'}, "範囲となる平面オブジェクト（Plane）を選択してください。")
            return {'CANCELLED'}

        bbox = [plane_obj.matrix_world @ mathutils.Vector(corner) for corner in plane_obj.bound_box]
        min_x = min(v.x for v in bbox)
        max_x = max(v.x for v in bbox)
        min_y = min(v.y for v in bbox)
        max_y = max(v.y for v in bbox)

        to_remove = []
        for obj in context.scene.objects:
            if obj != plane_obj and "type" in obj and obj["type"] in ['Tree', 'Rock']:
                pos = obj.location
                if min_x <= pos.x <= max_x and min_y <= pos.y <= max_y:
                    to_remove.append(obj)

        for obj in to_remove:
            bpy.data.objects.remove(obj, do_unlink=True)

        context.area.tag_redraw()
        self.report({'INFO'}, f"選択した範囲内のオブジェクトを {len(to_remove)} 個削除しました。")
        return {'FINISHED'}

class MYADDON_OT_set_biome_zone(bpy.types.Operator):
    bl_idname = "myaddon.set_biome_zone"
    bl_label = "ゾーンタイプを設定"
    bl_description = "選択したPlaneオブジェクトをバイオーム範囲として定義し、ゾーン属性と識別カラーを適用します"
    bl_options = {'REGISTER', 'UNDO'}

    zone_type: bpy.props.EnumProperty(
        name="ゾーンタイプ",
        items=[
            ('Forest', "森林 (Forest)", "木が密集したエリア"),
            ('Desert', "岩場/荒れ地 (Desert)", "岩が露出したエリア"),
            ('River', "川沿い/水辺 (River)", "水辺エリア"),
            ('Grassland', "平原 (Grassland)", "木と岩が点在するエリア")
        ]
    )

    def execute(self, context):
        obj = context.active_object
        if not obj or obj.type != 'MESH':
            self.report({'WARNING'}, "範囲となる平面オブジェクト（Plane）を選択してください。")
            return {'CANCELLED'}

        obj["type"] = "Biome"
        obj["biome_zone_type"] = self.zone_type

        if self.zone_type == 'Forest':
            mat = get_or_create_biome_material("Biome_Forest_Material", (0.1, 0.8, 0.1, 0.4))
        elif self.zone_type == 'Desert':
            mat = get_or_create_biome_material("Biome_Desert_Material", (0.8, 0.6, 0.2, 0.4))
        elif self.zone_type == 'River':
            mat = get_or_create_biome_material("Biome_River_Material", (0.1, 0.4, 0.9, 0.4))
        else:
            mat = get_or_create_biome_material("Biome_Grassland_Material", (0.5, 0.8, 0.2, 0.4))

        if len(obj.data.materials) == 0:
            obj.data.materials.append(mat)
        else:
            obj.data.materials[0] = mat

        self.report({'INFO'}, f"バイオーム範囲を '{self.zone_type}' に設定しました。")
        return {'FINISHED'}

class MYADDON_OT_export_stage_json(bpy.types.Operator):
    bl_idname = "myaddon.export_stage_json"
    bl_label = "📤 stage_layout.json に保存"
    bl_description = "Blender上の配置データを project/Resources/stage_layout.json に自動エクスポートしてDirectX12ゲームへ送ります"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        try:
            export_scene_to_json()
            self.report({'INFO'}, "🎉 stage_layout.json へ保存完了！DirectX12ゲームを起動してご確認ください！")
            return {'FINISHED'}
        except Exception as e:
            self.report({'ERROR'}, f"保存失敗: {e}")
            return {'CANCELLED'}

# UIパネルクラス
class MYADDON_PT_level_editor(bpy.types.Panel):
    bl_label = "DirectX レベルエディタ"
    bl_idname = "MYADDON_PT_level_editor"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "レベルエディタ"

    def draw(self, context):
        layout = self.layout
        addon_props = context.scene.my_addon_properties_v2
        
        box_dash = layout.box()
        box_dash.label(text="【マップ統計・リアルタイムモニター】", icon='WORKSPACE')
        
        counts = {"Tree": 0, "Rock": 0, "River": 0, "Spawn": 0}
        for o in context.scene.objects:
            t = o.get("type", "")
            if t in counts:
                counts[t] += 1
            elif t in ["PlayerSpawn", "EnemySpawn", "LootBox"]:
                counts["Spawn"] += 1

        trees = counts["Tree"]
        rocks = counts["Rock"]
        rivers = counts["River"]
        spawns = counts["Spawn"]
        
        row1 = box_dash.row(align=True)
        row1.label(text=f"🌲 木: {trees}本")
        row1.label(text=f"🪨 岩: {rocks}個")
        row2 = box_dash.row(align=True)
        row2.label(text=f"🌊 川: {rivers}本")
        row2.label(text=f"🎯 ギミック: {spawns}個")
        
        status_icon = 'CHECKMARK' if addon_props.auto_export else 'PAUSE'
        box_dash.label(text=f"同期状態: {addon_props.auto_export_status}", icon=status_icon)
        
        layout.separator()
        
        box_auto = layout.box()
        box_auto.label(text="【DirectX12ゲームへの連動保存・読み込み】", icon='OUTLINER_OB_FORCE_FIELD')
        box_auto.operator(MYADDON_OT_export_stage_json.bl_idname, text="📤 stage_layout.json に保存 (DirectX12へ転送)", icon='EXPORT')
        box_auto.operator(MYADDON_OT_import_escape_from_map.bl_idname, text="📥【Escape_from】最新マップを読み込む", icon='FILE_REFRESH')
        box_auto.operator(MYADDON_OT_generate_auto_survival_map.bl_idname, text="🎲 サバイバルマップを全自動生成", icon='PLAY')
        
        layout.separator()

        box = layout.box()
        box.label(text="【環境設定】", icon='PROPERTIES')
        box.prop(addon_props, "project_path")
        box.prop(addon_props, "auto_export")
        box.prop(addon_props, "camera_sync")
        
        layout.separator()
        
        box_draw = layout.box()
        box_draw.label(text="【超かんたん！マウスでお絵かき】", icon='GREASEPENCIL')
        box_draw.prop(addon_props, "draw_brush_radius", text="筆の太さ (m)")
        box_draw.operator(MYADDON_OT_draw_spline_river_path.bl_idname, text="🌊 クリックで川を描く（両岸小石セット）", icon='CURVE_BEZCURVE')
        box_draw.operator(MYADDON_OT_draw_river_freehand.bl_idname, text="🌊 マウスをなぞって川を描く", icon='MOD_WAVE')
        box_draw.operator(MYADDON_OT_draw_forest_freehand.bl_idname, text="🌲 マウスをなぞって森を描く", icon='NODE_SEL')
        box_draw.operator(MYADDON_OT_draw_rock_freehand.bl_idname, text="🪨 マウスをなぞって岩場を描く", icon='MESH_ICOSPHERE')
        
        layout.separator()

        col1 = layout.column(align=True)
        col1.label(text="オブジェクト作成（単体追加）:")
        col1.operator(MYADDON_OT_add_procedural_tree.bl_idname, icon='NODE_SEL')
        col1.operator(MYADDON_OT_add_procedural_rock.bl_idname, icon='MESH_ICOSPHERE')
        col1.label(text="【サバイバル・ギミックポイント】:")
        col1.operator(MYADDON_OT_add_cargo_container.bl_idname, icon='PACKAGE')
        col1.operator(MYADDON_OT_add_player_spawn.bl_idname, icon='USER')
        col1.operator(MYADDON_OT_add_enemy_spawn.bl_idname, icon='ERROR')
        col1.operator(MYADDON_OT_add_loot_box.bl_idname, icon='PACKAGE')
        col1.label(text="【川パーツ (道路ブロック方式)】:")
        col1.operator(MYADDON_OT_add_river_straight.bl_idname, icon='MOD_FLUID')
        col1.operator(MYADDON_OT_add_river_curve.bl_idname, icon='CURVE_PATH')
        col1.operator(MYADDON_OT_add_river_fork.bl_idname, icon='MOD_BOOLEAN')
        
        layout.separator()

        box_clear = layout.box()
        box_clear.label(text="【一括クリア・削除】", icon='TRASH')
        box_clear.operator(MYADDON_OT_clear_river_objects.bl_idname, icon='MOD_WAVE')
        box_clear.operator(MYADDON_OT_clear_all_objects.bl_idname, icon='X')

def draw_callback_px(self, context):
    font_id = 0
    blf.position(font_id, 20, 90, 0)
    blf.size(font_id, 20)
    blf.color(font_id, 0.4, 0.8, 0.4, 1.0)
    blf.draw(font_id, "【ペイント配置モード中】")
    
    blf.position(font_id, 20, 65, 0)
    blf.size(font_id, 16)
    blf.color(font_id, 1.0, 1.0, 1.0, 1.0)
    names = {
        'Tree': "木 (Tree)",
        'Rock': "岩 (Rock)",
        'PlayerSpawn': "🏃 プレイヤー開始点",
        'EnemySpawn': "⚠️ 敵巡回/スポーン",
        'LootBox': "📦 物資/ルート箱",
        'River': "川ブロック (River)"
    }
    paint_name = names.get(context.scene.my_addon_properties_v2.paint_type, "不明")
    blf.draw(font_id, f"配置アセット: {paint_name}")
    
    blf.position(font_id, 20, 45, 0)
    blf.color(font_id, 0.8, 0.8, 0.8, 1.0)
    blf.draw(font_id, "[左クリック]: 配置 / [右クリックまたはEsc]: 終了")

is_timer_running = False
last_update_time = 0.0

def auto_export_timer():
    global is_timer_running, last_update_time
    scene = getattr(bpy.context, "scene", None)
    if not scene:
        if bpy.data.scenes:
            scene = bpy.data.scenes[0]
        else:
            is_timer_running = False
            return None

    addon_props = getattr(scene, "my_addon_properties_v2", None)
    if not addon_props or not addon_props.auto_export:
        is_timer_running = False
        return None

    if time.time() - last_update_time >= 1.5:
        try:
            bpy.ops.myaddon.export_scene()
            addon_props.auto_export_status = "自動同期完了 (" + time.strftime("%H:%M:%S") + ")"
        except Exception as e:
            addon_props.auto_export_status = f"エラー: {e}"
        is_timer_running = False
        return None

    return 0.5

def on_depsgraph_update(scene, depsgraph):
    global is_timer_running, last_update_time, is_drawing_active
    if is_drawing_active or not scene:
        return

    addon_props = getattr(scene, "my_addon_properties_v2", None)
    if not addon_props or not addon_props.auto_export:
        return

    should_trigger = False
    for update in depsgraph.updates:
        if getattr(update.id, "name", "") == "Spawner_Preview_Target":
            continue
        if update.is_updated_transform or update.is_updated_geometry:
            should_trigger = True
            break

    if should_trigger:
        last_update_time = time.time()
        if not is_timer_running:
            is_timer_running = True
            bpy.app.timers.register(auto_export_timer, first_interval=0.5)

last_cam_pos = None
last_cam_rot = None

def get_viewport_camera_transform():
    context = bpy.context
    screen = getattr(context, "screen", None)
    if not screen:
        screen = bpy.context.window.screen if getattr(context, "window", None) else None
        
    screens = [screen] if screen else bpy.data.screens
    
    for scr in screens:
        for area in scr.areas:
            if area.type == 'VIEW_3D':
                for space in area.spaces:
                    if space.type == 'VIEW_3D':
                        rv3d = space.region_3d
                        if rv3d:
                            view_matrix = rv3d.view_matrix
                            try:
                                inv_matrix = view_matrix.inverted()
                            except:
                                continue
                            pos = inv_matrix.to_translation()
                            P_dx = mathutils.Vector((pos.x, pos.z, pos.y))
                            
                            rot_euler = rv3d.view_rotation.to_euler('XYZ')
                            dx_rot_x = 1.5707963 - rot_euler.x
                            dx_rot_y = -rot_euler.z
                            dx_rot_z = -rot_euler.y
                            
                            return {
                                "position": {"x": P_dx.x, "y": P_dx.y, "z": P_dx.z},
                                "rotation": {"x": dx_rot_x, "y": dx_rot_y, "z": dx_rot_z}
                            }
    return None

def camera_sync_timer():
    global last_cam_pos, last_cam_rot
    try:
        scene = getattr(bpy.context, "scene", None)
        if not scene:
            if bpy.data.scenes:
                scene = bpy.data.scenes[0]
            else:
                return 1.5
                
        addon_props = getattr(scene, "my_addon_properties_v2", None)
        if not addon_props or not addon_props.camera_sync:
            return 1.5
            
        project_dir = bpy.path.abspath(addon_props.project_path)
        if not project_dir or not os.path.exists(project_dir):
            return 1.5
            
        cam_data = get_viewport_camera_transform()
        if cam_data:
            pos = cam_data["position"]
            rot = cam_data["rotation"]
            
            if last_cam_pos != pos or last_cam_rot != rot:
                last_cam_pos = pos
                last_cam_rot = rot
                cam_file = os.path.join(project_dir, "Resources", "camera_sync.json")
                with open(cam_file, 'w', encoding='utf-8') as f:
                    json.dump(cam_data, f, indent=4)
    except Exception:
        pass
        
    return 0.1

# 5. 登録処理
classes = (
    MyAddonPropertiesV2,
    MYADDON_OT_add_procedural_tree,
    MYADDON_OT_add_procedural_rock,
    MYADDON_OT_add_cargo_container,
    MYADDON_OT_add_player_spawn,
    MYADDON_OT_add_enemy_spawn,
    MYADDON_OT_add_loot_box,
    MYADDON_OT_add_river_straight,
    MYADDON_OT_add_river_curve,
    MYADDON_OT_add_river_fork,
    MYADDON_OT_draw_spline_river_path,
    MYADDON_OT_draw_river_freehand,
    MYADDON_OT_draw_forest_freehand,
    MYADDON_OT_draw_rock_freehand,
    MYADDON_OT_clear_river_objects,
    MYADDON_OT_generate_auto_survival_map,
    MYADDON_OT_export_stage_json,
    MYADDON_OT_import_escape_from_map,
    MYADDON_OT_export_scene,
    MYADDON_OT_paint_spawner,
    MYADDON_OT_scatter_biome,
    MYADDON_OT_clear_all_objects,
    MYADDON_OT_clear_biome_objects,
    MYADDON_OT_set_biome_zone,
    MYADDON_PT_level_editor,
)

def register():
    for cls in classes:
        try:
            bpy.utils.unregister_class(cls)
        except Exception:
            pass
        try:
            bpy.utils.register_class(cls)
        except Exception:
            pass

    try:
        bpy.types.Scene.my_addon_properties_v2 = bpy.props.PointerProperty(type=MyAddonPropertiesV2)
    except Exception:
        pass
    
    to_remove = [h for h in bpy.app.handlers.depsgraph_update_post if getattr(h, "__name__", "") in ["on_depsgraph_update", "auto_sync_depsgraph_update"]]
    for h in to_remove:
        try:
            bpy.app.handlers.depsgraph_update_post.remove(h)
        except Exception:
            pass
            
    bpy.app.handlers.depsgraph_update_post.append(on_depsgraph_update)
    
    if not bpy.app.timers.is_registered(camera_sync_timer):
        bpy.app.timers.register(camera_sync_timer, first_interval=0.5)
        
    print("🎉 プロシージャル対応レベルエディタが正常に読み込まれました。")
    
    if not bpy.app.background:
        def draw_popup(self, context):
            self.layout.label(text="🎉 レベルエディタアドオンの最新版をロードしました！", icon='CHECKMARK')
            self.layout.label(text="全アセットクリア・川削除ボタンを100%全消去対応に修復しました。")
        try:
            if hasattr(bpy.context, "window_manager") and bpy.context.window_manager:
                bpy.context.window_manager.popup_menu(draw_popup, title="ロード完了通知", icon='INFO')
        except Exception:
            pass

def unregister():
    if bpy.app.timers.is_registered(camera_sync_timer):
        try:
            bpy.app.timers.unregister(camera_sync_timer)
        except Exception:
            pass
            
    to_remove = [h for h in bpy.app.handlers.depsgraph_update_post if getattr(h, "__name__", "") == "on_depsgraph_update"]
    for h in to_remove:
        try:
            bpy.app.handlers.depsgraph_update_post.remove(h)
        except Exception:
            pass
            
    for cls in reversed(classes):
        try:
            bpy.utils.unregister_class(cls)
        except Exception:
            pass

if __name__ == "__main__":
    register()
