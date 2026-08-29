# -*- coding: utf-8 -*-
"""
🎖️ Duckov Stage Studio (Level & Object Editor)
stage_layout.json を視覚的に編集・配置・保存できる専用GUIツール
"""

import os
import sys
import json
import math
import tkinter as tk
from tkinter import ttk, messagebox, filedialog

# stage_layout.json の検索パス
LAYOUT_PATHS = [
    os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Resources", "stage_layout.json"),
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "Resources", "stage_layout.json"),
    r"project\Resources\stage_layout.json",
    r"Resources\stage_layout.json"
]

def find_layout_path():
    for p in LAYOUT_PATHS:
        if os.path.exists(p):
            return os.path.abspath(p)
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target = os.path.join(base, "Resources", "stage_layout.json")
    os.makedirs(os.path.dirname(target), exist_ok=True)
    return target

class LevelStudioApp:
    def __init__(self, root):
        self.root = root
        self.root.title("🎖️ Duckov Stage Studio - ステージ＆オブジェクト配置エディタ")
        self.root.geometry("1180x780")
        self.root.minsize(980, 640)
        self.root.configure(bg="#0f141c")

        self.layout_path = find_layout_path()
        self.objects = []
        self.selected_index = None

        # ビューポート（キャンバス座標変換）パラメータ
        self.scale = 16.0  # 1メートルあたりのピクセル数
        self.offset_x = 420.0
        self.offset_z = 180.0
        self.pan_start = None
        self.dragged_obj_idx = None
        self.drag_offset_x = 0.0
        self.drag_offset_z = 0.0

        self.build_ui()
        self.load_layout()

    def build_ui(self):
        # 1. ヘッダー
        header = tk.Frame(self.root, bg="#141c26", height=60, padx=20, pady=10)
        header.pack(fill="x")

        title_box = tk.Frame(header, bg="#141c26")
        title_box.pack(side="left")
        tk.Label(title_box, text="🎖️ DUCKOV STAGE STUDIO", font=("Segoe UI", 15, "bold"), fg="#00ff88", bg="#141c26").pack(anchor="w")
        self.path_label = tk.Label(title_box, text=f"Layout: {self.layout_path}", font=("Segoe UI", 8), fg="#7a8d9e", bg="#141c26")
        self.path_label.pack(anchor="w")

        btn_box = tk.Frame(header, bg="#141c26")
        btn_box.pack(side="right")

        tk.Button(btn_box, text="💾 保存 (Save JSON)", font=("Segoe UI", 10, "bold"), bg="#00a854", fg="#ffffff", activebackground="#00cc66", padx=16, pady=6, bd=0, cursor="hand2", command=self.save_layout).pack(side="left", padx=5)
        tk.Button(btn_box, text="🔄 再読み込み", font=("Segoe UI", 9), bg="#222f3e", fg="#a0b5c8", activebackground="#304050", padx=12, pady=6, bd=0, cursor="hand2", command=self.load_layout).pack(side="left", padx=5)
        tk.Button(btn_box, text="🔍 表示リセット", font=("Segoe UI", 9), bg="#1f2937", fg="#9ca3af", padx=10, pady=6, bd=0, cursor="hand2", command=self.reset_view).pack(side="left", padx=5)

        # 2. メインコンテナ (キャンバス + サイドバー)
        main_container = tk.Frame(self.root, bg="#0f141c")
        main_container.pack(fill="both", expand=True, padx=12, pady=12)

        # 左側: 2Dマップキャンバス
        canvas_frame = tk.Frame(main_container, bg="#0a0d13", relief="solid", borderwidth=1)
        canvas_frame.pack(side="left", fill="both", expand=True)

        # ツールバー（キャンバス上部）
        toolbar = tk.Frame(canvas_frame, bg="#131922", height=36, padx=10, pady=4)
        toolbar.pack(fill="x")
        tk.Label(toolbar, text="【操作】 左ドラッグ: オブジェクト移動 / 右ドラッグ: 画面パン / ホイール: 拡大縮小", font=("Segoe UI", 8), fg="#94a3b8", bg="#131922").pack(side="left")

        self.canvas = tk.Canvas(canvas_frame, bg="#0d1117", highlightthickness=0)
        self.canvas.pack(fill="both", expand=True)

        self.canvas.bind("<ButtonPress-1>", self.on_canvas_click)
        self.canvas.bind("<B1-Motion>", self.on_canvas_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_canvas_release)
        self.canvas.bind("<ButtonPress-3>", self.on_pan_start)
        self.canvas.bind("<B3-Motion>", self.on_pan_drag)
        self.canvas.bind("<MouseWheel>", self.on_zoom)
        self.canvas.bind("<Configure>", lambda e: self.redraw())

        # 右側: オブジェクト一覧 ＆ プロパティインスペクタ
        sidebar = tk.Frame(main_container, bg="#141c26", width=360, relief="solid", borderwidth=1, padx=12, pady=12)
        sidebar.pack(side="right", fill="y", padx=(12, 0))
        sidebar.pack_propagate(False)

        # オブジェクト追加ボタン群
        tk.Label(sidebar, text="➕ 新規オブジェクト追加", font=("Segoe UI", 11, "bold"), fg="#38bdf8", bg="#141c26").pack(anchor="w", pady=(0, 6))
        add_grid = tk.Frame(sidebar, bg="#141c26")
        add_grid.pack(fill="x", pady=(0, 10))

        tk.Button(add_grid, text="🎯 標的 (Target)", font=("Segoe UI", 9, "bold"), bg="#be123c", fg="#fff", activebackground="#e11d48", bd=0, padx=6, pady=4, cursor="hand2", command=lambda: self.add_object("Target")).grid(row=0, column=0, padx=2, pady=2, sticky="ew")
        tk.Button(add_grid, text="📦 コンテナ", font=("Segoe UI", 9), bg="#1e3a8a", fg="#fff", activebackground="#2563eb", bd=0, padx=6, pady=4, cursor="hand2", command=lambda: self.add_object("Container")).grid(row=0, column=1, padx=2, pady=2, sticky="ew")
        tk.Button(add_grid, text="🚧 フェンス", font=("Segoe UI", 9), bg="#475569", fg="#fff", activebackground="#64748b", bd=0, padx=6, pady=4, cursor="hand2", command=lambda: self.add_object("Fence")).grid(row=1, column=0, padx=2, pady=2, sticky="ew")
        tk.Button(add_grid, text="👾 敵スポーン", font=("Segoe UI", 9), bg="#7c2d12", fg="#fff", activebackground="#c2410c", bd=0, padx=6, pady=4, cursor="hand2", command=lambda: self.add_object("SpawnPoint")).grid(row=1, column=1, padx=2, pady=2, sticky="ew")
        add_grid.columnconfigure(0, weight=1)
        add_grid.columnconfigure(1, weight=1)

        # オブジェクト一覧リスト
        tk.Label(sidebar, text="📋 配置オブジェクト一覧", font=("Segoe UI", 10, "bold"), fg="#94a3b8", bg="#141c26").pack(anchor="w", pady=(6, 2))
        list_frame = tk.Frame(sidebar, bg="#0f141c")
        list_frame.pack(fill="x", pady=(0, 10))

        self.obj_listbox = tk.Listbox(list_frame, bg="#0d1117", fg="#e2e8f0", selectbackground="#0284c7", selectforeground="#ffffff", font=("Consolas", 9), height=7, bd=0, relief="flat")
        self.obj_listbox.pack(side="left", fill="both", expand=True)
        scrollbar = tk.Scrollbar(list_frame, orient="vertical", command=self.obj_listbox.yview, bg="#141c26")
        scrollbar.pack(side="right", fill="y")
        self.obj_listbox.config(yscrollcommand=scrollbar.set)
        self.obj_listbox.bind("<<ListboxSelect>>", self.on_listbox_select)

        # アクションボタン
        act_box = tk.Frame(sidebar, bg="#141c26")
        act_box.pack(fill="x", pady=(0, 10))
        tk.Button(act_box, text="🗑️ 削除", font=("Segoe UI", 9), bg="#7f1d1d", fg="#fca5a5", activebackground="#991b1b", bd=0, padx=8, pady=3, cursor="hand2", command=self.delete_selected).pack(side="left", padx=2)
        tk.Button(act_box, text="📋 複製", font=("Segoe UI", 9), bg="#1e293b", fg="#cbd5e1", activebackground="#334155", bd=0, padx=8, pady=3, cursor="hand2", command=self.duplicate_selected).pack(side="left", padx=2)

        # プロパティインスペクタ
        tk.Label(sidebar, text="⚙️ プロパティ設定", font=("Segoe UI", 10, "bold"), fg="#38bdf8", bg="#141c26").pack(anchor="w", pady=(6, 4))
        self.prop_frame = tk.Frame(sidebar, bg="#18202c", padx=10, pady=10, relief="solid", borderwidth=1)
        self.prop_frame.pack(fill="both", expand=True)

        self.create_property_inputs()

    def create_property_inputs(self):
        labels = ["名前 (Name):", "種類 (Type):", "座標 X (m):", "座標 Y (m):", "座標 Z (m):", "回転 Y (度):", "サイズ (Scale):", "判定半径 (m):"]
        self.prop_vars = {}

        for i, lbl in enumerate(labels):
            tk.Label(self.prop_frame, text=lbl, font=("Segoe UI", 8), fg="#94a3b8", bg="#18202c").grid(row=i, column=0, sticky="w", pady=2)
            var = tk.StringVar()
            entry = tk.Entry(self.prop_frame, textvariable=var, font=("Segoe UI", 9), bg="#0f141c", fg="#f8fafc", insertbackground="#38bdf8", relief="flat")
            entry.grid(row=i, column=1, sticky="ew", padx=(6, 0), pady=2)
            entry.bind("<KeyRelease>", self.on_property_changed)
            self.prop_vars[lbl] = var

        self.prop_frame.columnconfigure(1, weight=1)

    def world_to_screen(self, wx, wz):
        sx = self.offset_x + wx * self.scale
        sy = self.offset_z + wz * self.scale
        return sx, sy

    def screen_to_world(self, sx, sy):
        wx = (sx - self.offset_x) / self.scale
        wz = (sy - self.offset_z) / self.scale
        return wx, wz

    def load_layout(self):
        if os.path.exists(self.layout_path):
            try:
                with open(self.layout_path, "r", encoding="utf-8") as f:
                    self.objects = json.load(f)
            except Exception as e:
                messagebox.showerror("エラー", f"JSON読み込み失敗:\n{e}")
                self.objects = []
        else:
            self.objects = []

        self.refresh_listbox()
        self.redraw()

    def save_layout(self):
        try:
            with open(self.layout_path, "w", encoding="utf-8") as f:
                json.dump(self.objects, f, indent=4, ensure_ascii=False)
            messagebox.showinfo("成功", f"✅ stage_layout.json を保存しました！\nゲーム側へ即時反映されます。")
        except Exception as e:
            messagebox.showerror("エラー", f"JSON保存失敗:\n{e}")

    def refresh_listbox(self):
        self.obj_listbox.delete(0, tk.END)
        for i, obj in enumerate(self.objects):
            name = obj.get("name", f"Object_{i}")
            otype = obj.get("type", "Obstacle")
            pos = obj.get("position", {"x": 0, "z": 0})
            self.obj_listbox.insert(tk.END, f"{name[:18]:<18} ({pos.get('x',0):.1f}, {pos.get('z',0):.1f})")

        if self.selected_index is not None and self.selected_index < len(self.objects):
            self.obj_listbox.selection_set(self.selected_index)
            self.update_property_ui()

    def update_property_ui(self):
        if self.selected_index is None or self.selected_index >= len(self.objects):
            return
        obj = self.objects[self.selected_index]
        self.prop_vars["名前 (Name):"].set(obj.get("name", ""))
        self.prop_vars["種類 (Type):"].set(obj.get("type", ""))
        pos = obj.get("position", {})
        self.prop_vars["座標 X (m):"].set(str(pos.get("x", 0.0)))
        self.prop_vars["座標 Y (m):"].set(str(pos.get("y", 0.0)))
        self.prop_vars["座標 Z (m):"].set(str(pos.get("z", 0.0)))
        rot = obj.get("rotation", {})
        self.prop_vars["回転 Y (度):"].set(str(round(math.degrees(rot.get("y", 0.0)), 1)))
        scl = obj.get("scale", {})
        self.prop_vars["サイズ (Scale):"].set(str(scl.get("x", 1.0)))
        self.prop_vars["判定半径 (m):"].set(str(obj.get("radius", 1.0)))

    def on_property_changed(self, event=None):
        if self.selected_index is None or self.selected_index >= len(self.objects):
            return
        obj = self.objects[self.selected_index]
        obj["name"] = self.prop_vars["名前 (Name):"].get()
        obj["type"] = self.prop_vars["種類 (Type):"].get()
        try:
            obj["position"]["x"] = float(self.prop_vars["座標 X (m):"].get())
            obj["position"]["y"] = float(self.prop_vars["座標 Y (m):"].get())
            obj["position"]["z"] = float(self.prop_vars["座標 Z (m):"].get())
        except ValueError:
            pass

        try:
            deg = float(self.prop_vars["回転 Y (度):"].get())
            obj["rotation"]["y"] = math.radians(deg)
        except ValueError:
            pass

        try:
            s = float(self.prop_vars["サイズ (Scale):"].get())
            obj["scale"] = {"x": s, "y": s, "z": s}
        except ValueError:
            pass

        try:
            obj["radius"] = float(self.prop_vars["判定半径 (m):"].get())
        except ValueError:
            pass

        self.redraw()

    def redraw(self):
        self.canvas.delete("all")
        cw = self.canvas.winfo_width()
        ch = self.canvas.winfo_height()

        # 1. 背景グリッド
        grid_step = 5.0  # 5メートル間隔
        start_wx, start_wz = self.screen_to_world(0, 0)
        end_wx, end_wz = self.screen_to_world(cw, ch)

        min_gx = math.floor(min(start_wx, end_wx) / grid_step) * grid_step
        max_gx = math.ceil(max(start_wx, end_wx) / grid_step) * grid_step
        min_gz = math.floor(min(start_wz, end_wz) / grid_step) * grid_step
        max_gz = math.ceil(max(start_wz, end_wz) / grid_step) * grid_step

        for gx in [min_gx + i * grid_step for i in range(int((max_gx - min_gx) / grid_step) + 1)]:
            sx0, sy0 = self.world_to_screen(gx, min_gz)
            sx1, sy1 = self.world_to_screen(gx, max_gz)
            col = "#1e293b" if abs(gx) < 0.1 else "#131c26"
            self.canvas.create_line(sx0, 0, sx0, ch, fill=col, width=1.5 if abs(gx) < 0.1 else 1)
            self.canvas.create_text(sx0 + 12, 16, text=f"{gx:.0f}m", fill="#475569", font=("Segoe UI", 7))

        for gz in [min_gz + i * grid_step for i in range(int((max_gz - min_gz) / grid_step) + 1)]:
            sx0, sy0 = self.world_to_screen(min_gx, gz)
            col = "#1e293b" if abs(gz) < 0.1 else "#131c26"
            self.canvas.create_line(0, sy0, cw, sy0, fill=col, width=1.5 if abs(gz) < 0.1 else 1)
            self.canvas.create_text(16, sy0 - 8, text=f"{gz:.0f}m", fill="#475569", font=("Segoe UI", 7))

        # 2. 川 (River) の帯を描画 (Z: 16.25m 〜 21.25m)
        r_top_sx, r_top_sy = self.world_to_screen(-30, 16.25)
        r_bot_sx, r_bot_sy = self.world_to_screen(30, 21.25)
        self.canvas.create_rectangle(0, r_top_sy, cw, r_bot_sy, fill="#0c4a6e", outline="#0284c7", width=1.5)
        self.canvas.create_text(cw * 0.5, (r_top_sy + r_bot_sy) * 0.5, text="🌊 RIVER / 川エリア (レーザー・弾丸貫通)", fill="#38bdf8", font=("Segoe UI", 10, "bold"))

        # 3. プレイヤー初期位置 (0, 0)
        p_sx, p_sy = self.world_to_screen(0.0, 0.0)
        self.canvas.create_oval(p_sx - 10, p_sy - 10, p_sx + 10, p_sy + 10, fill="#facc15", outline="#eab308", width=2)
        self.canvas.create_text(p_sx, p_sy - 16, text="🦆 PLAYER (0, 0)", fill="#facc15", font=("Segoe UI", 9, "bold"))

        # 4. オブジェクト描画
        for i, obj in enumerate(self.objects):
            otype = obj.get("type", "")
            name = obj.get("name", "")
            pos = obj.get("position", {"x": 0, "z": 0})
            wx = pos.get("x", 0.0)
            wz = pos.get("z", 0.0)
            sx, sy = self.world_to_screen(wx, wz)

            is_sel = (i == self.selected_index)

            if otype == "Target" or "Target" in name:
                # 標的 (🎯 赤丸)
                r = obj.get("radius", 0.8) * self.scale
                self.canvas.create_oval(sx - r, sy - r, sx + r, sy + r, fill="#be123c", outline="#fb7185" if is_sel else "#e11d48", width=3 if is_sel else 1.5)
                self.canvas.create_oval(sx - r*0.5, sy - r*0.5, sx + r*0.5, sy + r*0.5, fill="#fff", outline="")
                self.canvas.create_oval(sx - r*0.25, sy - r*0.25, sx + r*0.25, sy + r*0.25, fill="#be123c", outline="")
                self.canvas.create_text(sx, sy - r - 10, text=f"🎯 {name}", fill="#f43f5e" if is_sel else "#cbd5e1", font=("Segoe UI", 9, "bold" if is_sel else "normal"))

            elif otype == "GoalRing" or "Goal" in name:
                # 脱出ヘリパッド (🚁 エメラルド)
                r = 2.5 * self.scale
                self.canvas.create_oval(sx - r, sy - r, sx + r, sy + r, fill="#064e3b", outline="#34d399", width=2.5)
                self.canvas.create_text(sx, sy, text="🚁 [H] HELIPAD", fill="#6ee7b7", font=("Segoe UI", 9, "bold"))

            elif otype == "SpawnPoint" or "Spawn" in name:
                # 敵スポーン (👾 オレンジ)
                r = 1.0 * self.scale
                self.canvas.create_rectangle(sx - r, sy - r, sx + r, sy + r, fill="#7c2d12", outline="#fb923c" if is_sel else "#ea580c", width=2)
                self.canvas.create_text(sx, sy - r - 8, text=f"👾 {name}", fill="#fdba74", font=("Segoe UI", 8))

            elif "container" in obj.get("modelFilename", "").lower() or otype == "Container":
                # コンテナ (📦 大型ブルー)
                bw = 2.4 * self.scale
                bh = 4.0 * self.scale
                self.canvas.create_rectangle(sx - bw*0.5, sy - bh*0.5, sx + bw*0.5, sy + bh*0.5, fill="#1e3a8a", outline="#60a5fa" if is_sel else "#3b82f6", width=2.5 if is_sel else 1.5)
                self.canvas.create_text(sx, sy, text=name, fill="#bfdbfe", font=("Segoe UI", 8))

            else:
                # フェンス・その他 (🚧 スレート)
                bw = 1.8 * self.scale
                bh = 0.6 * self.scale
                self.canvas.create_rectangle(sx - bw*0.5, sy - bh*0.5, sx + bw*0.5, sy + bh*0.5, fill="#334155", outline="#cbd5e1" if is_sel else "#64748b", width=2 if is_sel else 1)
                self.canvas.create_text(sx, sy - 8, text=name[:10], fill="#94a3b8", font=("Segoe UI", 7))

            if is_sel:
                # 選択ハイライトサークル
                self.canvas.create_oval(sx - 18, sy - 18, sx + 18, sy + 18, outline="#38bdf8", width=2, dash=(4, 4))

    def on_canvas_click(self, event):
        wx, wz = self.screen_to_world(event.x, event.y)
        clicked_idx = None
        min_dist = 2.0  # クリック許容距離 (m)

        for i, obj in enumerate(self.objects):
            pos = obj.get("position", {})
            ox = pos.get("x", 0.0)
            oz = pos.get("z", 0.0)
            dist = math.sqrt((ox - wx)**2 + (oz - wz)**2)
            if dist < min_dist:
                min_dist = dist
                clicked_idx = i

        self.selected_index = clicked_idx
        if clicked_idx is not None:
            self.dragged_obj_idx = clicked_idx
            pos = self.objects[clicked_idx].get("position", {})
            self.drag_offset_x = pos.get("x", 0.0) - wx
            self.drag_offset_z = pos.get("z", 0.0) - wz
            self.obj_listbox.selection_clear(0, tk.END)
            self.obj_listbox.selection_set(clicked_idx)
            self.obj_listbox.see(clicked_idx)
            self.update_property_ui()
        else:
            self.dragged_obj_idx = None

        self.redraw()

    def on_canvas_drag(self, event):
        if self.dragged_obj_idx is not None and self.dragged_obj_idx < len(self.objects):
            wx, wz = self.screen_to_world(event.x, event.y)
            obj = self.objects[self.dragged_obj_idx]
            new_x = round(wx + self.drag_offset_x, 2)
            new_z = round(wz + self.drag_offset_z, 2)
            obj["position"]["x"] = new_x
            obj["position"]["z"] = new_z
            self.update_property_ui()
            self.redraw()

    def on_canvas_release(self, event):
        self.dragged_obj_idx = None

    def on_pan_start(self, event):
        self.pan_start = (event.x, event.y)

    def on_pan_drag(self, event):
        if self.pan_start is not None:
            dx = event.x - self.pan_start[0]
            dy = event.y - self.pan_start[1]
            self.offset_x += dx
            self.offset_z += dy
            self.pan_start = (event.x, event.y)
            self.redraw()

    def on_zoom(self, event):
        factor = 1.15 if event.delta > 0 else 0.85
        self.scale *= factor
        if self.scale < 4.0: self.scale = 4.0
        if self.scale > 60.0: self.scale = 60.0
        self.redraw()

    def reset_view(self):
        self.scale = 16.0
        self.offset_x = self.canvas.winfo_width() * 0.5
        self.offset_z = self.canvas.winfo_height() * 0.2
        self.redraw()

    def on_listbox_select(self, event):
        sel = self.obj_listbox.curselection()
        if sel:
            self.selected_index = sel[0]
            self.update_property_ui()
            self.redraw()

    def add_object(self, otype):
        cx, cz = self.screen_to_world(self.canvas.winfo_width() * 0.5, self.canvas.winfo_height() * 0.5)
        new_obj = {
            "name": f"New_{otype}_{len(self.objects)+1}",
            "type": otype,
            "modelDirectory": "Resources",
            "modelFilename": "shooting_target.obj" if otype == "Target" else ("container.obj" if otype == "Container" else "fence.obj"),
            "position": {"x": round(cx, 1), "y": 0.0, "z": round(cz, 1)},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
            "radius": 0.8 if otype == "Target" else 1.0,
            "isStatic": True
        }
        self.objects.append(new_obj)
        self.selected_index = len(self.objects) - 1
        self.refresh_listbox()
        self.redraw()

    def delete_selected(self):
        if self.selected_index is not None and self.selected_index < len(self.objects):
            del self.objects[self.selected_index]
            self.selected_index = None
            self.refresh_listbox()
            self.redraw()

    def duplicate_selected(self):
        if self.selected_index is not None and self.selected_index < len(self.objects):
            import copy
            clone = copy.deepcopy(self.objects[self.selected_index])
            clone["name"] += "_Copy"
            clone["position"]["x"] += 1.0
            self.objects.append(clone)
            self.selected_index = len(self.objects) - 1
            self.refresh_listbox()
            self.redraw()

if __name__ == "__main__":
    root = tk.Tk()
    app = LevelStudioApp(root)
    root.mainloop()
