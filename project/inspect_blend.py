import bpy

blend_filepath = r"C:\Users\3329a\OneDrive\デスクトップ\Engine\project\stage_editor.blend"
bpy.ops.wm.open_mainfile(filepath=blend_filepath)

print("--- OBJECT LIST IN stage_editor.blend ---")
for obj in bpy.context.scene.objects:
    if obj.type == 'MESH':
        bbox = obj.bound_box
        min_z = min(v[2] for v in bbox)
        max_z = max(v[2] for v in bbox)
        height = max_z - min_z
        print(f"Name: {obj.name:30s} | Type: {obj.get('type',''):10s} | Height(Z): {height:.2f}m | Loc: ({obj.location.x:.1f}, {obj.location.y:.1f}, {obj.location.z:.1f}) | Rot: ({obj.rotation_euler.x:.2f}, {obj.rotation_euler.y:.2f}, {obj.rotation_euler.z:.2f})")
