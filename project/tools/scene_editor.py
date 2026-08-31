import json
import os
import sys
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser

CONFIG_PATHS = [
    os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Resources", "scene_config.json"),
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "Resources", "scene_config.json"),
    r"project\Resources\scene_config.json",
    r"Resources\scene_config.json"
]

def find_config_path():
    for p in CONFIG_PATHS:
        if os.path.exists(p):
            return os.path.abspath(p)
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target = os.path.join(base, "Resources", "scene_config.json")
    os.makedirs(os.path.dirname(target), exist_ok=True)
    return target

class VisualSceneEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("🎖️ Duckov Studio - シーン遷移＆演出ビジュアルエディタ")
        self.root.geometry("980x680")
        self.root.minsize(880, 580)
        self.root.configure(bg="#0f141c")

        self.config_path = find_config_path()
        self.data = self.load_config()
        self.current_scene = "GAMEPLAY"

        self.create_styles()
        self.build_ui()
        self.select_scene("GAMEPLAY")

    def create_styles(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure(".", background="#0f141c", foreground="#E0E6ED", font=("Segoe UI", 10))
        style.configure("TFrame", background="#0f141c")
        style.configure("Card.TFrame", background="#18202c", relief="solid", borderwidth=1)

    def load_config(self):
        if os.path.exists(self.config_path):
            try:
                with open(self.config_path, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception as e:
                print(f"Failed to load JSON: {e}")
        
        return {
            "transitions": {
                "TITLE": {
                    "title": "RETURNING TO HQ",
                    "subtitle": "TACTICAL BRIEFING TERMINAL INITIALIZED",
                    "frameCount": 20,
                    "hudColor": [0.0, 0.8, 1.0]
                },
                "GAMEPLAY": {
                    "title": "DEPLOYING AGENT DUCK",
                    "subtitle": "OBJECTIVE: DESTROY 3 TARGETS & EXTRACT SAFELY",
                    "frameCount": 20,
                    "hudColor": [0.0, 1.0, 0.55]
                },
                "CLEAR": {
                    "title": "EXTRACTION CONFIRMED",
                    "subtitle": "TACTICAL AIRLIFT INBOUND... RETURNING TO BASE",
                    "frameCount": 25,
                    "hudColor": [0.2, 1.0, 0.4]
                },
                "GAMEOVER": {
                    "title": "SIGNAL LOST",
                    "subtitle": "CASUALTY DETECTED... INITIATING RESCUE OPERATION",
                    "frameCount": 20,
                    "hudColor": [1.0, 0.15, 0.15]
                }
            }
        }

    def build_ui(self):
        # 1. Header Bar
        header = tk.Frame(self.root, bg="#141c26", height=60, padx=20, pady=10)
        header.pack(fill="x")

        title_box = tk.Frame(header, bg="#141c26")
        title_box.pack(side="left")
        tk.Label(title_box, text="🎖️ ESCAPE FROM DUCKOV", font=("Segoe UI", 14, "bold"), fg="#00ff80", bg="#141c26").pack(anchor="w")
        tk.Label(title_box, text="シーン遷移・HUD演出ビジュアルエディタ (変更はゲームへ即時反映)", font=("Segoe UI", 9), fg="#7a8d9e", bg="#141c26").pack(anchor="w")

        btn_box = tk.Frame(header, bg="#141c26")
        btn_box.pack(side="right")

        save_btn = tk.Button(btn_box, text="💾 設定を保存 (Save)", font=("Segoe UI", 10, "bold"), bg="#00aa55", fg="#ffffff", activebackground="#00cc66", padx=16, pady=6, bd=0, cursor="hand2", command=self.save_config)
        save_btn.pack(side="left", padx=5)

        reload_btn = tk.Button(btn_box, text="🔄 再読み込み", font=("Segoe UI", 9), bg="#222f3e", fg="#a0b5c8", activebackground="#304050", padx=12, pady=6, bd=0, cursor="hand2", command=self.reload_config)
        reload_btn.pack(side="left", padx=5)

        # 2. Main Content (Left: Scene Map Navigation / Right: Details & Realtime Preview)
        content = tk.Frame(self.root, bg="#0f141c", padx=15, pady=15)
        content.pack(fill="both", expand=True)

        # Left Column: Scene Flow Visual Map
        left_col = tk.Frame(content, bg="#141c26", width=260, padx=15, pady=15, relief="solid", bd=1, highlightbackground="#223040")
        left_col.pack(side="left", fill="y", padx=(0, 15))
        left_col.pack_propagate(False)

        tk.Label(left_col, text="🗺️ シーン一覧・全体マップ", font=("Segoe UI", 11, "bold"), fg="#00ff80", bg="#141c26").pack(anchor="w", pady=(0, 10))
        tk.Label(left_col, text="編集したいシーンを選択してください：", font=("Segoe UI", 8), fg="#7a8d9e", bg="#141c26").pack(anchor="w", pady=(0, 15))

        self.scene_buttons = {}
        scene_defs = [
            ("TITLE", "🪿 1. TITLE (タイトル画面)", "#00ccff"),
            ("GAMEPLAY", "🎯 2. GAMEPLAY (出撃・本編)", "#00ff80"),
            ("CLEAR", "🏆 3. CLEAR (脱出成功)", "#33ff77"),
            ("GAMEOVER", "💀 4. GAMEOVER (作戦失敗)", "#ff4444")
        ]

        for scene_id, label, accent_color in scene_defs:
            f = tk.Frame(left_col, bg="#1c2633", pady=4, padx=6)
            f.pack(fill="x", pady=6)
            
            btn = tk.Button(f, text=label, font=("Segoe UI", 10, "bold"), bg="#1c2633", fg="#e0e8f0", activebackground="#2a3a4d", activeforeground="#00ff80", bd=0, anchor="w", padx=10, pady=8, cursor="hand2", command=lambda s=scene_id: self.select_scene(s))
            btn.pack(fill="x")
            self.scene_buttons[scene_id] = btn

        # Flow Guide Box
        guide_frame = tk.Frame(left_col, bg="#101820", padx=10, pady=10, relief="solid", bd=1)
        guide_frame.pack(fill="x", side="bottom", pady=(20, 0))
        tk.Label(guide_frame, text="【遷移フロー】\nTITLE ➔ GAMEPLAY\n  ├ 脱出 ➔ CLEAR\n  └ 死亡 ➔ GAMEOVER\nCLEAR/GAMEOVER ➔ TITLE", font=("Consolas", 8), justify="left", fg="#7a95a8", bg="#101820").pack(anchor="w")

        # Right Column: Editor Controls & Live Preview
        right_col = tk.Frame(content, bg="#0f141c")
        right_col.pack(side="left", fill="both", expand=True)

        # Right Top: Live HUD Preview
        preview_frame = tk.Frame(right_col, bg="#141c26", padx=15, pady=12, relief="solid", bd=1, highlightbackground="#223040")
        preview_frame.pack(fill="x", pady=(0, 15))

        tk.Label(preview_frame, text="📺 リアルタイムHUD演出プレビュー (ゲーム中の見え方)", font=("Segoe UI", 10, "bold"), fg="#00ff80", bg="#141c26").pack(anchor="w", pady=(0, 8))

        # Canvas for HUD preview
        self.canvas = tk.Canvas(preview_frame, bg="#0a0e13", height=130, bd=0, highlightthickness=1, highlightbackground="#1e2c3a")
        self.canvas.pack(fill="x")

        # Right Bottom: Edit Controls
        edit_frame = tk.Frame(right_col, bg="#141c26", padx=20, pady=15, relief="solid", bd=1, highlightbackground="#223040")
        edit_frame.pack(fill="both", expand=True)

        self.edit_header = tk.Label(edit_frame, text="⚙️ 設定の編集", font=("Segoe UI", 11, "bold"), fg="#e0e8f0", bg="#141c26")
        self.edit_header.pack(anchor="w", pady=(0, 15))

        # Variables
        self.title_var = tk.StringVar()
        self.subtitle_var = tk.StringVar()
        self.frames_var = tk.IntVar(value=20)
        self.color_var = [0.0, 1.0, 0.55]

        self.title_var.trace_add("write", lambda *args: self.update_preview())
        self.subtitle_var.trace_add("write", lambda *args: self.update_preview())
        self.frames_var.trace_add("write", lambda *args: self.update_preview())

        # Title Input
        tk.Label(edit_frame, text="メインHUDタイトル:", font=("Segoe UI", 9, "bold"), fg="#8fa0b5", bg="#141c26").pack(anchor="w", pady=(0, 2))
        self.title_entry = tk.Entry(edit_frame, textvariable=self.title_var, font=("Consolas", 11), bg="#1e2936", fg="#ffffff", insertbackground="#00ff80", bd=1, relief="solid")
        self.title_entry.pack(fill="x", pady=(0, 12), ipady=4)

        # Subtitle Input
        tk.Label(edit_frame, text="サブテキスト・作戦指示:", font=("Segoe UI", 9, "bold"), fg="#8fa0b5", bg="#141c26").pack(anchor="w", pady=(0, 2))
        self.sub_entry = tk.Entry(edit_frame, textvariable=self.subtitle_var, font=("Consolas", 10), bg="#1e2936", fg="#ffffff", insertbackground="#00ff80", bd=1, relief="solid")
        self.sub_entry.pack(fill="x", pady=(0, 12), ipady=4)

        # Bottom Row: Speed & Color
        row = tk.Frame(edit_frame, bg="#141c26")
        row.pack(fill="x", pady=(5, 0))

        # Frame Count
        speed_box = tk.Frame(row, bg="#141c26")
        speed_box.pack(side="left", padx=(0, 30))
        tk.Label(speed_box, text="シャッター遷移速度 (Frames):", font=("Segoe UI", 9, "bold"), fg="#8fa0b5", bg="#141c26").pack(anchor="w")
        speed_spin = tk.Spinbox(speed_box, from_=5, to=120, textvariable=self.frames_var, width=6, font=("Consolas", 10), bg="#1e2936", fg="#ffffff")
        speed_spin.pack(side="left", pady=(4, 0))
        tk.Label(speed_box, text=" (約0.33秒)", font=("Segoe UI", 8), fg="#657585", bg="#141c26").pack(side="left", padx=5, pady=(4, 0))

        # Color Picker
        color_box = tk.Frame(row, bg="#141c26")
        color_box.pack(side="left")
        tk.Label(color_box, text="HUD発光カラー:", font=("Segoe UI", 9, "bold"), fg="#8fa0b5", bg="#141c26").pack(anchor="w")
        
        color_sub = tk.Frame(color_box, bg="#141c26")
        color_sub.pack(anchor="w", pady=(4, 0))

        self.color_preview_btn = tk.Label(color_sub, text="   ", font=("Segoe UI", 9, "bold"), bg="#00ff8c", width=5, relief="solid", bd=1)
        self.color_preview_btn.pack(side="left")

        pick_btn = tk.Button(color_sub, text="🎨 カラー選択", font=("Segoe UI", 9), bg="#222f3e", fg="#c0d0e0", bd=0, padx=10, pady=2, cursor="hand2", command=self.pick_color)
        pick_btn.pack(side="left", padx=10)

    def select_scene(self, scene_id):
        # Save current scene in memory before switching
        if hasattr(self, 'current_scene') and self.current_scene in self.data.get("transitions", {}):
            self.data["transitions"][self.current_scene] = {
                "title": self.title_var.get(),
                "subtitle": self.subtitle_var.get(),
                "frameCount": self.frames_var.get(),
                "hudColor": self.color_var
            }

        self.current_scene = scene_id

        # Update button highlights
        for sid, btn in self.scene_buttons.items():
            if sid == scene_id:
                btn.config(bg="#00aa55", fg="#ffffff")
            else:
                btn.config(bg="#1c2633", fg="#e0e8f0")

        cfg = self.data.get("transitions", {}).get(scene_id, {})
        self.edit_header.config(text=f"⚙️ 設定の編集: 【 {scene_id} 】シーン")
        self.title_var.set(cfg.get("title", ""))
        self.subtitle_var.set(cfg.get("subtitle", ""))
        self.frames_var.set(cfg.get("frameCount", 20))
        self.color_var = list(cfg.get("hudColor", [0.0, 1.0, 0.55]))
        
        hex_c = "#{:02x}{:02x}{:02x}".format(int(self.color_var[0]*255), int(self.color_var[1]*255), int(self.color_var[2]*255))
        self.color_preview_btn.config(bg=hex_c)

        self.update_preview()

    def pick_color(self):
        hex_c = "#{:02x}{:02x}{:02x}".format(int(self.color_var[0]*255), int(self.color_var[1]*255), int(self.color_var[2]*255))
        chosen = colorchooser.askcolor(hex_c, title=f"カラー選択 - {self.current_scene}")
        if chosen and chosen[0]:
            r, g, b = chosen[0]
            self.color_var = [r / 255.0, g / 255.0, b / 255.0]
            self.color_preview_btn.config(bg=chosen[1])
            self.update_preview()

    def update_preview(self):
        self.canvas.delete("all")
        w = self.canvas.winfo_width()
        if w <= 1:
            w = 600
        h = 130

        # Scanlines
        for y in range(0, h, 4):
            self.canvas.create_line(0, y, w, y, fill="#121a22", width=1)

        # HUD Box
        box_w = min(500, w - 40)
        box_h = 100
        bx1 = (w - box_w) / 2
        by1 = (h - box_h) / 2
        bx2 = bx1 + box_w
        by2 = by1 + box_h

        hex_c = "#{:02x}{:02x}{:02x}".format(int(self.color_var[0]*255), int(self.color_var[1]*255), int(self.color_var[2]*255))

        self.canvas.create_rectangle(bx1, by1, bx2, by2, fill="#111822", outline=hex_c, width=2)

        # Title
        t_text = f"[ {self.title_var.get()} ]"
        self.canvas.create_text(w / 2, by1 + 25, text=t_text, fill=hex_c, font=("Consolas", 12, "bold"))

        # Subtitle
        sub_text = self.subtitle_var.get()
        self.canvas.create_text(w / 2, by1 + 55, text=sub_text, fill="#cfdce8", font=("Consolas", 9))

        # Progress bar
        bar_w = 300
        bar_x1 = (w - bar_w) / 2
        bar_y = by1 + 80
        self.canvas.create_rectangle(bar_x1, bar_y, bar_x1 + bar_w, bar_y + 4, fill="#223344", outline="")
        self.canvas.create_rectangle(bar_x1 + 40, bar_y, bar_x1 + 180, bar_y + 4, fill=hex_c, outline="")

    def save_config(self):
        # Save current scene
        self.data["transitions"][self.current_scene] = {
            "title": self.title_var.get(),
            "subtitle": self.subtitle_var.get(),
            "frameCount": self.frames_var.get(),
            "hudColor": self.color_var
        }

        try:
            with open(self.config_path, "w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=2, ensure_ascii=False)
            messagebox.showinfo("保存完了", f"🎉 設定を正常に保存しました！\n\nファイル:\n{self.config_path}\n\nC++のリビルドなしで即座にゲームへ反映されます。")
        except Exception as e:
            messagebox.showerror("エラー", f"保存に失敗しました:\n{e}")

    def reload_config(self):
        self.data = self.load_config()
        self.select_scene(self.current_scene)
        messagebox.showinfo("再読み込み", "ファイルから最新設定を読み込みました。")

if __name__ == "__main__":
    root = tk.Tk()
    app = VisualSceneEditor(root)
    # Trigger preview redraw on resize
    root.bind("<Configure>", lambda e: app.update_preview())
    root.mainloop()
