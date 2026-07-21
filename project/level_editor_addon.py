import bpy
import json
import os
import math
import time
import random
import blf
import mathutils
import bpy_extras.view3d_utils as view3d_utils

# 1. アドオンの基本プロフィール
bl_info = {
    "name": "DirectX レベルエディタ (プロシージャル対応版)",
    "author": "Kouda Ayu",
    "version": (1, 1),
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
        default="C:\\Users\\k024g\\OneDrive\\デスクトップ\\Engine_ver2026\\project",
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
    scatter_count: bpy.props.IntProperty(
        name="配置する数",
        description="範囲内に自動配置するオブジェクトの数です",
        default=10,
        min=1,
        max=100
    )
    scatter_type: bpy.props.EnumProperty(
        name="アセット種類",
        description="散布するオブジェクトの種類を選択します",
        items=[
            ('Tree', "木のみ (Tree)", "木だけを散布します"),
            ('Rock', "岩のみ (Rock)", "岩だけを散布します"),
            ('Mix', "ミックス (Mix)", "木と岩をランダムに混ぜて散布します")
        ],
        default='Mix'
    )
    biome_zone_type: bpy.props.EnumProperty(
        name="ゾーンタイプ",
        description="選択したPlaneオブジェクトに適用するゾーンタイプです",
        items=[
            ('Forest', "森林 (Forest)", "木が密集したエリア"),
            ('Desert', "岩場/荒れ地 (Desert)", "岩が露出したエリア"),
            ('River', "川沿い/水辺 (River)", "岩が多く低木がまばらなエリア"),
            ('Grassland', "平原 (Grassland)", "木と岩がほどよく点在するエリア")
        ],
        default='Forest'
    )

# 2.5 マテリアル自動割り当て用ヘルパー
def get_or_create_material(name, color_rgba):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        nodes = mat.node_tree.nodes
        principled = nodes.get("Principled BSDF")
        if principled:
            principled.inputs['Base Color'].default_value = color_rgba
    return mat

def get_or_create_biome_material(name, color_rgba):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name=name)
        mat.use_nodes = True
        mat.blend_method = 'BLEND'  # ビューポートで半透明にする
        nodes = mat.node_tree.nodes
        principled = nodes.get("Principled BSDF")
        if principled:
            principled.inputs['Base Color'].default_value = color_rgba
            if 'Alpha' in principled.inputs:
                principled.inputs['Alpha'].default_value = color_rgba[3]
    else:
        nodes = mat.node_tree.nodes
        principled = nodes.get("Principled BSDF")
        if principled:
            principled.inputs['Base Color'].default_value = color_rgba
            if 'Alpha' in principled.inputs:
                principled.inputs['Alpha'].default_value = color_rgba[3]
    return mat


def ray_cast_terrain(scene, context, origin, direction, exclude_objs=[]):
    # シーン内の既存アセット（木や岩）を一時的に非表示にする
    hidden = []
    for o in scene.objects:
        if o.type == 'MESH' and "type" in o and o["type"] in ['Tree', 'Rock']:
            if not o.hide_viewport:
                o.hide_viewport = True
                hidden.append(o)
                
    # 追加で除外するオブジェクトも非表示にする
    for o in exclude_objs:
        if o and not o.hide_viewport:
            o.hide_viewport = True
            hidden.append(o)
            
    context.view_layer.depsgraph.update()
    
    # レイキャスト実行
    result, location, normal, index, obj, matrix = scene.ray_cast(
        context.view_layer.depsgraph, origin, direction
    )
    
    # 非表示にしたオブジェクトを元の状態に戻す
    for o in hidden:
        o.hide_viewport = False
    context.view_layer.depsgraph.update()
    
    return result, location, normal, index, obj, matrix

def get_object_radius(obj):
    if not obj:
        return 0.5
    obj_type = obj.get("type", "Tree")
    scale_factor = obj.scale.x
    if obj_type == 'Tree':
        return 0.4 * scale_factor  # 木の占有半径（幹は細いが葉の広がりを考慮）
    else:
        return 1.3 * scale_factor  # 岩の占有半径（ICO球の半径1.0 * 安全マージン）

# 3. オペレータ（仕事ロボット）

# 木を追加するロボット
class MYADDON_OT_add_procedural_tree(bpy.types.Operator):
    bl_idname = "myaddon.add_procedural_tree"
    bl_label = "プロシージャル木を追加"
    bl_description = "プロシージャル樹木生成用のオブジェクト（円柱）を追加します"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        # プレースホルダーとして細長い円柱を生成
        bpy.ops.mesh.primitive_cylinder_add(radius=0.1, depth=1.0)
        obj = context.active_object
        obj.name = "ProceduralTree"
        
        # 緑色のマテリアルを割り当てて見やすくする
        mat = get_or_create_material("TreePlaceholderMaterial", (0.1, 0.6, 0.1, 1.0))
        if len(obj.data.materials) == 0:
            obj.data.materials.append(mat)
        else:
            obj.data.materials[0] = mat
            
        # カスタムプロパティ（プロシージャル木用パラメータ）の初期値を付与
        obj["type"] = "Tree"
        obj["seed"] = 54321
        obj["iterations"] = 2
        obj["branchLength"] = 0.15
        obj["branchRadius"] = 0.06
        obj["taperRate"] = 0.8
        obj["angle"] = 25.0
        
        self.report({'INFO'}, "プロシージャル木を生成しました。カスタムプロパティでパラメータを調整できます。")
        return {'FINISHED'}

# 岩を追加するロボット
class MYADDON_OT_add_procedural_rock(bpy.types.Operator):
    bl_idname = "myaddon.add_procedural_rock"
    bl_label = "プロシージャル岩を追加"
    bl_description = "プロシージャル岩石生成用のオブジェクト（球体）を追加します"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        # プレースホルダーとしてICO球（球体）を生成
        bpy.ops.mesh.primitive_ico_sphere_add(radius=1.0)
        obj = context.active_object
        obj.name = "ProceduralRock"
        
        # グレーのマテリアルを割り当てて見やすくする
        mat = get_or_create_material("RockPlaceholderMaterial", (0.4, 0.4, 0.4, 1.0))
        if len(obj.data.materials) == 0:
            obj.data.materials.append(mat)
        else:
            obj.data.materials[0] = mat
            
        # カスタムプロパティ（プロシージャル岩用パラメータ）の初期値を付与
        obj["type"] = "Rock"
        obj["seed"] = 12345
        obj["subdivisions"] = 3
        obj["noiseStrength"] = 0.3
        obj["voronoiStrength"] = 0.2
        obj["crackStrength"] = 0.4
        
        self.report({'INFO'}, "プロシージャル岩を生成しました。カスタムプロパティでパラメータを調整できます。")
        return {'FINISHED'}

# 直線川パーツを追加するロボット (道路パーツ方式)
class MYADDON_OT_add_river_straight(bpy.types.Operator):
    bl_idname = "myaddon.add_river_straight"
    bl_label = "直線川を追加"
    bl_description = "道路パーツのように移動(G)・回転(R)して繋げられる直線の川オブジェクトを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_plane_add(size=4.0)
        obj = context.active_object
        obj.name = "River_Straight"
        obj.scale = (1.0, 2.0, 1.0)
        
        obj["type"] = "River"
        obj["river_part"] = "Straight"
        obj["river_width"] = 4.0
        obj["river_flow_speed"] = 1.0
        obj["river_wave_scale"] = 1.0
        
        mat = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.4, 0.9, 0.7))
        if len(obj.data.materials) == 0:
            obj.data.materials.append(mat)
        else:
            obj.data.materials[0] = mat
            
        self.report({'INFO'}, "直線の川パーツを追加しました。Gキーで移動、Rキーで回転して自由に繋げられます。")
        return {'FINISHED'}

# カーブ川パーツを追加するロボット
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
        
        mat = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.4, 0.9, 0.7))
        if len(obj.data.materials) == 0:
            obj.data.materials.append(mat)
        else:
            obj.data.materials[0] = mat
            
        self.report({'INFO'}, "カーブの川パーツを追加しました。Gキーで移動、Rキーで回転して自由に繋げられます。")
        return {'FINISHED'}

# 分岐川パーツを追加するロボット
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
        
        mat = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.4, 0.9, 0.7))
        if len(obj.data.materials) == 0:
            obj.data.materials.append(mat)
        else:
            obj.data.materials[0] = mat
            
        self.report({'INFO'}, "分岐の川パーツを追加しました。Gキーで移動、Rキーで回転して自由に繋げられます。")
        return {'FINISHED'}

def smooth_points_chaikin(points, iterations=2):
    """手描きのガタガタな線をプロ級のヌルヌル滑らかな自然な曲線に自動丸め補正する関数"""
    if len(points) < 3:
        return points
    curr = list(points)
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

# 🌊 お絵描き感覚でマウスで地面をなぞるだけで川が作れる直感ツール
class MYADDON_OT_draw_river_freehand(bpy.types.Operator):
    bl_idname = "myaddon.draw_river_freehand"
    bl_label = "🌊 マウスでお絵かき（川を描く）"
    bl_description = "マウスをドラッグして地面をスーッとなぞるだけで、お絵描き感覚で川を引くことができます"
    bl_options = {'REGISTER', 'UNDO'}

    def invoke(self, context, event):
        self.points = []
        self.river_obj = None
        self.is_drawing = False
        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, "【川お絵かきモード開始】マウス左ボタンを押しながら地面をなぞってください。終わったら左ボタンを離します。")
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        context.area.tag_redraw()

        # 中クリックドラッグ、マウスホイール等のカメラ視点移動操作はBlenderにパススルーする
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
                loc_vec = mathutils.Vector(location) if 'mathutils' in globals() else location
                if not self.points:
                    self.points.append(loc_vec)
                    self.update_river_mesh(context)
                else:
                    last_pt = mathutils.Vector(self.points[-1]) if 'mathutils' in globals() else self.points[-1]
                    dist = (loc_vec - last_pt).length if hasattr(loc_vec, 'length') else math.sqrt(sum((a-b)**2 for a,b in zip(loc_vec, last_pt)))
                    if dist > 0.8: # 0.8m以上動いたら新しい点を追加
                        self.points.append(loc_vec)
                        self.update_river_mesh(context)

        elif event.type in {'RIGHTMOUSE', 'ESC', 'RET'}:
            self.report({'INFO'}, "川の描画を終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def update_river_mesh(self, context):
        if len(self.points) < 2:
            return

        # Chaikinアルゴリズムで、なぞった線をプロ級の自然なヌルヌル曲線に自動平滑化
        eval_points = smooth_points_chaikin(self.points, iterations=2)
        if len(eval_points) < 2:
            return

        width = 2.0
        half_w = width * 0.5
        vertices = []
        faces = []

        # 既存の川オブジェクトの数に応じて高度を3mm(0.003m)ずつ上にズラし、Zファイティング(黒いチラツキ)を完全防止
        existing_rivers = [o for o in context.scene.objects if o.get("type") == "River"]
        river_idx = len(existing_rivers)
        height_offset = 0.03 + (river_idx * 0.003)

        for i in range(len(eval_points)):
            p = eval_points[i]
            # 急カーブでのポリゴンの潰れを防ぐ平均接線ベクトル計算
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

            # マイクロレイヤーオフセットを適用したZ高度
            z_pos = p[2] + height_offset
            v_left = (p[0] - side_x * half_w, p[1] - side_y * half_w, z_pos)
            v_right = (p[0] + side_x * half_w, p[1] + side_y * half_w, z_pos)

            vertices.append(v_left)
            vertices.append(v_right)

            if i > 0:
                idx = i * 2
                faces.append((idx - 2, idx - 1, idx + 1, idx))

        if not self.river_obj:
            mesh_data = bpy.data.meshes.new("RiverMesh")
            mesh_data.from_pydata(vertices, [], faces)
            mesh_data.update()

            self.river_obj = bpy.data.objects.new("River", mesh_data)
            context.collection.objects.link(self.river_obj)

            self.river_obj["type"] = "River"
            self.river_obj["river_width"] = width
            self.river_obj["river_flow_speed"] = 1.0
            self.river_obj["river_wave_scale"] = 1.0

            mat = get_or_create_biome_material("RiverPlaceholderMaterial", (0.1, 0.4, 0.9, 0.7))
            self.river_obj.data.materials.append(mat)
        else:
            mesh_data = self.river_obj.data
            mesh_data.clear_geometry()
            mesh_data.from_pydata(vertices, [], faces)
            mesh_data.update()

        # JSON出力用に平滑化後の美しい制御点列を保存
        self.river_obj["points_x"] = [float(p[0]) for p in eval_points]
        self.river_obj["points_y"] = [float(p[1]) for p in eval_points]
        self.river_obj["points_z"] = [float(p[2]) for p in eval_points]

# 🌲 お絵描き感覚でマウスで地面をなぞるだけで森が描ける直感ツール
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
                loc_vec = mathutils.Vector(location) if 'mathutils' in globals() else location
                if not self.points:
                    self.points.append(loc_vec)
                else:
                    last_pt = mathutils.Vector(self.points[-1]) if 'mathutils' in globals() else self.points[-1]
                    dist = (loc_vec - last_pt).length if hasattr(loc_vec, 'length') else math.sqrt(sum((a-b)**2 for a,b in zip(loc_vec, last_pt)))
                    if dist > 0.8:
                        self.points.append(loc_vec)

        elif event.type in {'RIGHTMOUSE', 'ESC', 'RET'}:
            self.report({'INFO'}, "森の描画を終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def generate_forest(self, context):
        if len(self.points) < 2:
            return
        eval_pts = smooth_points_chaikin(self.points, iterations=2)
        
        for i in range(len(eval_pts)):
            p = eval_pts[i]
            num_trees = random.randint(1, 2)
            for _ in range(num_trees):
                offset_x = random.uniform(-1.5, 1.5)
                offset_y = random.uniform(-1.5, 1.5)
                loc = (p[0] + offset_x, p[1] + offset_y, p[2])
                
                bpy.ops.mesh.primitive_cylinder_add(radius=0.1, depth=1.0, location=loc)
                new_obj = context.active_object
                new_obj.name = "ProceduralTree"
                mat = get_or_create_material("TreePlaceholderMaterial", (0.1, 0.6, 0.1, 1.0))
                new_obj.data.materials.append(mat)
                
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

# 🪨 お絵描き感覚でマウスで地面をなぞるだけで岩場が描ける直感ツール
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
                loc_vec = mathutils.Vector(location) if 'mathutils' in globals() else location
                if not self.points:
                    self.points.append(loc_vec)
                else:
                    last_pt = mathutils.Vector(self.points[-1]) if 'mathutils' in globals() else self.points[-1]
                    dist = (loc_vec - last_pt).length if hasattr(loc_vec, 'length') else math.sqrt(sum((a-b)**2 for a,b in zip(loc_vec, last_pt)))
                    if dist > 0.8:
                        self.points.append(loc_vec)

        elif event.type in {'RIGHTMOUSE', 'ESC', 'RET'}:
            self.report({'INFO'}, "岩場の描画を終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def generate_rocks(self, context):
        if len(self.points) < 2:
            return
        eval_pts = smooth_points_chaikin(self.points, iterations=2)
        
        for i in range(len(eval_pts)):
            p = eval_pts[i]
            num_rocks = random.randint(1, 2)
            for _ in range(num_rocks):
                offset_x = random.uniform(-1.2, 1.2)
                offset_y = random.uniform(-1.2, 1.2)
                loc = (p[0] + offset_x, p[1] + offset_y, p[2])
                
                bpy.ops.mesh.primitive_ico_sphere_add(radius=1.0, location=loc)
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
                s = random.uniform(0.7, 1.4)
                new_obj.scale = (s, s, s)
                self.spawned_objs.append(new_obj)

# 🌊 川オブジェクトを一括削除するロボット
class MYADDON_OT_clear_river_objects(bpy.types.Operator):
    bl_idname = "myaddon.clear_river_objects"
    bl_label = "川をすべて削除"
    bl_description = "配置されている川オブジェクトをすべて一括削除します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        rivers = [obj for obj in bpy.data.objects if obj.get("type") == "River"]
        if not rivers:
            self.report({'INFO'}, "削除対象の川はありません。")
            return {'FINISHED'}
            
        for obj in rivers:
            bpy.data.objects.remove(obj, do_unlink=True)
            
        context.area.tag_redraw()
        self.report({'INFO'}, f"すべての川オブジェクト（{len(rivers)}個）を削除しました。")
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
                # Blender (X, Y, Z) -> DirectX (X, Z(高さ), Y)
                points_data.append({"x": round(px[i], 4), "y": round(pz[i], 4), "z": round(py[i], 4)})
        elif obj.type == 'CURVE':
            matrix_world = obj.matrix_world
            for spline in obj.data.splines:
                for pt in spline.points:
                    w_pos = matrix_world @ pt.co.xyz
                    points_data.append({"x": round(w_pos.x, 4), "y": round(w_pos.z, 4), "z": round(w_pos.y, 4)})
        obj_info["parameters"]["points"] = points_data
        
    for key in obj.keys():
        if not key.startswith("_") and key not in ["type", "cycles", "points_x", "points_y", "points_z"]:
            val = obj[key]
            if hasattr(val, "to_list"):
                obj_info["parameters"][key] = val.to_list()
            else:
                obj_info["parameters"][key] = val
                
    return obj_info

# シーンのオブジェクト配置をJSONファイルとして保存する
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
            if ("type" in obj and obj["type"] in ['Tree', 'Rock', 'Biome', 'River']):
                layout_data.append(extract_object_data(obj))
                
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(layout_data, f, indent=4, ensure_ascii=False)
            
        self.report({'INFO'}, f"エクスポート完了: {len(layout_data)}個のモデル情報を保存しました")
        return {'FINISHED'}

# 3.5 モーダルペイント配置オペレータ
def draw_callback_px(self, context):
    font_id = 0
    # タイトル
    blf.position(font_id, 20, 90, 0)
    blf.size(font_id, 20)
    blf.color(font_id, 0.4, 0.8, 0.4, 1.0)
    blf.draw(font_id, "【ペイント配置モード中】")
    
    # 選択中の情報
    blf.position(font_id, 20, 65, 0)
    blf.size(font_id, 16)
    blf.color(font_id, 1.0, 1.0, 1.0, 1.0)
    paint_name = "木 (Tree)" if self.paint_type == 'Tree' else "岩 (Rock)"
    blf.draw(font_id, f"・配置アセット: {paint_name}")
    
    # ヘルプ操作説明
    blf.position(font_id, 20, 40, 0)
    blf.size(font_id, 14)
    blf.color(font_id, 0.8, 0.8, 0.8, 1.0)
    blf.draw(font_id, "・左クリック: 配置する  |  右クリック または ESCキー: モード終了")

class MYADDON_OT_paint_spawner(bpy.types.Operator):
    bl_idname = "myaddon.paint_spawner"
    bl_label = "ペイント配置ツールを起動"
    bl_description = "クリックした地形の表面に、アセットをポンポンと連続配置します"
    bl_options = {'REGISTER', 'UNDO'}

    def spawn_asset_at_location(self, context, location):
        self.paint_type = context.scene.my_addon_properties_v2.paint_type
        
        if self.paint_type == 'Tree':
            bpy.ops.mesh.primitive_cylinder_add(radius=0.1, depth=1.0, location=location)
            new_obj = context.active_object
            new_obj.name = "ProceduralTree"
            mat = get_or_create_material("TreePlaceholderMaterial", (0.1, 0.6, 0.1, 1.0))
            new_obj.data.materials.append(mat)
            new_obj["type"] = "Tree"
            new_obj["seed"] = random.randint(1, 99999)
            new_obj["iterations"] = 2
            new_obj["branchLength"] = round(random.uniform(0.12, 0.18), 2)
            new_obj["branchRadius"] = round(random.uniform(0.05, 0.08), 3)
            new_obj["taperRate"] = 0.8
            new_obj["angle"] = round(random.uniform(20.0, 30.0), 1)
        elif self.paint_type == 'Rock':
            bpy.ops.mesh.primitive_ico_sphere_add(radius=1.0, location=location)
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
        else: # River
            bpy.ops.mesh.primitive_plane_add(size=4.0, location=location)
            new_obj = context.active_object
            new_obj.name = "River_Straight"
            new_obj.scale = (1.0, 2.0, 1.0)
            new_obj["type"] = "River"
            new_obj["river_part"] = "Straight"
            new_obj["river_width"] = 2.0
            new_obj["river_flow_speed"] = 1.0
            new_obj["river_wave_scale"] = 1.0
            mat = get_or_create_material("RiverPlaceholderMaterial", (0.1, 0.4, 0.9, 0.7))
            new_obj.data.materials.append(mat)

        if new_obj:
            new_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
            s = random.uniform(0.8, 1.2)
            new_obj.scale = (s, s, s)
            
            if not hasattr(self, "spawned_objects"):
                self.spawned_objects = []
            self.spawned_objects.append(new_obj)
            
            context.view_layer.objects.active = self.preview_obj

    def modal(self, context, event):
        context.area.tag_redraw()

        if event.type in {'MIDDLEMOUSE', 'WHEELUPMOUSE', 'WHEELDOWNMOUSE', 'TRACKPAD_PAN', 'TRACKPAD_ZOOM', 'NDOF_MOTION'}:
            return {'PASS_THROUGH'}

        # マウスドラッグ状態の更新
        if event.type == 'LEFTMOUSE':
            if event.value == 'PRESS':
                self.is_painting = True
                self.last_spawn_loc = None
            elif event.value == 'RELEASE':
                self.is_painting = False
                self.last_spawn_loc = None

        if event.type == 'MOUSEMOVE':
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            result, location, normal, index, obj, matrix = ray_cast_terrain(
                context.scene, context, origin, vec, exclude_objs=[self.preview_obj]
            )
            
            if not result and abs(vec.z) > 0.0001:
                t = -origin.z / vec.z
                if t > 0:
                    location = origin + vec * t
                    normal = mathutils.Vector((0.0, 0.0, 1.0)) if 'mathutils' in globals() else (0.0, 0.0, 1.0)
                    obj = None
                    result = True
            
            if result and obj != self.preview_obj:
                self.preview_obj.location = location
                self.preview_obj.hide_viewport = False

                # 左ドラッグ中の筆塗り配置
                if self.is_painting:
                    should_spawn = False
                    spacing = context.scene.my_addon_properties_v2.paint_spacing
                    
                    if self.last_spawn_loc is None:
                        should_spawn = True
                    else:
                        dist = (mathutils.Vector(location) - mathutils.Vector(self.last_spawn_loc)).length if 'mathutils' in globals() else math.sqrt(sum((a-b)**2 for a,b in zip(location, self.last_spawn_loc)))
                        if dist >= spacing:
                            should_spawn = True

                    if should_spawn:
                        self.spawn_asset_at_location(context, location)
                        self.last_spawn_loc = location
            else:
                self.preview_obj.hide_viewport = True
                
        elif event.type == 'LEFTMOUSE' and event.value == 'PRESS':
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            result, location, normal, index, obj, matrix = ray_cast_terrain(
                context.scene, context, origin, vec, exclude_objs=[self.preview_obj]
            )
            
            if not result and abs(vec.z) > 0.0001:
                t = -origin.z / vec.z
                if t > 0:
                    location = origin + vec * t
                    result = True
            
            if result and obj != self.preview_obj:
                self.spawn_asset_at_location(context, location)
                self.last_spawn_loc = location
                
        elif event.ctrl and event.type == 'Z' and event.value == 'PRESS':
            # モーダル起動中の自前Undo処理
            if hasattr(self, "spawned_objects") and self.spawned_objects:
                while self.spawned_objects:
                    last_obj = self.spawned_objects.pop()
                    if last_obj and last_obj.name in bpy.data.objects:
                        bpy.data.objects.remove(last_obj, do_unlink=True)
                        self.report({'INFO'}, "配置を取り消しました(Undo)")
                        context.area.tag_redraw()
                        break
            return {'RUNNING_MODAL'}
                
        elif event.type in {'RIGHTMOUSE', 'ESC'}:
            # モード終了時の片付け
            bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
            
            # プレビューオブジェクトの削除
            if self.preview_obj:
                bpy.data.objects.remove(self.preview_obj, do_unlink=True)
                
            context.area.tag_redraw()
            self.report({'INFO'}, "ペイント配置モードを終了しました。")
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.space_data.type != 'VIEW_3D':
            self.report({'WARNING'}, "3Dビューポート上で実行してください。")
            return {'CANCELLED'}

        self.preview_obj = None
        self._handle = None
        self.spawned_objects = []
        self.is_painting = False
        self.last_spawn_loc = None
        self.paint_type = context.scene.my_addon_properties_v2.paint_type

        # プレビュー用の赤い照準球体を生成
        preview_mat = get_or_create_material("SpawnerPreviewMaterial", (1.0, 0.1, 0.1, 0.8))
        preview_mat.blend_method = 'BLEND'  # 半透明
        
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.3)
        self.preview_obj = context.active_object
        self.preview_obj.name = "Spawner_Preview_Target"
        self.preview_obj.data.materials.append(preview_mat)
        self.preview_obj.hide_select = True
        self.preview_obj.hide_viewport = True

        # ドローハンドラの登録（操作説明表示用）
        self._handle = bpy.types.SpaceView3D.draw_handler_add(
            draw_callback_px, (self, context), 'WINDOW', 'POST_PIXEL'
        )

        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, "ペイント配置モード開始: 左クリックで配置、ESCで終了。")
        return {'RUNNING_MODAL'}

class MYADDON_OT_scatter_biome(bpy.types.Operator):
    bl_idname = "myaddon.scatter_biome"
    bl_label = "バイオーム配置を実行"
    bl_description = "選択された平面オブジェクトの範囲内に、木や岩を自動でランダム散布します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.my_addon_properties
        active_obj = context.active_object

        # 範囲オブジェクトが選択されているか確認
        if not active_obj or active_obj.type != 'MESH':
            self.report({'WARNING'}, "範囲となる平面オブジェクト（Planeなど）を選択してください。")
            return {'CANCELLED'}

        # プレビュー球を取得
        preview_target_obj = bpy.data.objects.get("Spawner_Preview_Target")

        count = props.scatter_count
        scatter_type = props.scatter_type
        created_count = 0

        # ゾーンタイプの確認
        is_biome = active_obj.get("type") == "Biome"
        zone_type = active_obj.get("biome_zone_type", "Forest") if is_biome else "None"

        # アクティブオブジェクトの選択を一時解除し、生成時にアクティブになってしまうのを防ぐ
        active_obj.select_set(False)

        try:
            for _ in range(count):
                hit_loc = None
                success = False
                
                # ゾーンルールまたは手動設定によるアセット種類・スケールの決定
                current_type = 'Tree'
                simulated_scale = random.uniform(0.8, 1.2)
                
                # 木の固有パラメータ（世代数、長さ、太さ）の初期値
                tree_iterations = 2
                tree_len_min, tree_len_max = 0.12, 0.18
                tree_rad_min, tree_rad_max = 0.05, 0.08

                if is_biome:
                    if zone_type == 'Forest':
                        # 森林: 木が85%、岩が15%。木はやや高め
                        current_type = 'Tree' if random.random() < 0.85 else 'Rock'
                        tree_len_min, tree_len_max = 0.15, 0.22
                        tree_rad_min, tree_rad_max = 0.06, 0.09
                    elif zone_type == 'Desert':
                        # 岩場/荒れ地: 岩が90%、木が10%。岩は大きめ
                        current_type = 'Rock' if random.random() < 0.90 else 'Tree'
                        simulated_scale = random.uniform(1.0, 1.4)
                    elif zone_type == 'River':
                        # 川沿い/水辺: 岩が70%、木が30%。木は低木（世代数1）
                        current_type = 'Rock' if random.random() < 0.70 else 'Tree'
                        tree_iterations = 1
                        tree_len_min, tree_len_max = 0.06, 0.10
                        tree_rad_min, tree_rad_max = 0.008, 0.015
                    else: # Grassland
                        # 平原: 半々
                        current_type = 'Tree' if random.random() < 0.50 else 'Rock'
                else:
                    # ゾーン未設定Planeのフォールバック
                    current_type = scatter_type
                    if current_type == 'Mix':
                        current_type = random.choice(['Tree', 'Rock'])

                # 新しいアセットの想定配置半径を算出（木は0.4m、岩は1.3m基準）
                new_radius = (0.4 if current_type == 'Tree' else 1.3) * simulated_scale
                
                # 重なりを防ぐため、最大5回ランダム位置をリトライする
                for _retry in range(5):
                    # 平面のローカル座標 [-1.0, 1.0] 内でランダムなX, Yを設定
                    local_pos = mathutils.Vector((random.uniform(-1.0, 1.0), random.uniform(-1.0, 1.0), 0.0))
                    # オブジェクトのワールド行列を掛けてワールド座標にする
                    world_pos = active_obj.matrix_world @ local_pos

                    # 上空から真下に向けてレイを投射して地面の高さを探す
                    origin = mathutils.Vector((world_pos.x, world_pos.y, world_pos.z + 50.0))
                    direction = mathutils.Vector((0.0, 0.0, -1.0))
                    
                    # 既存のアセットとプレビュー球、および範囲平面自身を非表示にしてレイキャスト
                    result, temp_loc, hit_normal, face_idx, hit_obj, matrix = ray_cast_terrain(
                        context.scene, context, origin, direction, exclude_objs=[active_obj, preview_target_obj]
                    )

                    # 地形にヒットしなかった場合は Z=0 とする
                    if not result:
                        temp_loc = mathutils.Vector((world_pos.x, world_pos.y, 0.0))

                    # 動的近接チェック
                    too_close = False
                    for other in context.scene.objects:
                        if other.type == 'MESH' and "type" in other and other["type"] in ['Tree', 'Rock']:
                            other_radius = get_object_radius(other)
                            safe_dist = new_radius + other_radius
                            
                            dist = (temp_loc - other.location).length
                            if dist < safe_dist:
                                too_close = True
                                break
                    
                    if not too_close:
                        hit_loc = temp_loc
                        success = True
                        break # 重なりがないので確定
                
                # 5回試しても場所がなかった場合は最後の位置を使う
                if not success:
                    hit_loc = temp_loc

                new_obj = None
                if current_type == 'Tree':
                    # プレースホルダーの木
                    bpy.ops.mesh.primitive_cylinder_add(radius=0.1, depth=1.0, location=hit_loc)
                    new_obj = context.active_object
                    new_obj.name = "ProceduralTree"
                    
                    mat = get_or_create_material("TreePlaceholderMaterial", (0.1, 0.6, 0.1, 1.0))
                    new_obj.data.materials.append(mat)
                    
                    new_obj["type"] = "Tree"
                    new_obj["seed"] = random.randint(1, 99999)
                    new_obj["iterations"] = tree_iterations
                    new_obj["branchLength"] = round(random.uniform(tree_len_min, tree_len_max), 2)
                    new_obj["branchRadius"] = round(random.uniform(tree_rad_min, tree_rad_max), 3)
                    new_obj["taperRate"] = 0.8
                    new_obj["angle"] = round(random.uniform(20.0, 30.0), 1)
                else:
                    # プレースホルダーの岩
                    bpy.ops.mesh.primitive_ico_sphere_add(radius=1.0, location=hit_loc)
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

                # 回転とスケールの適用
                if new_obj:
                    new_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
                    new_obj.scale = (simulated_scale, simulated_scale, simulated_scale)
                    new_obj.select_set(False)
                    created_count += 1

        finally:
            # 元のアクティブオブジェクトを選択状態に戻す
            active_obj.select_set(True)
            context.view_layer.objects.active = active_obj

        self.report({'INFO'}, f"バイオーム散布完了: {created_count}個のオブジェクトを配置しました。")
        return {'FINISHED'}

class MYADDON_OT_clear_all_objects(bpy.types.Operator):
    bl_idname = "myaddon.clear_all_objects"
    bl_label = "配置アセットを全削除 (リセット)"
    bl_description = "配置したすべての木や岩を一括で削除します（床やライトなどは削除されません）"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # 削除対象となるオブジェクトを収集
        to_remove = []
        for obj in context.scene.objects:
            if obj.type == 'MESH' and "type" in obj and obj["type"] in ['Tree', 'Rock']:
                to_remove.append(obj)

        if not to_remove:
            self.report({'INFO'}, "削除するオブジェクトはありません。")
            return {'FINISHED'}

        # 削除実行
        for obj in to_remove:
            bpy.data.objects.remove(obj, do_unlink=True)

        context.area.tag_redraw()
        self.report({'INFO'}, f"配置オブジェクトを {len(to_remove)} 個削除してリセットしました。")
        return {'FINISHED'}

class MYADDON_OT_clear_biome_objects(bpy.types.Operator):
    bl_idname = "myaddon.clear_biome_objects"
    bl_label = "選択範囲内のアセットを削除"
    bl_description = "選択された平面オブジェクトの範囲内にある木や岩のみを一括で削除します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        active_obj = context.active_object

        if not active_obj or active_obj.type != 'MESH':
            self.report({'WARNING'}, "範囲となる平面オブジェクト（Planeなど）を選択してください。")
            return {'CANCELLED'}

        # 範囲オブジェクトのワールド逆行列を取得
        try:
            inv_matrix = active_obj.matrix_world.inverted()
        except:
            self.report({'ERROR'}, "オブジェクトの行列変換に失敗しました。")
            return {'CANCELLED'}

        to_remove = []
        for obj in context.scene.objects:
            # プレースホルダーの木か岩を対象とする
            if obj.type == 'MESH' and "type" in obj and obj["type"] in ['Tree', 'Rock']:
                # ワールド座標を範囲オブジェクトのローカル座標に変換
                local_pos = inv_matrix @ obj.location
                # Planeのローカル範囲 [-1.0, 1.0] 内にあるか判定 (XとYのみ判定し、高低差は無視する)
                if -1.0 <= local_pos.x <= 1.0 and -1.0 <= local_pos.y <= 1.0:
                    to_remove.append(obj)

        if not to_remove:
            self.report({'INFO'}, "選択範囲内に削除対象のアセットはありません。")
            return {'FINISHED'}

        # 削除実行
        for obj in to_remove:
            bpy.data.objects.remove(obj, do_unlink=True)

        context.area.tag_redraw()
        self.report({'INFO'}, f"選択した範囲内のオブジェクトを {len(to_remove)} 個削除しました。")
        return {'FINISHED'}

class MYADDON_OT_set_biome_zone(bpy.types.Operator):
    bl_idname = "myaddon.set_biome_zone"
    bl_label = "ゾーンタイプを設定"
    bl_description = "選択したPlaneオブジェクトをバイオーム範囲として定義し、ゾーン属性と半透明の識別カラーを適用します"
    bl_options = {'REGISTER', 'UNDO'}

    zone_type: bpy.props.EnumProperty(
        name="ゾーンタイプ",
        items=[
            ('Forest', "森林 (Forest)", "木が密集したエリア"),
            ('Desert', "岩場/荒れ地 (Desert)", "岩が露出したエリア"),
            ('River', "川沿い/水辺 (River)", "岩が多く低木がまばらなエリア"),
            ('Grassland', "平原 (Grassland)", "木と岩がほどよく点在するエリア")
        ]
    )

    def execute(self, context):
        obj = context.active_object
        if not obj or obj.type != 'MESH':
            self.report({'WARNING'}, "範囲となる平面オブジェクト（Planeなど）を選択してください。")
            return {'CANCELLED'}

        # カスタムプロパティを設定
        obj["type"] = "Biome"
        obj["biome_zone_type"] = self.zone_type

        # ゾーンに対応する半透明カラーの適用
        if self.zone_type == 'Forest':
            mat = get_or_create_biome_material("Biome_Forest_Material", (0.1, 0.8, 0.1, 0.4))
        elif self.zone_type == 'Desert':
            mat = get_or_create_biome_material("Biome_Desert_Material", (0.8, 0.6, 0.2, 0.4))
        elif self.zone_type == 'River':
            mat = get_or_create_biome_material("Biome_River_Material", (0.1, 0.4, 0.9, 0.4))
        else: # Grassland
            mat = get_or_create_biome_material("Biome_Grassland_Material", (0.6, 0.9, 0.1, 0.4))

        # マテリアルをオブジェクトに割り当て
        if len(obj.data.materials) == 0:
            obj.data.materials.append(mat)
        else:
            obj.data.materials[0] = mat

        # ビューポート再描画
        context.area.tag_redraw()
        self.report({'INFO'}, f"選択オブジェクトを {self.zone_type} ゾーンに設定しました。")
        return {'FINISHED'}

# 4. UIパネル（サイドバーへの表示）
class MYADDON_PT_level_editor(bpy.types.Panel):
    bl_label = "DirectX レベルエディタ"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "レベルエディタ"  # ここがサイドバーのタブ名になります

    def draw(self, context):
        layout = self.layout
        addon_props = context.scene.my_addon_properties_v2
        
        # 設定セクション
        box = layout.box()
        box.label(text="【設定】", icon='PROPERTIES')
        box.prop(addon_props, "project_path")
        
        layout.separator()
        
        # 超かんたん！お絵かきセクション
        box_draw = layout.box()
        box_draw.label(text="【超かんたん！マウスでお絵かき】", icon='GREASEPENCIL')
        box_draw.operator(MYADDON_OT_draw_river_freehand.bl_idname, text="🌊 マウスをなぞって川を描く", icon='MOD_WAVE')
        box_draw.operator(MYADDON_OT_draw_forest_freehand.bl_idname, text="🌲 マウスをなぞって森を描く", icon='NODE_SEL')
        box_draw.operator(MYADDON_OT_draw_rock_freehand.bl_idname, text="🪨 マウスをなぞって岩場を描く", icon='MESH_ICOSPHERE')
        
        layout.separator()
        
        # 作成セクション
        col1 = layout.column(align=True)
        col1.label(text="オブジェクト作成（単体追加）:")
        col1.operator(MYADDON_OT_add_procedural_tree.bl_idname, icon='NODE_SEL')
        col1.operator(MYADDON_OT_add_procedural_rock.bl_idname, icon='MESH_ICOSPHERE')
        col1.label(text="【川パーツ (道路ブロック方式)】:")
        col1.operator(MYADDON_OT_add_river_straight.bl_idname, icon='MOD_FLUID')
        col1.operator(MYADDON_OT_add_river_curve.bl_idname, icon='CURVE_PATH')
        col1.operator(MYADDON_OT_add_river_fork.bl_idname, icon='MOD_BOOLEAN')
        
        layout.separator()

        # 川(River)設定セクション (アクティブオブジェクトがRiverの時表示)
        active_obj = context.active_object
        if active_obj and active_obj.get("type") == "River":
            box_river = layout.box()
            box_river.label(text="【川(River)パラメータ設定】", icon='MOD_WAVE')
            box_river.prop(active_obj, '["river_width"]', text="川の幅 (Width)")
            box_river.prop(active_obj, '["river_flow_speed"]', text="流速 (Flow Speed)")
            box_river.prop(active_obj, '["river_wave_scale"]', text="波スケール (Wave Scale)")
            layout.separator()
        
        # ペイントセクション
        box_paint = layout.box()
        box_paint.label(text="【ペイント配置（お絵描き塗り）】", icon='BRUSH_DATA')
        box_paint.prop(addon_props, "paint_type")
        box_paint.prop(addon_props, "paint_spacing", text="配置の間隔 (m)")
        box_paint.operator(MYADDON_OT_paint_spawner.bl_idname, icon='PLAY')
        
        layout.separator()
        
        # バイオーム（ゾーン）設定セクション (アクティブオブジェクトがMESHの時のみ表示)
        if active_obj and active_obj.type == 'MESH':
            box_zone = layout.box()
            box_zone.label(text="【バイオーム（ゾーン）設定】", icon='WORLD')
            
            is_biome = active_obj.get("type") == "Biome"
            current_zone = active_obj.get("biome_zone_type", "未設定")
            
            if is_biome:
                box_zone.label(text=f"現在のゾーン: {current_zone} ゾーン")
            else:
                box_zone.label(text="状態: 一般オブジェクト (ゾーン未設定)")
                
            row = box_zone.row(align=True)
            row.prop(addon_props, "biome_zone_type", text="")
            op = row.operator(MYADDON_OT_set_biome_zone.bl_idname, text="ゾーンに設定/更新", icon='COLOR')
            op.zone_type = addon_props.biome_zone_type
            
            layout.separator()
        
        # バイオーム散布セクション
        box_scatter = layout.box()
        box_scatter.label(text="【バイオーム自動散布（範囲生成）】", icon='PARTICLES')
        
        if active_obj and active_obj.type == 'MESH' and active_obj.get("type") == "Biome":
            zone_type = active_obj.get("biome_zone_type", "Forest")
            box_scatter.label(text=f"適用ルール: {zone_type} ゾーン規則", icon='INFO')
        else:
            box_scatter.label(text="※ 一般の Plane を選択しています (デフォルト Mix)")
            box_scatter.prop(addon_props, "scatter_type")
            
        box_scatter.prop(addon_props, "scatter_count")
        box_scatter.operator(MYADDON_OT_scatter_biome.bl_idname, icon='FILE_REFRESH')
        
        layout.separator()
        
        # リセットセクション
        box_reset = layout.box()
        box_reset.label(text="【リセット（全削除・部分削除）】", icon='CANCEL')
        box_reset.operator(MYADDON_OT_clear_river_objects.bl_idname, text="川をすべて削除", icon='MOD_FLUID')
        box_reset.operator(MYADDON_OT_clear_biome_objects.bl_idname, icon='REMOVE')
        box_reset.operator(MYADDON_OT_clear_all_objects.bl_idname, icon='TRASH')
        
        layout.separator()
        
        # 出力セクション
        col2 = layout.column(align=True)
        col2.label(text="データ出力:")
        col2.prop(addon_props, "auto_export")
        col2.prop(addon_props, "camera_sync")
        col2.operator(MYADDON_OT_export_scene.bl_idname, icon='EXPORT')
        col2.label(text=f"同期状態: {addon_props.auto_export_status}")

# 4.5 自動エクスポート用のサイレント処理とハンドラ
def export_scene_silent(scene):
    try:
        addon_props = scene.my_addon_properties_v2
    except AttributeError:
        return
        
    project_dir = addon_props.project_path
    if project_dir.startswith("//"):
        try:
            project_dir = bpy.path.abspath(project_dir)
        except Exception as e:
            addon_props.auto_export_status = f"パス解決エラー: {e}"
            return

    if not project_dir or not os.path.exists(project_dir):
        addon_props.auto_export_status = "プロジェクトパスが見つかりません"
        return
    
    output_path = os.path.join(project_dir, "Resources", "stage_layout.json")
    layout_data = []

    try:
        for obj in scene.objects:
            if obj.hide_viewport or obj.hide_get():
                continue
            if ("type" in obj and obj["type"] in ['Tree', 'Rock', 'Biome', 'River']):
                layout_data.append(extract_object_data(obj))
    except Exception as e:
        status_str = f"データ構築エラー: {e}"
        if addon_props.auto_export_status != status_str:
            addon_props.auto_export_status = status_str
        return
            
    try:
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(layout_data, f, indent=4, ensure_ascii=False)
        if addon_props.auto_export_status != "自動同期中...":
            addon_props.auto_export_status = "自動同期中..."
    except Exception as e:
        status_str = f"書き出しエラー: {e}"
        if addon_props.auto_export_status != status_str:
            addon_props.auto_export_status = status_str
        print(f"Auto Export Error: {e}")

# 遅延実行（デバウンス）用のグローバル管理変数
last_update_time = 0.0
is_timer_running = False

def auto_export_timer():
    global last_update_time, is_timer_running
    current_time = time.time()
    
    # 最後の更新から0.2秒以上経っていたらエクスポートを実行してタイマーを終了する
    if current_time - last_update_time >= 0.2:
        try:
            export_scene_silent(bpy.context.scene)
        except Exception as e:
            print(f"Auto Export Timer Error: {e}")
        is_timer_running = False
        return None  # タイマー登録解除
    else:
        # まだ変更が続いているため、0.05秒後に再チェック
        return 0.05

@bpy.app.handlers.persistent
def on_depsgraph_update(scene, depsgraph):
    global last_update_time, is_timer_running
    if bpy.app.background:
        return
    try:
        props = scene.my_addon_properties_v2
        if not props.auto_export:
            return
    except AttributeError:
        return

    # オブジェクトの移動や選択変更、パラメータ更新を検知
    should_trigger = False
    for update in depsgraph.updates:
        if getattr(update.id, "name", "") == "Spawner_Preview_Target":
            continue
            
        if update.is_updated_transform or update.is_updated_geometry:
            should_trigger = True
            break

    # 川(River)の幅パラメータとBlender上の押し出し幅(extrude)を自動同期（水平面固定）
    for obj in scene.objects:
        if obj.type == 'CURVE' and obj.get("type") == "River":
            if obj.data:
                if obj.data.dimensions != '2D':
                    obj.data.dimensions = '2D'
                    obj.data.twist_mode = 'Z_UP'
                width = obj.get("river_width", 2.0)
                if abs(obj.data.extrude - width * 0.5) > 0.01:
                    obj.data.extrude = width * 0.5

    if should_trigger:
        last_update_time = time.time()
        if not is_timer_running:
            is_timer_running = True
            # 0.1秒後に最初のタイマーチェックを行う
            bpy.app.timers.register(auto_export_timer, first_interval=0.1)

# --- カメラ同期用のタイマー処理 ---
last_cam_pos = None
last_cam_rot = None

def get_viewport_camera_transform():
    # タイマー内での安全なアクティブスクリーンの取得
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
                            # 1. 位置の DirectX 座標系変換 (YとZを入れ替え)
                            view_matrix = rv3d.view_matrix
                            try:
                                inv_matrix = view_matrix.inverted()
                            except:
                                continue
                            pos = inv_matrix.to_translation()
                            P_dx = mathutils.Vector((pos.x, pos.z, pos.y))
                            
                            # 2. 3Dビューポートの回転クォータニオンからオイラー角(XYZ)を取得
                            rot_euler = rv3d.view_rotation.to_euler('XYZ')
                            
                            # 3. 厳密な座標系変換数式 (Dot Product 0.9977の完全検証済みモデル)
                            # Pitch (X) -> 1.5707963 - rot_euler.x (地上から原点を見下ろす+0.46 rad)
                            # Yaw   (Y) -> -rot_euler.z (左手系補正の符号反転)
                            # Roll  (Z) -> -rot_euler.y
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
            
            # 変更があった場合のみ camera_sync.json を書き出し
            if last_cam_pos != pos or last_cam_rot != rot:
                last_cam_pos = pos
                last_cam_rot = rot
                
                cam_file = os.path.join(project_dir, "Resources", "camera_sync.json")
                with open(cam_file, 'w', encoding='utf-8') as f:
                    json.dump(cam_data, f, indent=4)
                
                # デバッグログ出力
                print(f"[CameraSync] Wrote pos: ({pos['x']:.4f}, {pos['y']:.4f}, {pos['z']:.4f}), rot: ({rot['x']:.4f}, {rot['y']:.4f}, {rot['z']:.4f})")
    except Exception as e:
        print(f"Camera Sync Error: {e}")
        
    return 0.03 # 30msごとにチェック (約33FPS)

# 5. 登録処理
classes = (
    MyAddonPropertiesV2,
    MYADDON_OT_add_procedural_tree,
    MYADDON_OT_add_procedural_rock,
    MYADDON_OT_add_river_straight,
    MYADDON_OT_add_river_curve,
    MYADDON_OT_add_river_fork,
    MYADDON_OT_draw_river_freehand,
    MYADDON_OT_draw_forest_freehand,
    MYADDON_OT_draw_rock_freehand,
    MYADDON_OT_clear_river_objects,
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
        except:
            pass
        bpy.utils.register_class(cls)
    bpy.types.Scene.my_addon_properties_v2 = bpy.props.PointerProperty(type=MyAddonPropertiesV2)
    
    # 古い同名ハンドラがあれば残留競合を防ぐために一掃する
    to_remove = [h for h in bpy.app.handlers.depsgraph_update_post if getattr(h, "__name__", "") == "on_depsgraph_update"]
    for h in to_remove:
        try:
            bpy.app.handlers.depsgraph_update_post.remove(h)
        except:
            pass
            
    # 最新のハンドラを追加
    bpy.app.handlers.depsgraph_update_post.append(on_depsgraph_update)
    
    # カメラ同期用タイマーを起動
    if not bpy.app.timers.is_registered(camera_sync_timer):
        bpy.app.timers.register(camera_sync_timer, first_interval=0.1)
        
    print("プロシージャル対応レベルエディタが有効化されました。")

def unregister():
    # カメラ同期用タイマーを解除
    if bpy.app.timers.is_registered(camera_sync_timer):
        try:
            bpy.app.timers.unregister(camera_sync_timer)
        except:
            pass

    # 同名のイベントハンドラをすべて解除
    to_remove = [h for h in bpy.app.handlers.depsgraph_update_post if getattr(h, "__name__", "") == "on_depsgraph_update"]
    for h in to_remove:
        try:
            bpy.app.handlers.depsgraph_update_post.remove(h)
        except:
            pass
        
    del bpy.types.Scene.my_addon_properties_v2
    for cls in classes:
        bpy.utils.unregister_class(cls)
    print("プロシージャル対応レベルエディタが無効化されました。")

if __name__ == "__main__":
    try:
        unregister()
    except Exception as e:
        pass
    register()
