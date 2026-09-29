import os
import zipfile
import sys

source_dir = r"C:\Users\3329a\Downloads\月次制作\クリ博提出用（3作品）"
zip_path = r"C:\Users\3329a\Downloads\月次制作\公田亜優_提出作品（3作品）.zip"

print(f"Creating zip file: {zip_path}...")
total_files = 0
for dp, dns, fns in os.walk(source_dir):
    total_files += len(fns)

processed = 0
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
    for dp, dns, fns in os.walk(source_dir):
        for f in fns:
            file_path = os.path.join(dp, f)
            arcname = os.path.relpath(file_path, source_dir)
            z.write(file_path, arcname)
            processed += 1
            if processed % 500 == 0 or processed == total_files:
                print(f"Compressed {processed}/{total_files} files...")

zip_size_mb = os.path.getsize(zip_path) / (1024 * 1024)
print(f"\nZIP created successfully! Size: {zip_size_mb:.2f} MB")
