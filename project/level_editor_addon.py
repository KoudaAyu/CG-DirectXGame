import bpy
import json
import os
import math

# -------------------------------------------------------------
# 1. アドオンのプロフィール情報（Blenderに伝える設定）
# -------------------------------------------------------------
bl_info = {
    "name": "DirectX レベルエディタ",
    "author": "Kouda Ayu",
    "version": (1, 0),
    "blender": (3, 3, 0),
    "location": "3Dビューポート > サイドバー(Nキー) > レベルエディタ",
    "description": "Blenderの配置データをDirectX用のY-up・左手系（ラジアン）座標に変換して保存します",
    "category": "Object"
}

# -------------------------------------------------------------
# 2. 設定データ（アドオン内に保存する変数）
# -------------------------------------------------------------
class MyAddonProperties(bpy.types.PropertyGroup):
    # プロジェクトフォルダを選択するためのパス入力欄（フォルダ選択アイコン付き）
    project_path: bpy.props.StringProperty(
        name="プロジェクトパス",
        description="DirectXGame.sln があるprojectフォルダを選択してください",
        default="C:\\Users\\k024g\\OneDrive\\デスクトップ\\Engine_ver2026\\project",
        subtype='DIR_PATH'
    )

# -------------------------------------------------------------
# 3. オペレータ（ボタンを押したときに働く処理ロボット）
# -------------------------------------------------------------

# シーン内のオブジェクトをJSONファイルとして保存するロボット
class MYADDON_OT_export_scene(bpy.types.Operator):
    bl_idname = "myaddon.export_scene"
    bl_label = "JSONファイルに出力"
    bl_description = "配置データをゲームのResourcesフォルダにJSONとして出力します"

    def execute(self, context):
        # アドオン設定から保存先パスを取得する
        addon_props = context.scene.my_addon_properties
        project_dir = bpy.path.abspath(addon_props.project_path)
        
        if not project_dir or not os.path.exists(project_dir):
            self.report({'ERROR'}, "有効なプロジェクトパスを選択してください。")
            return {'CANCELLED'}
        
        # 保存する先のパスを決める ( project/Resources/stage_layout.json )
        output_path = os.path.join(project_dir, "Resources", "stage_layout.json")
        
        layout_data = []

        # シーン内のオブジェクトをチェック
        for obj in bpy.context.scene.objects:
            # メッシュオブジェクトのみ対象
            if obj.type == 'MESH':
                
                # --- 座標の変換 (Blender:Z-up右手系 -> DirectX:Y-up左手系) ---
                # 位置の変換 (Zが高さ、Yが奥行き)
                pos_x = obj.location.x
                pos_y = obj.location.z
                pos_z = obj.location.y
                
                # 回転の変換 (ラジアンのまま格納)
                rot_x = obj.rotation_euler.x
                rot_y = obj.rotation_euler.z  # Z軸回転 -> DirectXのY軸回転(Yaw)
                rot_z = obj.rotation_euler.y  # Y軸回転 -> DirectXのZ軸回転(Roll)
                
                # スケールの変換
                scale_x = obj.scale.x
                scale_y = obj.scale.z
                scale_z = obj.scale.y
                
                # コピー名（.001など）を取り除く
                obj_name = obj.name.split('.')[0]
                
                obj_info = {
                    "name": obj_name,
                    "position": {"x": round(pos_x, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
                    "rotation": {"x": round(rot_x, 4), "y": round(rot_y, 4), "z": round(rot_z, 4)},
                    "scale":    {"x": round(scale_x, 4), "y": round(scale_y, 4), "z": round(scale_z, 4)}
                }
                layout_data.append(obj_info)
                
        # ディレクトリがない場合は自動作成
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        # JSONファイルの保存
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(layout_data, f, indent=4, ensure_ascii=False)
            
        self.report({'INFO'}, f"エクスポート完了: {len(layout_data)}個のモデル情報を保存しました")
        print(f"[Success] Exported to: {output_path}")
        return {'FINISHED'}

# ICO球を追加するロボット (テスト用)
class MYADDON_OT_add_ico_sphere(bpy.types.Operator):
    bl_idname = "myaddon.add_ico_sphere"
    bl_label = "ICO球を追加"
    bl_description = "テスト用の球体オブジェクトを追加します"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add()
        return {'FINISHED'}


# -------------------------------------------------------------
# 4. パネル（画面右側のサイドバーに表示する操作UI）
# -------------------------------------------------------------
class MYADDON_PT_level_editor(bpy.types.Panel):
    bl_label = "DirectX レベルエディタ"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "レベルエディタ"  # Nキーを押した時にでてくるタブ名

    def draw(self, context):
        layout = self.layout
        addon_props = context.scene.my_addon_properties
        
        # 設定エリア
        box = layout.box()
        box.label(text="【設定】", icon='PROPERTIES')
        box.prop(addon_props, "project_path") # フォルダ選択UIを表示する
        
        layout.separator()
        
        # 配置ツールエリア
        col1 = layout.column(align=True)
        col1.label(text="オブジェクト作成:")
        col1.operator(MYADDON_OT_add_ico_sphere.bl_idname, icon='MESH_ICOSPHERE')
        
        layout.separator()
        
        # エクスポートエリア
        col2 = layout.column(align=True)
        col2.label(text="データ出力:")
        col2.operator(MYADDON_OT_export_scene.bl_idname, icon='EXPORT')


# -------------------------------------------------------------
# 5. 登録と登録解除の処理
# -------------------------------------------------------------
classes = (
    MyAddonProperties,
    MYADDON_OT_export_scene,
    MYADDON_OT_add_ico_sphere,
    MYADDON_PT_level_editor,
)

def register():
    # 1. 各種クラスをBlenderに登録
    for cls in classes:
        bpy.utils.register_class(cls)
        
    # 2. カスタム変数を登録
    bpy.types.Scene.my_addon_properties = bpy.props.PointerProperty(type=MyAddonProperties)
    print("DirectXレベルエディタが有効化されました。")

def unregister():
    # 2. カスタム変数の登録解除
    del bpy.types.Scene.my_addon_properties
    
    # 1. クラスを順に登録解除
    for cls in classes:
        bpy.utils.unregister_class(cls)
    print("DirectXレベルエディタが無効化されました。")

if __name__ == "__main__":
    register()
