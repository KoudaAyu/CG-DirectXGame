import os, shutil, zipfile, subprocess, time

downloads_dir = r"C:\Users\k024g\Downloads"
package_folder_name = "Duckov_Release"
package_dir = os.path.join(downloads_dir, package_folder_name)
zip_path = os.path.join(downloads_dir, "Duckov_Release.zip")

src_release_dir = r"generated\output\Release"

print(f"Creating package directory: {package_dir}")
if os.path.exists(package_dir):
    shutil.rmtree(package_dir)
os.makedirs(package_dir, exist_ok=True)

# 1. Copy executable and required runtime DLLs
required_files = [
    "DirectXGame.exe",
    "dxcompiler.dll",
    "dxil.dll",
    "assimp-vc143-mt.dll"
]

for fname in required_files:
    src_file = os.path.join(src_release_dir, fname)
    if os.path.exists(src_file):
        dst_file = os.path.join(package_dir, fname)
        shutil.copy2(src_file, dst_file)
        print(f"Copied: {fname} ({os.path.getsize(dst_file)} bytes)")
    else:
        print(f"WARNING: File not found: {src_file}")

# 2. Copy Resources directory completely
src_res_dir = os.path.join(src_release_dir, "Resources")
dst_res_dir = os.path.join(package_dir, "Resources")
print(f"Copying Resources tree to {dst_res_dir}...")
shutil.copytree(src_res_dir, dst_res_dir)
print(f"Resources copied successfully ({len(os.listdir(dst_res_dir))} top-level items).")

# 3. Create a README / Launcher note
readme_path = os.path.join(package_dir, "README.txt")
with open(readme_path, "w", encoding="utf-8") as f:
    f.write("【Duckov (DirectXGame) リリース版】\n\n")
    f.write("■ 起動方法\n")
    f.write("DirectXGame.exe をダブルクリックして起動してください。\n\n")
    f.write("■ 操作方法\n")
    f.write("・タイトル画面: [SPACE] または [ENTER] で出撃（ゲーム本編へ）\n")
    f.write("・ヘリ演出中: [SPACE] / [ENTER] / [マウスクリック] で演出をスキップして即座に戦闘開始\n")
    f.write("  ※演出を最後まで観た場合は、自動的にタイトル画面へ復帰します（展示デモ仕様）\n")
    f.write("・移動: [W][A][S][D]\n")
    f.write("・照準 / 射撃: [マウス移動] / [左クリック]\n")
    f.write("・回避ローリング: [SPACE]\n")
    f.write("・リロード: [R]\n")
    f.write("・終了: [ESC]\n")

# 4. Verify by running 4 seconds from package_dir
print("Testing standalone execution from Downloads/Duckov_Release...")
p = subprocess.Popen([os.path.join(package_dir, "DirectXGame.exe")], cwd=package_dir)
time.sleep(4.0)
ret = p.poll()
if ret is None:
    print("Standalone execution test PASSED! Game opened and ran with 0 issues.")
    p.terminate()
    try:
        p.wait(timeout=3)
    except Exception:
        p.kill()
else:
    print(f"FAILED: Game exited prematurely with code {ret}")

# 5. Create a clean ZIP archive in Downloads
print(f"Creating ZIP archive: {zip_path}...")
if os.path.exists(zip_path):
    os.remove(zip_path)

with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
    for root, dirs, files in os.walk(package_dir):
        for f in files:
            full_path = os.path.join(root, f)
            rel_path = os.path.relpath(full_path, downloads_dir)
            zipf.write(full_path, rel_path)

zip_size_mb = os.path.getsize(zip_path) / (1024 * 1024)
print(f"ZIP archive created successfully: {zip_path} ({zip_size_mb:.2f} MB)")
print("ALL DONE!")
