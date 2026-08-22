import bpy
import os
import math
import random

output_dir = r"C:\Users\3329a\OneDrive\デスクトップ\Engine\project\Resources"
os.makedirs(output_dir, exist_ok=True)

# 1. リアルな血飛沫・血滴スプラッターテクスチャ blood_splatter.png (128x128)
width, height = 128, 128
image_blood = bpy.data.images.new("blood_splatter", width=width, height=height, alpha=True)
pixels_blood = [0.0] * (4 * width * height)

# 複数の不規則な血滴・飛沫を合成してリアルなスプラッターを生成
# 中心メインの血滴 + 周囲に飛び散る微細な滴 (Satellite droplets)
blobs = [
    (64, 64, 28.0, 1.0),    # メインの血滴コア
    (54, 70, 18.0, 0.9),    # 歪んだ肉塊・液滴
    (74, 58, 16.0, 0.9),
    (60, 48, 14.0, 0.85),
    (80, 78, 10.0, 0.75),   # 飛び散った小液滴
    (42, 82, 8.0, 0.7),
    (88, 44, 7.0, 0.7),
    (36, 42, 9.0, 0.65),
    (92, 70, 6.0, 0.6),
    (46, 28, 5.0, 0.6),
    (68, 96, 7.0, 0.6),
    (24, 66, 6.0, 0.5),
    (102, 54, 5.0, 0.5),
]

for y in range(height):
    for x in range(width):
        # 各ピクセルの血滴濃度を計算
        total_density = 0.0
        for (bx, by, br, bweight) in blobs:
            dx = x - bx
            dy = y - by
            dist = math.sqrt(dx*dx + dy*dy)
            if dist < br:
                # 滑らかなドロップオフ
                norm = dist / br
                density = (1.0 - norm * norm) * bweight
                total_density += density

        total_density = min(total_density, 1.0)
        
        idx = (y * width + x) * 4
        if total_density > 0.05:
            # 鮮血・深紅のグラデーション (中心部は暗い深紅、縁は鮮血)
            edge_factor = total_density
            r = 0.85 * edge_factor + 0.15
            g = 0.03 * edge_factor
            b = 0.03 * edge_factor
            a = min(total_density * 1.5, 1.0)
            pixels_blood[idx] = r
            pixels_blood[idx+1] = g
            pixels_blood[idx+2] = b
            pixels_blood[idx+3] = a
        else:
            pixels_blood[idx] = 0.0
            pixels_blood[idx+1] = 0.0
            pixels_blood[idx+2] = 0.0
            pixels_blood[idx+3] = 0.0

image_blood.pixels = pixels_blood
blood_path = os.path.join(output_dir, "blood_splatter.png")
image_blood.filepath_raw = blood_path
image_blood.file_format = 'PNG'
image_blood.save()
print(f"Blood splatter texture saved to {blood_path}")

# 2. リアルな暗い血煙・硝煙テクスチャ smoke_dark.png (128x128)
image_smoke = bpy.data.images.new("smoke_dark", width=width, height=height, alpha=True)
pixels_smoke = [0.0] * (4 * width * height)

smoke_blobs = [
    (64, 64, 48.0, 1.0),
    (52, 58, 38.0, 0.8),
    (74, 68, 36.0, 0.8),
    (60, 76, 32.0, 0.7),
    (70, 50, 30.0, 0.7),
]

for y in range(height):
    for x in range(width):
        total_density = 0.0
        for (bx, by, br, bweight) in smoke_blobs:
            dx = x - bx
            dy = y - by
            dist = math.sqrt(dx*dx + dy*dy)
            if dist < br:
                norm = dist / br
                # ソフトなガウシアン風フォールオフ
                density = math.cos(norm * math.pi * 0.5) ** 2 * bweight
                total_density += density

        total_density = min(total_density, 1.0)
        idx = (y * width + x) * 4
        if total_density > 0.02:
            # 暗いダークグレー〜血煙
            pixels_smoke[idx] = 0.4
            pixels_smoke[idx+1] = 0.1
            pixels_smoke[idx+2] = 0.1
            pixels_smoke[idx+3] = total_density * 0.85
        else:
            pixels_smoke[idx] = 0.0
            pixels_smoke[idx+1] = 0.0
            pixels_smoke[idx+2] = 0.0
            pixels_smoke[idx+3] = 0.0

image_smoke.pixels = pixels_smoke
smoke_path = os.path.join(output_dir, "smoke_dark.png")
image_smoke.filepath_raw = smoke_path
image_smoke.file_format = 'PNG'
image_smoke.save()
print(f"Dark smoke texture saved to {smoke_path}")
