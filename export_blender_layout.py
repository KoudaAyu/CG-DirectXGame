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
            # Blenderの Z軸(高さ) -> DirectXの Y軸
            # Blenderの Y軸(奥)   -> DirectXの Z軸
            # Blenderの X軸(右)   -> DirectXの X軸 (符号反転なし)
            
            # 位置の変換
            pos_x = obj.location.x
            pos_y = obj.location.z  # Zが高さ
            pos_z = obj.location.y  # Yが奥行き
            
            # 回転の変換 (オイラー角をラジアンのまま格納)
            rot_x = obj.rotation_euler.x
            rot_y = obj.rotation_euler.z  # Z軸回転がDirectXのY軸回転(Yaw)に対応
            rot_z = obj.rotation_euler.y  # Y軸回転がDirectXのZ軸回転(Roll)に対応
            
            # スケールの変換
            scale_x = obj.scale.x
            scale_y = obj.scale.z
            scale_z = obj.scale.y
            
            # プレフィックスによる分類（例: "Obstacle_01" は "Obstacle" として処理）
            obj_name = obj.name.split('.')[0] # Blenderの ".001" などのコピー番号を切り捨てる
            
            obj_info = {
                "name": obj_name,
                "position": {"x": round(pos_x, 4), "y": round(pos_y, 4), "z": round(pos_z, 4)},
                "rotation": {"x": round(rot_x, 4), "y": round(rot_y, 4), "z": round(rot_z, 4)},
                "scale":    {"x": round(scale_x, 4), "y": round(scale_y, 4), "z": round(scale_z, 4)}
            }
            layout_data.append(obj_info)
            
    # JSONとして出力
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(layout_data, f, indent=4, ensure_ascii=False)
        
    print(f"[Success] Exported {len(layout_data)} objects to: {filepath}")

# 実行コード：デスクトップに "stage_layout.json" として書き出します
desktop_dir = os.path.expanduser("~/Desktop")
output_path = os.path.join(desktop_dir, "stage_layout.json")
export_stage_layout(output_path)
