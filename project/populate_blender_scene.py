import bpy, os, json

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
    obj = bpy.data.objects.new(name, mesh)
    return obj

# Clear existing mesh objects
objs_to_clear = [o for o in bpy.context.scene.objects if o.type in ['MESH', 'CURVE', 'SURFACE']]
for o in objs_to_clear:
    bpy.data.objects.remove(o, do_unlink=True)

col_name = "Imported_Stage_Layout"
if col_name in bpy.context.scene.collection.children:
    collection = bpy.context.scene.collection.children[col_name]
else:
    collection = bpy.data.collections.new(col_name)
    bpy.context.scene.collection.children.link(collection)

project_resources_dir = r"c:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources"
json_path = os.path.join(project_resources_dir, "stage_layout.json")

with open(json_path, 'r', encoding='utf-8') as f:
    stage_data = json.load(f)

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
            m_obj = load_obj_mesh_upright(obj_path, model_file)
            if m_obj:
                mesh_cache[model_file] = m_obj.data
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

    collection.objects.link(loaded_obj)
    imported_count += 1

print(f"SUCCESS: Imported {imported_count} objects into Blender (100% upright aligned!)")
