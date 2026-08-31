import bpy
import json
import os
import math
import re

def sanitize_filename(name):
    """ファイル名として使用不可な文字をアンダースコアに置換"""
    return re.sub(r'[\\/*?:"<>|]', '_', name).strip()

def export_stage_layout(filepath):
    """
    Blenderシーン内のメッシュオブジェクト情報を書き出すスクリプト。
    標準モデル(container, fence, plane, teapot)以外は個別の .obj ファイルとして 
    project/Resources/ へ自動エクスポートし、stage_layout.json に正しく紐付けます。
    """
    layout_data = []
    resources_dir = os.path.dirname(os.path.abspath(filepath))
    os.makedirs(resources_dir, exist_ok=True)
    
    # 選択状態を一時退避
    active_obj = bpy.context.active_object
    selected_objs = [o for o in bpy.context.selected_objects]

    # シーン内の全メッシュオブジェクトを走査
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            # Blender(右手系、Z-up) から DirectX(左手系、Y-up) への座標・回転・スケール変換
            pos_x = obj.location.x
            pos_y = obj.location.z  # Zが高さ
            pos_z = obj.location.y  # Yが奥行き
            
            rot_x = obj.rotation_euler.x
            rot_y = obj.rotation_euler.z  # Z軸回転がDirectXのY軸回転(Yaw)に対応
            rot_z = obj.rotation_euler.y  # Y軸回転がDirectXのZ軸回転(Roll)に対応
            
            scale_x = obj.scale.x
            scale_y = obj.scale.z
            scale_z = obj.scale.y
            
            obj_name = obj.name.split('.')[0]
            obj_type = obj.get("type", "Obstacle")
            
            model_dir = "Resources"
            lower_name = obj_name.lower()
            lower_type = str(obj_type).lower()

            # 1. 既存の共有標準モデル判定
            if "plane" in lower_name or "ground" in lower_name:
                model_file = "plane.obj"
            elif "container" in lower_name or lower_type == "container":
                model_file = "container.obj"
            elif "fence" in lower_name or lower_type == "fence":
                model_file = "fence.obj"
            elif "teapot" in lower_name:
                model_file = "teapot.obj"
            else:
                # 2. カスタム・自作メッシュオブジェクトの個別の .obj エクスポート
                mesh_base_name = sanitize_filename(obj.data.name if obj.data else obj_name)
                clean_name = sanitize_filename(obj_name)
                obj_filename = f"{clean_name}.obj"
                export_path = os.path.join(resources_dir, obj_filename)

                # オブジェクトを選択して個別.objとして保存
                bpy.ops.object.select_all(action='DESELECT')
                obj.select_set(True)
                bpy.context.view_layer.objects.active = obj

                try:
                    # Blender 3.2+ / 4.x / 5.x 高速ネイティブエクスポート
                    bpy.ops.wm.obj_export(
                        filepath=export_path,
                        export_selected_objects=True,
                        apply_modifiers=True
                    )
                except Exception:
                    try:
                        # Blender 3.1以前用
                        bpy.ops.export_scene.obj(
                            filepath=export_path,
                            use_selection=True,
                            use_mesh_modifiers=True
                        )
                    except Exception as e:
                        print(f"[Warning] Could not export OBJ for {obj.name}: {e}")

                # 該当ファイルが生成できたか確認
                if os.path.exists(export_path):
                    model_file = obj_filename
                else:
                    model_file = "fence.obj"  # 最終フォールバック

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

    # 選択状態の復元
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

    # JSONとして出力
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(layout_data, f, indent=4, ensure_ascii=False)
        
    print(f"[Success] Exported {len(layout_data)} objects to: {filepath}")

# 実行コード：プロジェクトの Resources/stage_layout.json として書き出します
script_dir = os.path.dirname(os.path.abspath(__file__))
output_path = os.path.join(script_dir, "project", "Resources", "stage_layout.json")
export_stage_layout(output_path)

