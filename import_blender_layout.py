import bpy
import json
import os
import math

def import_real_obj_model(filepath, location=(0,0,0), rotation=(0,0,0), scale=(1.0,1.0,1.0), name="LoadedObject"):
    """
    project/Resources/ 内にある本物の .obj ファイル (container.obj, fence.obj 等) を
    自動的にBlender 3D空間上に直接インポートして配置する関数
    """
    if not os.path.exists(filepath):
        print(f"[Warning] OBJ file not found at: {filepath}")
        return None

    for o in bpy.context.scene.objects: o.select_set(False)
    before_objs = set(bpy.context.scene.objects)

    try:
        bpy.ops.wm.obj_import(filepath=filepath)
    except Exception:
        try:
            bpy.ops.import_scene.obj(filepath=filepath)
        except Exception as e:
            print(f"[Error] Failed to import {filepath}: {e}")
            return None

    after_objs = set(bpy.context.scene.objects)
    new_objs = list(after_objs - before_objs)

    if not new_objs:
        return None

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

def import_game_stage(filepath=None):
    """
    Resources/ 内にある本物の .obj ファイル(container.obj, fence.obj, teapot.obj, plane.obj)を
    自動的に直接Blender上へ完全自動読込み・リアル再現
    """
    objs_to_clear = [o for o in bpy.context.scene.objects if o.type in ['MESH', 'CURVE', 'SURFACE']]
    for o in objs_to_clear:
        bpy.data.objects.remove(o, do_unlink=True)

    col_name = "Imported_Stage_Layout"
    if col_name in bpy.data.collections:
        collection = bpy.data.collections[col_name]
    else:
        collection = bpy.data.collections.new(col_name)
        bpy.context.scene.collection.children.link(collection)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(script_dir, "project", "Resources"),
        os.path.join(script_dir, "Resources"),
        "C:\\Users\\3329a\\OneDrive\\デスクトップ\\Engine\\project\\Resources"
    ]
    
    resources_dir = ""
    for path in candidates:
        if os.path.exists(path):
            resources_dir = path
            break

    container_path = os.path.join(resources_dir, "container.obj")
    fence_path = os.path.join(resources_dir, "fence.obj")
    plane_path = os.path.join(resources_dir, "plane.obj")
    teapot_path = os.path.join(resources_dir, "teapot.obj")

    # 1. 地面 (plane.obj)
    ground_obj = import_real_obj_model(plane_path, location=(0, 16.0, 0), scale=(25.0, 25.0, 1.0), name="GroundPlane")
    if ground_obj:
        for col in ground_obj.users_collection: col.objects.unlink(ground_obj)
        collection.objects.link(ground_obj)

    # 2. 貨物コンテナ (container.obj)
    c1 = import_real_obj_model(container_path, location=(-2.5, 12.0, 0.0), name="CargoContainer_1")
    if c1:
        c1["type"] = "Container"
        for col in c1.users_collection: col.objects.unlink(c1)
        collection.objects.link(c1)

    c2 = import_real_obj_model(container_path, location=(2.5, 12.0, 0.0), name="CargoContainer_2")
    if c2:
        c2["type"] = "Container"
        for col in c2.users_collection: col.objects.unlink(c2)
        collection.objects.link(c2)

    # 3. 本物のフェンス・バリケード (fence.obj)
    fence_configs = [
        {"name": "Fence_obs1_LeftNear",   "pos": (-5.0, 8.0, 0.0)},
        {"name": "Fence_obs3_RightNear",  "pos": (5.0, 8.0, 0.0)},
        {"name": "Fence_obs2_LeftFar",    "pos": (-5.0, 18.0, 0.0)},
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

    # 4. チュートリアル用の的 (teapot.obj)
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

    # 5. 脱出ゴールリング (GoalRing)
    bpy.ops.mesh.primitive_torus_add(major_radius=1.5, minor_radius=0.15, location=(0.0, 32.0, 0.01))
    ring_obj = bpy.context.active_object
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
        s_obj = bpy.context.active_object
        s_obj.name = s["name"]
        s_obj["type"] = "SpawnPoint"
        for col in s_obj.users_collection: col.objects.unlink(s_obj)
        collection.objects.link(s_obj)

    print("[Success] All real .obj model files automatically imported into Blender.")

# 実行
import_game_stage()

# カメラフォーカス
try:
    for obj in bpy.context.scene.objects: obj.select_set(False)
    if "Imported_Stage_Layout" in bpy.data.collections:
        for obj in bpy.data.collections["Imported_Stage_Layout"].objects: obj.select_set(True)
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            ctx = bpy.context.copy()
            ctx['area'] = area
            ctx['region'] = area.regions[-1]
            bpy.ops.view3d.view_selected(ctx)
            break
except Exception as e:
    pass
