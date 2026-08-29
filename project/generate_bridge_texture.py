import os
from PIL import Image, ImageDraw

def create_bridge_texture():
    width, height = 512, 512
    img = Image.new("RGBA", (width, height), (70, 50, 35, 255))
    draw = ImageDraw.Draw(img)

    # 1. 木製プランク（板張り）ベースカラー
    plank_count = 8
    plank_height = height // plank_count

    colors = [
        (105, 75, 50, 255),
        (95, 68, 45, 255),
        (115, 82, 55, 255),
        (90, 64, 42, 255),
        (110, 78, 52, 255),
        (100, 72, 48, 255),
        (112, 80, 54, 255),
        (92, 65, 43, 255)
    ]

    for i in range(plank_count):
        y0 = i * plank_height
        y1 = (i + 1) * plank_height
        col = colors[i % len(colors)]
        draw.rectangle([0, y0, width, y1], fill=col)

        # 木目調の微細ストライプ
        for step in range(3, plank_height - 3, 5):
            grain_col = (col[0] - 12, col[1] - 10, col[2] - 8, 255)
            draw.line([(0, y0 + step), (width, y0 + step)], fill=grain_col, width=1)

        # 溝（板と板の境界の暗いスリット）
        draw.line([(0, y0), (width, y0)], fill=(30, 20, 15, 255), width=3)
        draw.line([(0, y0 + 1), (width, y0 + 1)], fill=(50, 35, 25, 255), width=1)

        # ボルト / 釘の頭 (左右両端)
        for bolt_x in [24, 48, width - 48, width - 24]:
            bolt_y = y0 + plank_height // 2
            draw.ellipse([bolt_x - 4, bolt_y - 4, bolt_x + 4, bolt_y + 4], fill=(45, 45, 50, 255), outline=(20, 20, 25, 255))
            draw.ellipse([bolt_x - 2, bolt_y - 2, bolt_x + 1, bolt_y + 1], fill=(120, 120, 125, 255))

    # 左右の鉄製補強フレーム帯
    draw.rectangle([0, 0, 12, height], fill=(40, 42, 45, 255))
    draw.rectangle([width - 12, 0, width, height], fill=(40, 42, 45, 255))
    draw.line([(12, 0), (12, height)], fill=(20, 20, 22, 255), width=2)
    draw.line([(width - 12, 0), (width - 12, height)], fill=(20, 20, 22, 255), width=2)

    out_path = r"project\Resources\duckov_bridge.png"
    img.save(out_path)
    print(f"Generated {out_path}")

    # 出力先にもコピー
    out_debug = r"generated\output\Debug\Resources\duckov_bridge.png"
    if os.path.exists(os.path.dirname(out_debug)):
        img.save(out_debug)
        print(f"Copied to {out_debug}")

if __name__ == "__main__":
    create_bridge_texture()
