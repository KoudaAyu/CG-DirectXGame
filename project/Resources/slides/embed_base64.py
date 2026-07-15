import os
import base64
import re

# パス定義
slides_md_path = r"c:\Users\k024g\OneDrive\デスクトップ\Engine_ver2026\sega_portfolio_slides.md"
image_dir = r"c:\Users\k024g\OneDrive\デスクトップ\Engine_ver2026\project\Resources\slides"

if not os.path.exists(slides_md_path):
    print("Error: sega_portfolio_slides.md not found.")
    exit(1)

with open(slides_md_path, "r", encoding="utf-8") as f:
    content = f.read()

# 画像タグを検索し、Base64に置換する
# 例: ![System Architecture](file:///...) または ![System Architecture](project/...)
pattern = r'!\[([^\]]*)\]\(([^)]+)\)'

def replace_with_base64(match):
    alt_text = match.group(1)
    img_path = match.group(2)
    
    # ファイル名を取り出す (例: slide_system.png)
    file_name = os.path.basename(img_path)
    local_img_path = os.path.join(image_dir, file_name)
    
    if os.path.exists(local_img_path):
        with open(local_img_path, "rb") as img_file:
            encoded_string = base64.b64encode(img_file.read()).decode('utf-8')
            print(f"Embedding: {file_name}")
            return f"![{alt_text}](data:image/png;base64,{encoded_string})"
    else:
        print(f"Warning: Image file not found at {local_img_path}")
        return match.group(0)

new_content = re.sub(pattern, replace_with_base64, content)

with open(slides_md_path, "w", encoding="utf-8") as f:
    f.write(new_content)

print("Base64エンコード化が成功しました！")
