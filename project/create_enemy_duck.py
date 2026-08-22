import bpy
import os

# 敵アヒル用のダーク・タクティカルテクスチャ duck_enemy.png の生成
# (0,0): Dark Body         #2B2E34 -> (0.17, 0.18, 0.20)
# (1,0): Crimson Beak/Feet #D91E1E -> (0.85, 0.12, 0.12)
# (2,0): Glowing Red Eye   #FF0000 -> (1.0, 0.0, 0.0)
# (3,0): Skull White       #E6E6E6 -> (0.9, 0.9, 0.9)
# (0,1): Dark Camo Olive   #384030 -> (0.22, 0.25, 0.19)
# (1,1): Dark Blood Red    #800000 -> (0.50, 0.0, 0.0)
# (2,1): Warning Purple    #6B2D8C -> (0.42, 0.18, 0.55)
# (3,1): Steel Gray        #505860 -> (0.31, 0.35, 0.38)

width, height = 64, 64
image = bpy.data.images.new("duck_enemy_texture", width=width, height=height)
pixels = [1.0] * (4 * width * height)

def get_enemy_color(x, y):
    palette = [
        # row 0 (y=0)
        [(0.16, 0.17, 0.19, 1.0), (0.85, 0.12, 0.12, 1.0), (1.0, 0.05, 0.05, 1.0), (0.90, 0.90, 0.90, 1.0)],
        # row 1 (y=1)
        [(0.12, 0.13, 0.14, 1.0), (0.55, 0.05, 0.05, 1.0), (0.42, 0.18, 0.55, 1.0), (0.31, 0.35, 0.38, 1.0)],
        # row 2 (y=2)
        [(0.25, 0.28, 0.24, 1.0), (0.70, 0.15, 0.15, 1.0), (0.95, 0.20, 0.20, 1.0), (0.75, 0.78, 0.82, 1.0)],
        # row 3 (y=3)
        [(0.08, 0.08, 0.09, 1.0), (0.40, 0.02, 0.02, 1.0), (0.30, 0.10, 0.40, 1.0), (0.20, 0.22, 0.25, 1.0)],
    ]
    px = min(max(int(x), 0), 3)
    py = min(max(int(y), 0), 3)
    return palette[py][px]

for y in range(height):
    for x in range(width):
        cell_x = int(x / (width / 4))
        cell_y = int(y / (height / 4))
        col = get_enemy_color(cell_x, cell_y)
        idx = (y * width + x) * 4
        pixels[idx] = col[0]
        pixels[idx+1] = col[1]
        pixels[idx+2] = col[2]
        pixels[idx+3] = col[3]

image.pixels = pixels

output_dir = r"C:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources"
os.makedirs(output_dir, exist_ok=True)
tex_path = os.path.join(output_dir, "duck_enemy.png")
image.filepath_raw = tex_path
image.file_format = 'PNG'
image.save()
print(f"Enemy duck texture saved to {tex_path}")
