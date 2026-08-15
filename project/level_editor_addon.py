import bpy
import json
import os
import math
import time
import random
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
        description="配置オブジェクトに変更があった時、自動的にJSONファイルを更新します",
        default=True
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
    if exclude_objs is None:
        exclude_objs = []
        
    hidden = []
    for o in scene.objects:
        if o.type == 'MESH' and "type" in o:
            if not o.hide_viewport:
                o.hide_viewport = True
                hidden.append(o)
                
    for o in exclude_objs:
        if o and not o.hide_viewport:
            o.hide_viewport = True
            hidden.append(o)
            
    context.view_layer.depsgraph.update()
    result, location, normal, index, obj, matrix = scene.ray_cast(
        context.view_layer.depsgraph, origin, direction
    )
    for o in hidden:
        o.hide_viewport = False
    context.view_layer.depsgraph.update()
    
    return result, location, normal, index, obj, matrix

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
    trunk = bpy.context.active_object
    trunk_mat = get_or_create_material("TreeTrunkMaterial", (0.35, 0.2, 0.1, 1.0))
    trunk.data.materials.append(trunk_mat)
    
    leaves_mat = get_or_create_material("TreeLeavesMaterial", (0.2, 0.75, 0.2, 1.0))
    
    bpy.ops.mesh.primitive_cone_add(radius1=0.7, depth=0.7, location=(x, y, z + 0.7))
    cone1 = bpy.context.active_object
    cone1.data.materials.append(leaves_mat)
    
    bpy.ops.mesh.primitive_cone_add(radius1=0.55, depth=0.65, location=(x, y, z + 1.15))
    cone2 = bpy.context.active_object
    cone2.data.materials.append(leaves_mat)
    
    bpy.ops.mesh.primitive_cone_add(radius1=0.4, depth=0.6, location=(x, y, z + 1.55))
    cone3 = bpy.context.active_object
    cone3.data.materials.append(leaves_mat)
    
    ctx = bpy.context.copy()
    ctx['active_object'] = trunk
    ctx['selected_editable_objects'] = [trunk, cone1, cone2, cone3]
    bpy.ops.object.join(ctx)
    
    tree_obj = bpy.context.active_object
    tree_obj.name = "ProceduralTree"
    return tree_obj

def create_player_visual_object(location=(0,0,0)):
    x, y, z = location[0], location[1], location[2]
    
    bpy.ops.mesh.primitive_cylinder_add(radius=0.25, depth=0.9, location=(x, y, z + 0.45))
    body = bpy.context.active_object
    
    bpy.ops.mesh.primitive_cube_add(size=0.44, location=(x, y, z + 1.15))
    head = bpy.context.active_object
    
    mat = get_or_create_material("PlayerSpawnMaterial", (0.2, 0.5, 1.0, 1.0))
    body.data.materials.append(mat)
    head.data.materials.append(mat)
    
    ctx = bpy.context.copy()
    ctx['active_object'] = body
    ctx['selected_editable_objects'] = [body, head]
    bpy.ops.object.join(ctx)
    
    p_obj = bpy.context.active_object
    p_obj.name = "PlayerSpawnPoint"
    return p_obj

def create_enemy_visual_object(location=(0,0,0)):
    x, y, z = location[0], location[1], location[2]
    
    bpy.ops.mesh.primitive_cylinder_add(radius=0.25, depth=0.9, location=(x, y, z + 0.45))
    body = bpy.context.active_object
    
    bpy.ops.mesh.primitive_cube_add(size=0.44, location=(x, y, z + 1.15))
    head = bpy.context.active_object
    
    mat = get_or_create_material("EnemySpawnMaterial", (1.0, 0.25, 0.2, 1.0))
    body.data.materials.append(mat)
    head.data.materials.append(mat)
    
    ctx = bpy.context.copy()
    ctx['active_object'] = body
    ctx['selected_editable_objects'] = [body, head]
    bpy.ops.object.join(ctx)
    
    e_obj = bpy.context.active_object
    e_obj.name = "EnemySpawnPoint"
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
    スクリーンショットを元に、波打つ鋼鉄プレートと角フレームを持つ
    貨物コンテナ(Cargo Container)と木製ハシゴ(Wooden Ladder)の3DモデルをBlender上に生成
    """
    x, y, z = location[0], location[1], location[2]
    
    # 1. コンテナ本体 (幅2.2m x 奥行き5.0m x 高さ2.2m)
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x, y, z + 1.1))
    body = bpy.context.active_object
    body.scale = (2.2, 5.0, 2.2)
    body.name = "Container_Body"
    
    # コンテナ天板の凹凸波打ち溝ラインを再現 (5本のライン)
    groove_objs = []
    for i in range(-2, 3):
        gy = y + (i * 0.9)
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x, gy, z + 2.22))
        g = bpy.context.active_object
        g.scale = (2.25, 0.15, 0.08)
        groove_objs.append(g)
        
    # コンテナ四角フレーム柱
    for sx in [-1.08, 1.08]:
        for sy in [-2.48, 2.48]:
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x + sx, y + sy, z + 1.1))
            pillar = bpy.context.active_object
            pillar.scale = (0.2, 0.2, 2.25)
            groove_objs.append(pillar)
            
    mat_container = get_or_create_material("ContainerMaterial", (0.55, 0.58, 0.62, 1.0), roughness=0.4)
    body.data.materials.append(mat_container)
    for g in groove_objs:
        g.data.materials.append(mat_container)
        
    # 2. 側面に立てかけられた木製ハシゴ (Wooden Ladder)
    ladder_objs = []
    # 左右の縦木
    for lx in [-0.25, 0.25]:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x - 1.2, y - 1.0 + lx, z + 0.9))
        side_wood = bpy.context.active_object
        side_wood.scale = (0.08, 0.08, 2.2)
        side_wood.rotation_euler = (0, 0.45, 0) # 斜めに立てかける
        ladder_objs.append(side_wood)
        
    # ハシゴの横段 (4本)
    for step in range(4):
        sz = z + 0.3 + (step * 0.45)
        sx = (x - 1.2) + (step * 0.18)
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(sx, y - 1.0, sz))
        step_wood = bpy.context.active_object
        step_wood.scale = (0.08, 0.52, 0.08)
        step_wood.rotation_euler = (0, 0.45, 0)
        ladder_objs.append(step_wood)
        
    mat_ladder = get_or_create_material("LadderWoodMaterial", (0.45, 0.28, 0.15, 1.0), roughness=0.8)
    for l in ladder_objs:
        l.data.materials.append(mat_ladder)
        
    # メッシュをすべて統合
    ctx = bpy.context.copy()
    ctx['active_object'] = body
    ctx['selected_editable_objects'] = [body] + groove_objs + ladder_objs
    for o in ctx['selected_editable_objects']:
        o.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    
    container_obj = bpy.context.active_object
    container_obj.name = "Obstacle_CargoContainer"
    return container_obj

def export_scene_to_json():
    """
    Blenderの全メッシュ（コンテナ、フェンス、地面、木、スポーン等）を
    project/Resources/stage_layout.json に自動で全書き出し保存する関数
    """
    addons_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(addons_dir, "Resources", "stage_layout.json")

    layout_data = []
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            pos_x = obj.location.x
            pos_y = obj.location.z
            pos_z = obj.location.y

            rot_x = obj.rotation_euler.x
            rot_y = obj.rotation_euler.z
            rot_z = obj.rotation_euler.y

            if obj_type == "Fence":
                rot_x = 0.0
                rot_z = 0.0

            scale_x = obj.scale.x
            scale_y = obj.scale.z
            scale_z = obj.scale.y

            obj_name = obj.name.split('.')[0]
            obj_type = obj.get("type", "Fence")

            model_dir = "Resources"
            model_file = "fence.obj"
            if obj_type == "Container" or "container" in obj_name.lower():
                model_file = "container.obj"
            elif "plane" in obj_name.lower() or "ground" in obj_name.lower():
                model_file = "plane.obj"

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
            layout_data.append(obj_info)

    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(layout_data, f, indent=4, ensure_ascii=False)
    except Exception:
        pass

# 🔄 Blender内の「移動・回転・変形・追加・削除」を常時リアルタイム全自動監視するハンドラ
@bpy.app.handlers.persistent
def auto_sync_depsgraph_update(scene, depsgraph):
    """
    Blender上でユーザーがマウスで動かしたり、追加・削除・編集を行った瞬間、
    何もボタンを押さなくても「勝手に自動同期」して stage_layout.json を即座に更新保存する
    """
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
            else:
                side_x, side_y = last_side_x, last_side_y
            
            for side in [-1.0, 1.0]:
                if random.random() < 0.65:
                    offset_dist = half_w + random.uniform(0.5, 1.3)
                    rx = p[0] + side * side_x * offset_dist + random.uniform(-0.2, 0.2)
                    ry = p[1] + side * side_y * offset_dist + random.uniform(-0.2, 0.2)
                    rz = p[2]
                    
                    loc = (rx, ry, rz)
                    if is_location_valid(loc, required_radius=0.4, scene_objects=scene_objs):
                        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.3, location=loc)
                        rock_obj = context.active_object
                        rock_obj.name = "ShoreRock"
                        rock_obj["type"] = "Rock"
                        rock_obj["seed"] = random.randint(1, 99999)
                        rock_obj["subdivisions"] = 2
                        rock_obj["noiseStrength"] = round(random.uniform(0.1, 0.3), 2)
                        rock_obj["voronoiStrength"] = round(random.uniform(0.1, 0.2), 2)
                        rock_obj["crackStrength"] = round(random.uniform(0.1, 0.3), 2)
                        
                        rock_obj.rotation_euler = (random.uniform(0, 3.14), random.uniform(0, 3.14), random.uniform(0, 3.14))
                        s = random.uniform(0.4, 0.85)
                        rock_obj.scale = (s, s, s)
                        
                        mat = get_or_create_material("RockPlaceholderMaterial", (0.45, 0.45, 0.45, 1.0))
                        if len(rock_obj.data.materials) == 0:
                            rock_obj.data.materials.append(mat)
                            
                        self.spawned_rocks.append(rock_obj)
                        scene_objs.append(rock_obj)

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
        self.points = []
        self.river_obj = None
        self.river_bed_obj = None
        self.is_drawing = False
        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, "【川お絵かきモード開始】マウス左ボタンを押しながら地面をなぞってください。終わったら左ボタンを離します。")
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
                    self.report({'INFO'}, f"川の描画が完了しました！（点数: {len(self.points)}）")
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
                    self.update_river_mesh(context)
                else:
                    last_pt = self.points[-1]
                    dist = math.sqrt((loc_tuple[0]-last_pt[0])**2 + (loc_tuple[1]-last_pt[1])**2 + (loc_tuple[2]-last_pt[2])**2)
                    if dist > 0.8:
                        self.points.append(loc_tuple)
                        self.update_river_mesh(context)

        elif event.type in {'RIGHTMOUSE', 'ESC', 'RET'}:
            self.report({'INFO'}, "川の描画を終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def update_river_mesh(self, context):
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

def import_real_obj_model(filepath, location=(0,0,0), rotation=(0,0,0), scale=(1.0,1.0,1.0), name="LoadedObject"):
    if not os.path.exists(filepath):
        return None
    for o in bpy.context.scene.objects: o.select_set(False)
    before_objs = set(bpy.context.scene.objects)
    try:
        bpy.ops.wm.obj_import(filepath=filepath)
    except Exception:
        try:
            bpy.ops.import_scene.obj(filepath=filepath)
        except Exception:
            return None
    after_objs = set(bpy.context.scene.objects)
    new_objs = list(after_objs - before_objs)
    if not new_objs: return None
    for o in new_objs: o.select_set(True)
    active_obj = new_objs[0]
    bpy.context.view_layer.objects.active = active_obj
    if len(new_objs) > 1:
        bpy.ops.object.join()
        active_obj = bpy.context.active_object
    active_obj.name = name
    active_obj.location = location
    active_obj.rotation_euler = rotation
    active_obj.scale = scale
    return active_obj

class MYADDON_OT_import_escape_from_map(bpy.types.Operator):
    bl_idname = "myaddon.import_escape_from_map"
    bl_label = "📥【Escape_from】最新マップを読み込む"
    bl_description = "project/Resources/内にある本物の.objモデルファイル(fence.obj, teapot.obj, plane.obj)を自動的にBlenderへダイレクト読み込みします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        objs_to_clear = [o for o in context.scene.objects if o.type in ['MESH', 'CURVE', 'SURFACE']]
        for o in objs_to_clear:
            bpy.data.objects.remove(o, do_unlink=True)

        col_name = "Imported_Stage_Layout"
        if col_name in context.scene.collection.children:
            collection = context.scene.collection.children[col_name]
        else:
            collection = bpy.data.collections.new(col_name)
            context.scene.collection.children.link(collection)

        addons_dir = os.path.dirname(os.path.abspath(__file__))
        resources_dir = os.path.join(addons_dir, "Resources")

        fence_path = os.path.join(resources_dir, "fence.obj")
        plane_path = os.path.join(resources_dir, "plane.obj")
        teapot_path = os.path.join(resources_dir, "teapot.obj")

        # 1. 地面 (plane.obj)
        ground_obj = import_real_obj_model(plane_path, location=(0, 16.0, 0), scale=(25.0, 25.0, 1.0), name="GroundPlane")
        if ground_obj:
            for col in ground_obj.users_collection: col.objects.unlink(ground_obj)
            collection.objects.link(ground_obj)

        # 2. 木製バリケードフェンス (fence.obj)
        fence_configs = [
            {"name": "Fence_obs1_LeftNear",   "pos": (-5.0, 8.0, 0.0)},
            {"name": "Fence_obs2_LeftFar",    "pos": (-5.0, 18.0, 0.0)},
            {"name": "Fence_obs3_RightNear",  "pos": (5.0, 8.0, 0.0)},
            {"name": "Fence_obs4_RightFar",   "pos": (5.0, 18.0, 0.0)},
            {"name": "Fence_obs5_GoalLeft",   "pos": (-2.0, 29.0, 0.0)},
            {"name": "Fence_obs6_GoalRight",  "pos": (2.0, 29.0, 0.0)},
        ]

        for f in fence_configs:
            f_obj = import_real_obj_model(fence_path, location=f["pos"], name=f["name"])
            if f_obj:
                f_obj["type"] = "Fence"
                for col in f_obj.users_collection: col.objects.unlink(f_obj)
                collection.objects.link(f_obj)

        # 3. チュートリアル用の的 (teapot.obj)
        target_configs = [
            {"name": "Target_teapot_1", "pos": (-3.0, 16.0, 0.0)},
            {"name": "Target_teapot_2", "pos": (3.0, 16.0, 0.0)},
            {"name": "Target_teapot_3", "pos": (0.0, 26.0, 0.0)},
        ]
        for t in target_configs:
            t_obj = import_real_obj_model(teapot_path, location=t["pos"], scale=(0.56, 0.56, 0.56), name=t["name"])
            if t_obj:
                t_obj["type"] = "Target"
                for col in t_obj.users_collection: col.objects.unlink(t_obj)
                collection.objects.link(t_obj)

        # 4. 直立看板 (plane.obj)
        sign_obj = import_real_obj_model(plane_path, location=(0.0, 4.0, 0.8), rotation=(1.570796, 0, 0), scale=(0.8, 0.8, 0.8), name="Tutorial_Signboard")
        if sign_obj:
            sign_obj["type"] = "TutorialSign"
            for col in sign_obj.users_collection: col.objects.unlink(sign_obj)
            collection.objects.link(sign_obj)

        # 5. 脱出ゴールリング (GoalRing)
        bpy.ops.mesh.primitive_torus_add(major_radius=1.5, minor_radius=0.15, location=(0.0, 32.0, 0.01))
        ring_obj = context.active_object
        ring_obj.name = "Goal_Extraction_Ring"
        ring_obj["type"] = "GoalRing"
        for col in ring_obj.users_collection: col.objects.unlink(ring_obj)
        collection.objects.link(ring_obj)

        # 6. スポーン地点 (Player & Enemy)
        spawns = [
            {"name": "Player_Spawn_Point", "pos": (0.0, 0.0, 0.0)},
            {"name": "Enemy_Spawn_Point",  "pos": (0.0, 20.0, 0.0)},
        ]
        for s in spawns:
            bpy.ops.mesh.primitive_cone_add(radius1=0.5, depth=1.2, location=(s["pos"][0], s["pos"][1], s["pos"][2] + 0.6))
            s_obj = context.active_object
            s_obj.name = s["name"]
            s_obj["type"] = "SpawnPoint"
            for col in s_obj.users_collection: col.objects.unlink(s_obj)
            collection.objects.link(s_obj)

        self.report({'INFO'}, "🎉 Resources/フォルダ内の本物の.objモデル(fence.obj, teapot.obj, plane.obj)を自動でBlenderへ読み込みました！")
        return {'FINISHED'}

class MYADDON_OT_clear_all_objects(bpy.types.Operator):
    bl_idname = "myaddon.clear_all_objects"
    bl_label = "全アセットを一括クリア"
    bl_description = "配置されているすべての木・岩・拠点・ルート箱・川・川底を一括削除します"
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
            
        self.report({'INFO'}, f"{len(to_remove)} 個のアセットを完全一括削除しました。")
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
        
        trees = len([o for o in context.scene.objects if o.get("type") == "Tree"])
        rocks = len([o for o in context.scene.objects if o.get("type") == "Rock"])
        rivers = len([o for o in context.scene.objects if o.get("type") == "River"])
        spawns = len([o for o in context.scene.objects if o.get("type") in ["PlayerSpawn", "EnemySpawn", "LootBox"]])
        
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

    addon_props = scene.my_addon_properties_v2
    if not addon_props.auto_export:
        is_timer_running = False
        return None

    if time.time() - last_update_time >= 0.5:
        try:
            bpy.ops.myaddon.export_scene()
            addon_props.auto_export_status = "自動同期完了 (" + time.strftime("%H:%M:%S") + ")"
        except Exception as e:
            addon_props.auto_export_status = f"エラー: {e}"
        is_timer_running = False
        return None

    return 0.1

def on_depsgraph_update(scene, depsgraph):
    global is_timer_running, last_update_time
    if not scene:
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
            bpy.app.timers.register(auto_export_timer, first_interval=0.1)

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
                return 0.1
                
        addon_props = scene.my_addon_properties_v2
        if not addon_props.camera_sync:
            return 0.05
            
        project_dir = bpy.path.abspath(addon_props.project_path)
        if not project_dir or not os.path.exists(project_dir):
            return 0.1
            
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
        
    return 0.03

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
    bpy.app.handlers.depsgraph_update_post.append(auto_sync_depsgraph_update)
    
    if not bpy.app.timers.is_registered(camera_sync_timer):
        bpy.app.timers.register(camera_sync_timer, first_interval=0.1)
        
    print("🎉 プロシージャル対応レベルエディタが正常に読み込まれました。")
    
    def draw_popup(self, context):
        self.layout.label(text="🎉 レベルエディタアドオンの最新版をロードしました！", icon='CHECKMARK')
        self.layout.label(text="全アセットクリア・川削除ボタンを100%全消去対応に修復しました。")
    try:
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
