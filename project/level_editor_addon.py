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
class MyAddonProperties(bpy.types.PropertyGroup):
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
    auto_export_status: bpy.props.StringProperty(
        name="同期状態",
        default="準備完了"
    )
    paint_type: bpy.props.EnumProperty(
        name="配置アセット",
        description="ペイント配置するアセットの種類を選択します",
        items=[
            ('Tree', "木 (Tree)", "プロシージャル樹木を配置します"),
            ('Rock', "岩 (Rock)", "プロシージャル岩石を配置します")
        ],
        default='Tree'
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

# シーンのオブジェクト配置をJSONファイルとして保存する
class MYADDON_OT_export_scene(bpy.types.Operator):
    bl_idname = "myaddon.export_scene"
    bl_label = "JSONファイルに出力"
    bl_description = "配置データをゲームのResourcesフォルダにJSONとして出力します"

    def execute(self, context):
        addon_props = context.scene.my_addon_properties
        project_dir = bpy.path.abspath(addon_props.project_path)
        
        if not project_dir or not os.path.exists(project_dir):
            self.report({'ERROR'}, "有効なプロジェクトパスを選択してください。")
            return {'CANCELLED'}
        
        output_path = os.path.join(project_dir, "Resources", "stage_layout.json")
        layout_data = []

        for obj in bpy.context.scene.objects:
            # メッシュオブジェクトかつ、typeプロパティ（"Tree" or "Rock"）が設定されているものを対象にする
            if obj.type == 'MESH' and "type" in obj:
                obj_type = obj["type"]
                
                # --- 座標の変換 (Blender:Z-up右手系 -> DirectX:Y-up左手系) ---
                pos_x = obj.location.x
                pos_y = obj.location.z
                pos_z = obj.location.y
                
                rot_x = obj.rotation_euler.x
                rot_y = obj.rotation_euler.z  # Z軸回転 -> DirectXのY軸回転
                rot_z = obj.rotation_euler.y  # Y軸回転 -> DirectXのZ軸回転
                
                scale_x = obj.scale.x
                scale_y = obj.scale.z
                scale_z = obj.scale.y
                
                obj_name = obj.name.split('.')[0]
                
                obj_info = {
                    "name": obj_name,
                    "type": obj_type,
                    "position": {"x": round(pos_x, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
                    "rotation": {"x": round(rot_x, 4), "y": round(rot_y, 4), "z": round(rot_z, 4)},
                    "scale":    {"x": round(scale_x, 4), "y": round(scale_y, 4), "z": round(scale_z, 4)},
                    "parameters": {}
                }
                
                # "type" 以外のすべてのカスタムプロパティを parameters に詰める
                for key in obj.keys():
                    # bpy の内部用プロパティを除外
                    if not key.startswith("_") and key not in ["type", "cycles"]:
                        # JSONシリアライズ可能な型に変換して追加
                        val = obj[key]
                        # BlenderのIDPropertyから純粋な型に変換
                        if hasattr(val, "to_list"):
                            obj_info["parameters"][key] = val.to_list()
                        else:
                            obj_info["parameters"][key] = val
                            
                layout_data.append(obj_info)
                
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



    def modal(self, context, event):
        context.area.tag_redraw()

        # マウスイベント処理
        if event.type == 'MOUSEMOVE':
            # レイキャストによる座標取得
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            # ヘルパーを使用して既存の木・岩とプレビュー球を除外してレイキャスト
            result, location, normal, index, obj, matrix = ray_cast_terrain(
                context.scene, context, origin, vec, exclude_objs=[self.preview_obj]
            )
            
            # 地形に衝突しなかった場合、Z=0(無限グリッド床)との交点を計算する
            if not result:
                if abs(vec.z) > 0.0001:
                    t = -origin.z / vec.z
                    if t > 0:
                        location = origin + vec * t
                        normal = mathutils.Vector((0.0, 0.0, 1.0)) if 'mathutils' in globals() else (0.0, 0.0, 1.0)
                        obj = None
                        result = True
            
            # 赤いプレビュー照準の移動と再表示
            if result and obj != self.preview_obj:
                self.preview_obj.location = location
                self.preview_obj.hide_viewport = False
            else:
                self.preview_obj.hide_viewport = True
                
        elif event.type == 'LEFTMOUSE' and event.value == 'PRESS':
            # 配置処理
            coord = (event.mouse_region_x, event.mouse_region_y)
            region = context.region
            rv3d = context.space_data.region_3d
            
            vec = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
            origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
            
            # ヘルパーを使用して既存の木・岩とプレビュー球を除外してレイキャスト
            result, location, normal, index, obj, matrix = ray_cast_terrain(
                context.scene, context, origin, vec, exclude_objs=[self.preview_obj]
            )
            
            # 地形に衝突しなかった場合、Z=0(無限グリッド床)との交点を計算する
            if not result:
                if abs(vec.z) > 0.0001:
                    t = -origin.z / vec.z
                    if t > 0:
                        location = origin + vec * t
                        normal = (0.0, 0.0, 1.0)
                        obj = None
                        result = True
            
            if result and obj != self.preview_obj:
                # ユーザーの選択アセット種類を随時プロパティから取得
                self.paint_type = context.scene.my_addon_properties.paint_type
                
                # 新しいオブジェクトの生成とランダム化
                if self.paint_type == 'Tree':
                    # プレースホルダーの木
                    bpy.ops.mesh.primitive_cylinder_add(radius=0.1, depth=1.0, location=location)
                    new_obj = context.active_object
                    new_obj.name = "ProceduralTree"
                    
                    # 緑色マテリアルの割り当て
                    mat = get_or_create_material("TreePlaceholderMaterial", (0.1, 0.6, 0.1, 1.0))
                    new_obj.data.materials.append(mat)
                    
                    # ランダムパラメータ
                    new_obj["type"] = "Tree"
                    new_obj["seed"] = random.randint(1, 99999)
                    new_obj["iterations"] = 2
                    new_obj["branchLength"] = round(random.uniform(0.12, 0.18), 2)
                    new_obj["branchRadius"] = round(random.uniform(0.05, 0.08), 3)
                    new_obj["taperRate"] = 0.8
                    new_obj["angle"] = round(random.uniform(20.0, 30.0), 1)
                else:
                    # プレースホルダーの岩
                    bpy.ops.mesh.primitive_ico_sphere_add(radius=1.0, location=location)
                    new_obj = context.active_object
                    new_obj.name = "ProceduralRock"
                    
                    # グレーマテリアルの割り当て
                    mat = get_or_create_material("RockPlaceholderMaterial", (0.4, 0.4, 0.4, 1.0))
                    new_obj.data.materials.append(mat)
                    
                    # ランダムパラメータ
                    new_obj["type"] = "Rock"
                    new_obj["seed"] = random.randint(1, 99999)
                    new_obj["subdivisions"] = 3
                    new_obj["noiseStrength"] = round(random.uniform(0.2, 0.4), 2)
                    new_obj["voronoiStrength"] = round(random.uniform(0.1, 0.3), 2)
                    new_obj["crackStrength"] = round(random.uniform(0.3, 0.5), 2)
                
                # Z軸まわりのランダム回転、ランダムスケール
                new_obj.rotation_euler.z = random.uniform(0, math.pi * 2)
                s = random.uniform(0.8, 1.2)
                new_obj.scale = (s, s, s)
                
                # 配置したオブジェクトをUndo用に追跡リストに記録
                if not hasattr(self, "spawned_objects"):
                    self.spawned_objects = []
                self.spawned_objects.append(new_obj)
                
                # プレビューオブジェクトを再びアクティブにする
                context.view_layer.objects.active = self.preview_obj
                
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
        self.paint_type = context.scene.my_addon_properties.paint_type

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

        # アクティブオブジェクトの選択を一時解除し、生成時にアクティブになってしまうのを防ぐ
        active_obj.select_set(False)

        try:
            for _ in range(count):
                hit_loc = None
                success = False
                
                # 事前にアセットの種類とスケールを決定しておく（近接半径の計算に使うため）
                current_type = scatter_type
                if current_type == 'Mix':
                    current_type = random.choice(['Tree', 'Rock'])
                
                # スケールも事前想定（0.8〜1.2倍）
                simulated_scale = random.uniform(0.8, 1.2)
                
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
                            # 既存オブジェクトの半径を取得
                            other_radius = get_object_radius(other)
                            # 最小安全距離 = 新しいオブジェクトの半径 + 既存オブジェクトの半径
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
                    new_obj["iterations"] = 2
                    new_obj["branchLength"] = round(random.uniform(0.12, 0.18), 2)
                    new_obj["branchRadius"] = round(random.uniform(0.05, 0.08), 3)
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
                    # 新しく生成されたものの選択を解除して競合を防ぐ
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

# 4. UIパネル（サイドバーへの表示）
class MYADDON_PT_level_editor(bpy.types.Panel):
    bl_label = "DirectX レベルエディタ"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "レベルエディタ"  # ここがサイドバーのタブ名になります

    def draw(self, context):
        layout = self.layout
        addon_props = context.scene.my_addon_properties
        
        # 設定セクション
        box = layout.box()
        box.label(text="【設定】", icon='PROPERTIES')
        box.prop(addon_props, "project_path")
        
        layout.separator()
        
        # 作成セクション
        col1 = layout.column(align=True)
        col1.label(text="オブジェクト作成（単体追加）:")
        col1.operator(MYADDON_OT_add_procedural_tree.bl_idname, icon='NODE_SEL')
        col1.operator(MYADDON_OT_add_procedural_rock.bl_idname, icon='MESH_ICOSPHERE')
        
        layout.separator()
        
        # ペイントセクション
        box_paint = layout.box()
        box_paint.label(text="【ペイント配置（簡単クリック）】", icon='BRUSH_DATA')
        box_paint.prop(addon_props, "paint_type")
        box_paint.operator(MYADDON_OT_paint_spawner.bl_idname, icon='PLAY')
        
        layout.separator()
        
        # バイオーム散布セクション
        box_scatter = layout.box()
        box_scatter.label(text="【バイオーム自動散布（範囲生成）】", icon='PARTICLES')
        box_scatter.prop(addon_props, "scatter_count")
        box_scatter.prop(addon_props, "scatter_type")
        box_scatter.operator(MYADDON_OT_scatter_biome.bl_idname, icon='FILE_REFRESH')
        
        layout.separator()
        
        # リセットセクション
        box_reset = layout.box()
        box_reset.label(text="【リセット（全削除・部分削除）】", icon='CANCEL')
        box_reset.operator(MYADDON_OT_clear_biome_objects.bl_idname, icon='REMOVE')
        box_reset.operator(MYADDON_OT_clear_all_objects.bl_idname, icon='TRASH')
        
        layout.separator()
        
        # 出力セクション
        col2 = layout.column(align=True)
        col2.label(text="データ出力:")
        col2.prop(addon_props, "auto_export")
        col2.operator(MYADDON_OT_export_scene.bl_idname, icon='EXPORT')
        col2.label(text=f"同期状態: {addon_props.auto_export_status}")

# 4.5 自動エクスポート用のサイレント処理とハンドラ
def export_scene_silent(scene):
    try:
        addon_props = scene.my_addon_properties
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
            if obj.type == 'MESH' and "type" in obj:
                obj_type = obj["type"]
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
                obj_info = {
                    "name": obj_name,
                    "type": obj_type,
                    "position": {"x": round(pos_x, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
                    "rotation": {"x": round(rot_x, 4), "y": round(rot_y, 4), "z": round(rot_z, 4)},
                    "scale":    {"x": round(scale_x, 4), "y": round(scale_y, 4), "z": round(scale_z, 4)},
                    "parameters": {}
                }
                
                for key in obj.keys():
                    if not key.startswith("_") and key not in ["type", "cycles"]:
                        val = obj[key]
                        if hasattr(val, "to_list"):
                            obj_info["parameters"][key] = val.to_list()
                        else:
                            obj_info["parameters"][key] = val
                            
                layout_data.append(obj_info)
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
        props = scene.my_addon_properties
        if not props.auto_export:
            return
    except AttributeError:
        return

    # オブジェクトの移動や選択変更、パラメータ更新を検知
    should_trigger = False
    for update in depsgraph.updates:
        # プレビュー用の照準球の動きを除外（これを除外しないと、マウスを動かすたびに無限に同期が走って極端に重くなります）
        if getattr(update.id, "name", "") == "Spawner_Preview_Target":
            continue
            
        if update.is_updated_transform or update.is_updated_geometry:
            should_trigger = True
            break

    if should_trigger:
        last_update_time = time.time()
        if not is_timer_running:
            is_timer_running = True
            # 0.1秒後に最初のタイマーチェックを行う
            bpy.app.timers.register(auto_export_timer, first_interval=0.1)

# 5. 登録処理
classes = (
    MyAddonProperties,
    MYADDON_OT_add_procedural_tree,
    MYADDON_OT_add_procedural_rock,
    MYADDON_OT_export_scene,
    MYADDON_OT_paint_spawner,
    MYADDON_OT_scatter_biome,
    MYADDON_OT_clear_all_objects,
    MYADDON_OT_clear_biome_objects,
    MYADDON_PT_level_editor,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.my_addon_properties = bpy.props.PointerProperty(type=MyAddonProperties)
    
    # 古い同名ハンドラがあれば残留競合を防ぐために一掃する
    to_remove = [h for h in bpy.app.handlers.depsgraph_update_post if getattr(h, "__name__", "") == "on_depsgraph_update"]
    for h in to_remove:
        try:
            bpy.app.handlers.depsgraph_update_post.remove(h)
        except:
            pass
            
    # 最新のハンドラを追加
    bpy.app.handlers.depsgraph_update_post.append(on_depsgraph_update)
    print("プロシージャル対応レベルエディタが有効化されました。")

def unregister():
    # 同名のイベントハンドラをすべて解除
    to_remove = [h for h in bpy.app.handlers.depsgraph_update_post if getattr(h, "__name__", "") == "on_depsgraph_update"]
    for h in to_remove:
        try:
            bpy.app.handlers.depsgraph_update_post.remove(h)
        except:
            pass
        
    del bpy.types.Scene.my_addon_properties
    for cls in classes:
        bpy.utils.unregister_class(cls)
    print("プロシージャル対応レベルエディタが無効化されました。")

if __name__ == "__main__":
    try:
        unregister()
    except Exception as e:
        pass
    register()
