import bpy

blend_filepath = r"C:\Users\3329a\OneDrive\デスクトップ\Engine\project\stage_editor.blend"
bpy.ops.wm.open_mainfile(filepath=blend_filepath)

# Set camera to view the stage
cam_data = bpy.data.cameras.new("TestCam")
cam_obj = bpy.data.objects.new("TestCam", cam_data)
bpy.context.scene.collection.objects.link(cam_obj)
bpy.context.scene.camera = cam_obj

cam_obj.location = (0.0, -10.0, 15.0)
cam_obj.rotation_euler = (0.8, 0.0, 0.0)

render_path = r"C:\Users\3329a\.gemini\antigravity-ide\brain\31e4bd81-caf8-487a-85cd-12f13af42ebf\rendered_stage_view.png"
bpy.context.scene.render.filepath = render_path
bpy.context.scene.render.resolution_x = 800
bpy.context.scene.render.resolution_y = 600

bpy.ops.render.render(write_still=True)
print(f"Rendered viewport image to: {render_path}")
