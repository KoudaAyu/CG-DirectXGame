import os
import struct
import zlib
import math

def create_png(width, height, pixel_data, filepath):
    # pixel_data: list of (r, g, b, a) tuples, 0-255
    raw_data = bytearray()
    for y in range(height):
        raw_data.append(0) # Filter type 0 (None)
        for x in range(width):
            r, g, b, a = pixel_data[y * width + x]
            raw_data.extend([r, g, b, a])
    
    compressed = zlib.compress(bytes(raw_data), 9)
    
    def chunk(tag, data):
        c = bytearray(tag) + data
        crc = zlib.crc32(c)
        return struct.pack(">I", len(data)) + c + struct.pack(">I", crc)
    
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png_data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", compressed) + chunk(b"IEND", b"")
    
    os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
    with open(filepath, "wb") as f:
        f.write(png_data)
    print(f"Saved PNG: {filepath}")

def generate_bridge_texture():
    width, height = 256, 256
    pixels = []
    
    for y in range(height):
        # 8本の木製プランク
        plank_idx = y // 32
        plank_y = y % 32
        
        # 基本の木材カラー (落ち着いたダークブラウン / オリーブウッド)
        base_hues = [
            (100, 72, 48), (90, 65, 42), (110, 80, 54), (88, 62, 40),
            (105, 75, 50), (95, 68, 45), (108, 78, 52), (92, 64, 43)
        ]
        br, bg, bb = base_hues[plank_idx % 8]
        
        for x in range(width):
            # 木目グレインの計算
            grain = math.sin(x * 0.2 + math.sin(y * 0.1) * 2.0) * 8.0 + math.sin(x * 0.8) * 4.0
            r = max(0, min(255, int(br + grain)))
            g = max(0, min(255, int(bg + grain * 0.8)))
            b = max(0, min(255, int(bb + grain * 0.6)))
            
            # 板の継ぎ目の溝 (黒いスリット)
            if plank_y == 0 or plank_y == 31:
                r = int(r * 0.3)
                g = int(g * 0.3)
                b = int(b * 0.3)
            elif plank_y == 1 or plank_y == 30:
                r = int(r * 0.6)
                g = int(g * 0.6)
                b = int(b * 0.6)
                
            # 左右の鉄製固定ボルト
            for bolt_x in [16, 32, width - 32, width - 16]:
                dist = math.sqrt((x - bolt_x)**2 + (plank_y - 16)**2)
                if dist < 4.0:
                    r, g, b = 40, 42, 45
                    if dist < 2.0:
                        r, g, b = 130, 135, 140
                        
            # 左右両端の鉄製ガードレールフレーム帯
            if x < 8 or x > width - 8:
                r, g, b = 45, 48, 52
            elif x == 8 or x == width - 8:
                r, g, b = 25, 28, 30

            pixels.append((r, g, b, 255))

    p1 = r"project\Resources\duckov_bridge.png"
    create_png(width, height, pixels, p1)
    
    p2 = r"generated\output\Debug\Resources\duckov_bridge.png"
    create_png(width, height, pixels, p2)

if __name__ == "__main__":
    generate_bridge_texture()
