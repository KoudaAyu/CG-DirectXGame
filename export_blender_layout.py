import bpy
import json
import os
import math

def export_stage_layout(filepath):
    """
    Blenderシーン内のメッシュオブジェクトのトランスフォーム情報を取得し、
    DirectXゲームエンジン(左手系、Y-up)に適合するように座標変換してJSONとして書き出すスクリプト。
    """
    layout_data = []
    
    # シーン内のオブジェクトを走査
    for obj in bpy.context.scene.objects:
        # メッシュオブジェクトのみを対象とする
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
            obj_type = obj.get("type", "Fence")
            
            # C++エンジン側がモデルを正しくロードできるよう modelDirectory と modelFilename を自動付与
            model_dir = "Resources"
            model_file = "fence.obj"
            if "plane" in obj_name.lower() or "ground" in obj_name.lower():
                model_file = "plane.obj"

            obj_info = {
                "name": obj_name,
                "type": obj_type,
                "modelDirectory": model_dir,
                "modelFilename": model_file,
                "position": {"x": round(pos_x, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
                "rotation": {"x": round(rot_x, 4), "y": round(rot_y, 4), "z": round(rot_z, 4)},
                "scale":    {"x": round(scale_x, 4), "y": round(scale_y, 4), "z": round(scale_z, 4)}
            }
            layout_data.append(obj_info)
            
    # JSONとして出力
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(layout_data, f, indent=4, ensure_ascii=False)
        
    print(f"[Success] Exported {len(layout_data)} objects to: {filepath}")

# 実行コード：プロジェクトの Resources/stage_layout.json として書き出します
script_dir = os.path.dirname(os.path.abspath(__file__))
output_path = os.path.join(script_dir, "project", "Resources", "stage_layout.json")
export_stage_layout(output_path)
