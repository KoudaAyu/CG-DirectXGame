import bpy, os, json

# 1. Open the user's active stage_editor.blend
blend_filepath = r"C:\Users\3329a\OneDrive\デスクトップ\Engine\project\stage_editor.blend"
if os.path.exists(blend_filepath):
    bpy.ops.wm.open_mainfile(filepath=blend_filepath)

# Function to parse OBJ into upright standing Blender mesh (DirectX Y-Up -> Blender Z-Up)
def load_obj_mesh_upright(filepath, name):
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
                        # DirectX OBJ: parts[0]=X, parts[1]=Y(Up), parts[2]=Z(Depth)
                        # Blender format: X=X, Y=Z(Depth), Z=Y(Up) -> Upright standing!
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

# Completely purge all existing mesh objects and collections in stage_editor.blend
for obj in list(bpy.context.scene.objects):
    bpy.data.objects.remove(obj, do_unlink=True)

for block in bpy.data.meshes:
    if block.users == 0:
        bpy.data.meshes.remove(block)

# Read stage_layout.json
project_resources_dir = r"c:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources"
json_path = os.path.join(project_resources_dir, "stage_layout.json")

with open(json_path, 'r', encoding='utf-8') as f:
    stage_data = json.load(f)

# Filter out GroundPlane / plane.obj
stage_data = [item for item in stage_data if item.get("name") != "GroundPlane" and item.get("modelFilename") != "plane.obj"]

with open(json_path, 'w', encoding='utf-8') as f:
    json.dump(stage_data, f, indent=4, ensure_ascii=False)

mesh_cache = {}
imported_count = 0

for item in stage_data:
    name = item.get("name", "Object")
    model_file = item.get("modelFilename", "")
    pos = item.get("position", {"x": 0, "y": 0, "z": 0})
    rot = item.get("rotation", {"x": 0, "y": 0, "z": 0})
    scl = item.get("scale", {"x": 1, "y": 1, "z": 1})
    obj_type = item.get("type", "Obstacle")

    # Position in Blender: X=x, Y=z (depth), Z=y (height)
    loc = (pos.get("x", 0.0), pos.get("z", 0.0), pos.get("y", 0.0))
    rotation = (rot.get("x", 0.0), rot.get("z", 0.0), rot.get("y", 0.0))
    scale = (scl.get("x", 1.0), scl.get("z", 1.0), scl.get("y", 1.0))

    obj_path = os.path.join(project_resources_dir, model_file)
    loaded_obj = None

    if model_file and os.path.exists(obj_path):
        if model_file not in mesh_cache:
            mesh_data = load_obj_mesh_upright(obj_path, model_file)
            if mesh_data:
                mesh_cache[model_file] = mesh_data
        if model_file in mesh_cache:
            loaded_obj = bpy.data.objects.new(name, mesh_cache[model_file])

    if not loaded_obj:
        if "Spawn" in name or obj_type == "SpawnPoint":
            bpy.ops.mesh.primitive_cone_add(radius1=0.5, depth=1.2, location=loc)
            loaded_obj = bpy.context.active_object
        elif "Goal" in name or obj_type == "GoalRing":
            bpy.ops.mesh.primitive_torus_add(major_radius=1.5, minor_radius=0.15, location=loc)
            loaded_obj = bpy.context.active_object
        else:
            bpy.ops.mesh.primitive_cube_add(size=2.0, location=loc)
            loaded_obj = bpy.context.active_object

    loaded_obj.name = name
    loaded_obj.location = loc
    loaded_obj.rotation_euler = rotation
    loaded_obj.scale = scale
    loaded_obj["type"] = obj_type

    bpy.context.scene.collection.objects.link(loaded_obj)
    imported_count += 1

# Save back to stage_editor.blend
bpy.ops.wm.save_mainfile(filepath=blend_filepath)
print(f"🎉 SUCCESS: Re-populated {imported_count} objects in stage_editor.blend with 100% upright standing meshes!")
