# -*- coding: utf-8 -*-
"""
スライド用の図をまとめて生成するスクリプト。
- 紺背景テンプレートに直接置けるよう、背景透過PNGで出力する
- レイの当たり位置・A*の経路・BVHの分割・判定ペア数などは
  エンジンのコードと同じ手順・同じパラメータで計算して描いている

使い方:  py gen_figures.py
出力:    ./png/figXX.png（2倍解像度・透過） / ./preview/figXX.png（紺背景で確認用）
"""
import heapq
import html
import math
import pathlib
import random
import subprocess

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
SVG_DIR = HERE / "svg"
PNG_DIR = HERE / "png"
PREVIEW_DIR = HERE / "preview"
CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"

# ---- テンプレートに合わせた色 ----
BG = "#1B1F2A"      # スライド背景（プレビュー用）
FG = "#E9EDF3"      # 文字
SUB = "#A6B0BE"     # 補足文字
LINE = "#5F6A7B"    # 補助線
DIM = "#3A4252"     # 通れないマス・薄い面
PANEL = "#232937"   # 箱の塗り
GRID = "#2C3342"
BLUE = "#4A86E8"
BLUE_L = "#9CC0F5"
MINT = "#86CBA8"
RED = "#E5675F"
RED_L = "#F3A6A0"
AMBER = "#F2B84B"
WOOD = "#B5834F"
WOOD_S = "#6E4520"
FONT = "'Yu Gothic UI','Yu Gothic','Meiryo',sans-serif"
MONO = "Consolas,'Courier New',monospace"


def esc(s):
    return html.escape(s, quote=True)


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.e = w, h, []

    def add(self, s):
        self.e.append(s)

    def text(self, x, y, s, size=16, fill=FG, anchor="middle", weight=400, family=FONT, opacity=1):
        s = s.replace(" ", "\u00a0") if family == MONO else s   # SVGは連続スペースを詰めるので、コードの字下げを保つ
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" '
                 f'font-weight="{weight}" font-family="{family}" opacity="{opacity}">{esc(s)}</text>')

    def rich(self, x, y, parts, anchor="middle"):
        """parts: [(文字, size, fill, weight)]"""
        spans = "".join(f'<tspan font-size="{sz}" fill="{fl}" font-weight="{wt}">{esc(t)}</tspan>' for t, sz, fl, wt in parts)
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}" font-family="{FONT}">{spans}</text>')

    def line(self, x1, y1, x2, y2, stroke=LINE, w=2, dash=None, opacity=1, end=None, start=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        m = (f' marker-end="url(#{end})"' if end else "") + (f' marker-start="url(#{start})"' if start else "")
        self.add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}" stroke-width="{w}" '
                 f'stroke-linecap="round" opacity="{opacity}"{d}{m}/>')

    def polyline(self, pts, stroke=LINE, w=2, dash=None, opacity=1, end=None, fill="none"):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        m = f' marker-end="url(#{end})"' if end else ""
        p = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        self.add(f'<polyline points="{p}" fill="{fill}" stroke="{stroke}" stroke-width="{w}" stroke-linejoin="round" '
                 f'stroke-linecap="round" opacity="{opacity}"{d}{m}/>')

    def rect(self, x, y, w, h, fill="none", stroke="none", sw=2, rx=0, dash=None, opacity=1):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" fill="{fill}" '
                 f'stroke="{stroke}" stroke-width="{sw}" opacity="{opacity}"{d}/>')

    def circle(self, cx, cy, r, fill="none", stroke="none", sw=2, dash=None, opacity=1):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" '
                 f'opacity="{opacity}"{d}/>')

    def poly(self, pts, fill="none", stroke="none", sw=2, dash=None, opacity=1):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        p = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        self.add(f'<polygon points="{p}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" stroke-linejoin="round" '
                 f'opacity="{opacity}"{d}/>')

    def cross(self, x, y, r=6, stroke=RED, w=2.5):
        self.line(x - r, y - r, x + r, y + r, stroke, w)
        self.line(x - r, y + r, x + r, y - r, stroke, w)

    def hbracket(self, x1, x2, y, stroke=SUB, tick=7):
        self.line(x1, y, x2, y, stroke, 1.5)
        self.line(x1, y - tick, x1, y + tick, stroke, 1.5)
        self.line(x2, y - tick, x2, y + tick, stroke, 1.5)

    def svg(self):
        markers = ""
        for name, col in [("fg", FG), ("sub", SUB), ("mint", MINT), ("red", RED), ("blue", BLUE), ("amber", AMBER)]:
            markers += (f'<marker id="a-{name}" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" '
                        f'orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="{col}"/></marker>')
        body = "\n".join(self.e)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
                f'viewBox="0 0 {self.w} {self.h}"><defs>{markers}</defs>\n{body}\n</svg>')


# ---------------------------------------------------------------- 幾何ヘルパー
def rot(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c)


def board_poly(p1, p2, thick):
    """線分 p1-p2 に厚みをつけた長方形"""
    dx, dy = p2[0] - p1[0], p2[1] - p1[1]
    L = math.hypot(dx, dy)
    nx, ny = -dy / L * thick / 2, dx / L * thick / 2
    return [(p1[0] + nx, p1[1] + ny), (p2[0] + nx, p2[1] + ny), (p2[0] - nx, p2[1] - ny), (p1[0] - nx, p1[1] - ny)]


def ray_seg(o, d, a, b):
    """レイ o + t*d と線分 a-b の交差 t（なければ None）"""
    ex, ey = b[0] - a[0], b[1] - a[1]
    den = d[0] * ey - d[1] * ex
    if abs(den) < 1e-9:
        return None
    t = ((a[0] - o[0]) * ey - (a[1] - o[1]) * ex) / den
    u = ((a[0] - o[0]) * d[1] - (a[1] - o[1]) * d[0]) / den
    if t >= 0 and 0 <= u <= 1:
        return t
    return None


def ray_poly(o, d, pts):
    best = None
    for i in range(len(pts)):
        t = ray_seg(o, d, pts[i], pts[(i + 1) % len(pts)])
        if t is not None and (best is None or t < best):
            best = t
    return best


def seg_hits_poly(a, b, pts):
    L = math.dist(a, b)
    d = ((b[0] - a[0]) / L, (b[1] - a[1]) / L)
    t = ray_poly(a, d, pts)
    return t is not None and t <= L


def draw_board(s, pts, grain=True):
    s.poly(pts, fill=WOOD, stroke=WOOD_S, sw=2)


# ---------------------------------------------------------------- p.3 一覧
def fig03():
    s = Svg(760, 400)
    s.text(170, 28, "7月の発表で挙げたこと", 15, SUB, weight=700)
    s.text(530, 28, "今回やったこと", 15, SUB, weight=700)
    rows = [
        ("高速な弾がフェンスをすり抜ける", "追加", "移動した線で判定する（CCD）", "p.4-5"),
        ("フェンスの判定がずれる", "7月 p.14-15", "板ごとに斜めの箱で判定", "p.6"),
        ("BVHで判定を軽くする（予定）", "7月 p.12・16", "空間ハッシュ ＋ BVH", "p.7-8"),
        ("敵が遮蔽物を使わない", "7月 p.8-9", "隠れ場所を探すAI", "p.9"),
        ("障害物に引っかかる", "7月 p.8・10", "マス目 ＋ A* で経路探索", "p.10"),
        ("AIの機能をまとめて動かす", "追加", "ビヘイビアツリー ＋ エディタ", "p.11"),
    ]
    for i, (l, tag, r, page) in enumerate(rows):
        y0 = 44 + i * 58
        s.rect(20, y0, 300, 46, fill=PANEL, stroke=LINE, sw=1.5, rx=6)
        s.text(36, y0 + 29, l, 15, FG, "start")
        s.text(308, y0 + 29, tag, 12, AMBER if tag == "追加" else SUB, "end")
        s.line(330, y0 + 23, 370, y0 + 23, SUB, 2, end="a-sub")
        s.rect(380, y0, 300, 46, fill=PANEL, stroke=MINT, sw=1.5, rx=6)
        s.text(396, y0 + 29, r, 15, FG, "start")
        s.text(694, y0 + 29, page, 14, MINT, "start", weight=700)
    return s


# ---------------------------------------------------------------- p.4 CCD 課題
def fig04():
    s = Svg(600, 360)
    s.text(20, 26, "上から見た図", 13, SUB, "start")
    bx, bw, by0, by1 = 273, 48, 72, 282           # 板（厚さ0.15m = 48px）
    s.hbracket(bx, bx + bw, 54)
    s.text(bx + bw / 2, 42, "厚さ 0.15m", 14, FG)
    s.rect(bx, by0, bw, by1 - by0, fill=WOOD, stroke=WOOD_S, sw=2, rx=3)
    for gx in (bx + 16, bx + 32):
        s.line(gx, by0 + 10, gx, by1 - 10, WOOD_S, 1, opacity=0.5)
    y = 172
    f1, f2 = bx - 48, bx + bw + 48                # 1フレーム = 0.45m = 144px（板の3倍）
    s.line(f1, y, 520, y, LINE, 2, dash="6 6", end="a-sub")
    for x, name in ((f1, "1フレーム目"), (f2, "2フレーム目")):
        s.circle(x, y, 13, fill=BLUE, stroke=BLUE_L, sw=2)
        s.text(x, y + 40, name, 15, FG)
        s.text(x, y + 60, "板と重ならない", 13, RED)
    s.circle(470, y, 13, stroke=BLUE_L, sw=2, dash="3 3", opacity=0.6)
    s.text(470, y + 40, "そのまま奥へ", 14, SUB)
    s.hbracket(f1, f2, 306)
    s.text((f1 + f2) / 2, 334, "1フレームで 0.45m 進む", 15, FG)
    return s


# ---------------------------------------------------------------- p.5 CCD 解決
def fig05():
    s = Svg(600, 360)
    s.text(20, 26, "上から見た図", 13, SUB, "start")
    bx, bw, by0, by1 = 273, 48, 72, 282
    s.rect(bx, by0, bw, by1 - by0, fill=WOOD, stroke=WOOD_S, sw=2, rx=3)
    for gx in (bx + 16, bx + 32):
        s.line(gx, by0 + 10, gx, by1 - 10, WOOD_S, 1, opacity=0.5)
    y, r = 172, 13
    p0, p1 = bx - 48, bx + bw + 48
    s.line(bx + 6, y, p1 - 16, y, SUB, 2, dash="4 5", opacity=0.6)
    s.line(p0, y - r, bx, y - r, BLUE, 2, opacity=0.75)
    s.line(p0, y + r, bx, y + r, BLUE, 2, opacity=0.75)
    s.line(p0, y, bx, y, BLUE, 4)
    s.circle(p0, y, r, fill=BLUE, stroke=BLUE_L, sw=2)
    s.text(p0, y + 42, "前フレーム", 15, FG)
    s.circle(p1, y, r, stroke=SUB, sw=2, dash="3 3")
    s.text(p1, y + 42, "今フレーム", 15, SUB)
    s.text(p1, y + 62, "（進ませない）", 13, SUB)
    s.circle(bx, y, 6, fill=AMBER)
    s.circle(bx, y, 11, stroke=AMBER, sw=1.5)
    s.line(bx - 4, y - 10, 196, 118, SUB, 1.5)
    s.text(140, 92, "板の表面で止めて", 15, FG)
    s.text(140, 112, "火花と木片を出す", 15, FG)
    s.line(40, 318, 80, 318, BLUE, 4)
    s.text(90, 323, "前と今の位置を結ぶ線", 14, FG, "start")
    s.line(284, 318, 324, 318, BLUE, 2, opacity=0.75)
    s.text(334, 323, "弾の太さ分ずらした線（上下左右 4本）", 14, FG, "start")
    return s


# ---------------------------------------------------------------- p.6 X字フェンス
def fig06():
    s = Svg(720, 400)
    L, T = 280, 12
    s.line(360, 28, 360, 352, DIM, 2)
    s.text(180, 36, "1つの大きな箱で判定", 17, FG, weight=700)
    s.text(540, 36, "板ごとに箱を2つ（±30°）", 17, FG, weight=700)
    for side, cx in (("L", 180), ("R", 540)):
        cy = 200
        boards = []
        for a in (math.radians(30), math.radians(-30)):
            pts = [rot(p, a) for p in ((-L / 2, -T / 2), (L / 2, -T / 2), (L / 2, T / 2), (-L / 2, T / 2))]
            boards.append([(cx + x, cy + y) for x, y in pts])
        for b in boards:
            draw_board(s, b)
        start = (cx + 40, cy + 140)
        up = (0.0, -1.0)
        if side == "L":
            xs = [p[0] for b in boards for p in b]
            ys = [p[1] for b in boards for p in b]
            s.rect(min(xs), min(ys), max(xs) - min(xs), max(ys) - min(ys), stroke=RED, sw=2, dash="6 5")
            hit_y = max(ys)
            label, col = "何もない所で当たる", RED
        else:
            for a in (math.radians(30), math.radians(-30)):
                pts = [rot(p, a) for p in ((-L / 2 - 4, -T / 2 - 4), (L / 2 + 4, -T / 2 - 4), (L / 2 + 4, T / 2 + 4), (-L / 2 - 4, T / 2 + 4))]
                s.poly([(cx + x, cy + y) for x, y in pts], stroke=MINT, sw=2)
            t = min(ray_poly(start, up, b) or 1e9 for b in boards)
            hit_y = start[1] - t
            label, col = "板の表面で当たる", MINT
        s.line(start[0], start[1], start[0], hit_y + 6, BLUE, 3)
        s.circle(start[0], start[1], 9, fill=BLUE, stroke=BLUE_L, sw=2)
        s.circle(start[0], hit_y, 6, fill=AMBER)
        for dx in (-12, 0, 12):
            s.line(start[0] + dx * 0.4, hit_y + 6, start[0] + dx, hit_y + 18, AMBER, 2)
        s.text(start[0] + 14, start[1] - 26, label, 14, col, "start")
        s.text(start[0] + 16, start[1] + 5, "弾", 14, SUB, "start")
    s.text(360, 385, "上から見たX字フェンス（板 4.9m × 厚さ 0.15m を ±30° で2枚）", 13, SUB)
    return s


# ---------------------------------------------------------------- p.7 空間ハッシュ
def fig07():
    s = Svg(720, 380)
    objs = [(25, 30), (70, 25), (45, 80), (130, 45), (175, 85), (250, 55), (125, 140), (170, 150), (150, 195), (260, 180)]
    PW, PH, CW, CH = 300, 220, 100, 110
    s.line(360, 28, 360, 330, DIM, 2)
    s.text(180, 36, "全部の組み合わせを調べる", 16, FG, weight=700)
    s.text(540, 36, "同じマスの中だけ調べる", 16, FG, weight=700)
    # 左：総当たり
    ox, oy = 30, 60
    s.rect(ox, oy, PW, PH, stroke=DIM, sw=1.5, rx=4)
    n_all = 0
    for i in range(len(objs)):
        for j in range(i + 1, len(objs)):
            a, b = objs[i], objs[j]
            s.line(ox + a[0], oy + a[1], ox + b[0], oy + b[1], LINE, 1, opacity=0.6)
            n_all += 1
    for x, y in objs:
        s.circle(ox + x, oy + y, 8, fill=BLUE, stroke=BLUE_L, sw=1.5)
    # 右：空間ハッシュ
    ox = 390
    cells = {}
    for k, (x, y) in enumerate(objs):
        cells.setdefault((int(x // CW), int(y // CH)), []).append(k)
    for (cx, cy), ks in cells.items():
        if len(ks) >= 2:
            s.rect(ox + cx * CW, oy + cy * CH, CW, CH, fill="#26303F")
    s.rect(ox, oy, PW, PH, stroke=LINE, sw=1.5, rx=4)
    for gx in (CW, 2 * CW):
        s.line(ox + gx, oy, ox + gx, oy + PH, LINE, 1.5)
    s.line(ox, oy + CH, ox + PW, oy + CH, LINE, 1.5)
    n_hash = 0
    for ks in cells.values():
        for i in range(len(ks)):
            for j in range(i + 1, len(ks)):
                a, b = objs[ks[i]], objs[ks[j]]
                s.line(ox + a[0], oy + a[1], ox + b[0], oy + b[1], MINT, 2)
                n_hash += 1
    for x, y in objs:
        s.circle(ox + x, oy + y, 8, fill=BLUE, stroke=BLUE_L, sw=1.5)
    s.text(ox + PW, 54, "1マス = 10m", 12, SUB, "end")
    s.rich(180, 314, [("判定する組  ", 15, SUB, 400), (str(n_all), 26, RED, 700), (" 組", 15, SUB, 400)])
    s.rich(540, 314, [("判定する組  ", 15, SUB, 400), (str(n_hash), 26, MINT, 700), (" 組", 15, SUB, 400)])
    s.text(360, 362, "マス番号 = (x*73856093 ^ y*19349663 ^ z*83492791) % 2048", 13, SUB, family=MONO)
    return s, n_all, n_hash


# ---------------------------------------------------------------- p.8 BVH
def fig08():
    s = Svg(800, 400)
    rng = random.Random(11)
    ox, oy, AW, AH = 40, 78, 320, 228
    cw, ch = AW / 4, AH / 4
    tris = []

    def area(p):
        return abs((p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) - (p[2][0] - p[0][0]) * (p[1][1] - p[0][1])) / 2

    for r in range(4):
        for c in range(4):
            x0, y0 = ox + c * cw, oy + r * ch
            m = 9
            while True:   # 細すぎる三角形は作り直す
                p = [(x0 + rng.uniform(m, cw * 0.45), y0 + rng.uniform(m, ch - m)),
                     (x0 + rng.uniform(cw * 0.55, cw - m), y0 + rng.uniform(m, ch * 0.45)),
                     (x0 + rng.uniform(cw * 0.45, cw - m), y0 + rng.uniform(ch * 0.6, ch - m))]
                if area(p) > 380:
                    break
            tris.append(p)

    # エンジンと同じ組み立て：長い軸で重心ソート → 真ん中で分割、4枚以下で葉
    nodes = []

    def build(ids, depth):
        xs = [p[0] for i in ids for p in tris[i]]
        ys = [p[1] for i in ids for p in tris[i]]
        node = {"min": (min(xs), min(ys)), "max": (max(xs), max(ys)), "depth": depth, "ids": ids, "kids": []}
        idx = len(nodes)
        nodes.append(node)
        if len(ids) <= 4:
            return idx
        axis = 0 if (node["max"][0] - node["min"][0]) >= (node["max"][1] - node["min"][1]) else 1
        ids = sorted(ids, key=lambda i: sum(p[axis] for p in tris[i]) / 3)
        mid = len(ids) // 2
        node["kids"] = [build(ids[:mid], depth + 1), build(ids[mid:], depth + 1)]
        return idx

    build(list(range(16)), 0)

    def ray_aabb(o, d, mn, mx, maxd):
        tmin, tmax = 0.0, maxd
        for a in range(2):
            if abs(d[a]) < 1e-9:
                if o[a] < mn[a] or o[a] > mx[a]:
                    return False
            else:
                t1, t2 = (mn[a] - o[a]) / d[a], (mx[a] - o[a]) / d[a]
                if t1 > t2:
                    t1, t2 = t2, t1
                tmin, tmax = max(tmin, t1), min(tmax, t2)
                if tmin > tmax:
                    return False
        return True

    leftleaf = nodes[nodes[nodes[0]["kids"][0]]["kids"][0]] if nodes[nodes[0]["kids"][0]]["kids"] else None
    target = tris[leftleaf["ids"][0]]
    tc = (sum(p[0] for p in target) / 3, sum(p[1] for p in target) / 3)
    o = (ox - 26, oy + 6)
    L = math.dist(o, tc)
    d = ((tc[0] - o[0]) / L, (tc[1] - o[1]) / L)
    maxd = 1000.0
    status = {}
    tested = []

    def trav(i, maxd):
        n = nodes[i]
        if not ray_aabb(o, d, n["min"], n["max"], maxd):
            status[i] = "skip"
            return None
        status[i] = "visit"
        if not n["kids"]:
            best = None
            for k in n["ids"]:
                tested.append(k)
                t = ray_poly(o, d, tris[k])
                if t is not None and t < maxd and (best is None or t < best[0]):
                    best = (t, k)
            return best
        lh = trav(n["kids"][0], maxd)
        rh = trav(n["kids"][1], lh[0] if lh else maxd)
        c = [h for h in (lh, rh) if h]
        return min(c) if c else None

    hit = trav(0, maxd)

    col_of = {0: BLUE, 1: MINT, 2: AMBER}
    pad_of = {0: 10, 1: 5, 2: 1.5}
    for i, n in enumerate(nodes):
        st = status.get(i, "none")
        p = pad_of[n["depth"]]
        x, y = n["min"][0] - p, n["min"][1] - p
        w, h = n["max"][0] - n["min"][0] + 2 * p, n["max"][1] - n["min"][1] + 2 * p
        if st == "visit":
            s.rect(x, y, w, h, stroke=col_of[n["depth"]], sw=2, rx=3)
        else:
            s.rect(x, y, w, h, stroke=SUB, sw=1.5, rx=3, dash="5 5", opacity=0.55)
    for k, t in enumerate(tris):
        if hit and k == hit[1]:
            s.poly(t, fill=AMBER, stroke=AMBER, sw=1.5)
        elif k in tested:
            s.poly(t, fill=DIM, stroke=FG, sw=1.2)
        else:
            s.poly(t, fill=DIM, stroke=LINE, sw=1, opacity=0.8)
    hp = (o[0] + d[0] * hit[0], o[1] + d[1] * hit[0])
    s.line(o[0], o[1], hp[0], hp[1], RED, 2.5)
    s.circle(o[0], o[1], 5, fill=RED)
    s.text(o[0] - 4, o[1] - 12, "レイ", 13, RED, "start")
    s.text(200, 40, "三角形を箱で囲んで階層にする", 16, FG, weight=700)

    # 右：木構造
    s.text(610, 40, "レイが通る箱だけ中を見る", 16, FG, weight=700)
    pos = {}
    root = 0
    l1 = nodes[0]["kids"]
    pos[root] = (610, 92)
    pos[l1[0]], pos[l1[1]] = (518, 172), (702, 172)
    leaves = nodes[l1[0]]["kids"] + nodes[l1[1]]["kids"]
    for k, xx in zip(leaves, (472, 564, 656, 748)):
        pos[k] = (xx, 252)
    names = {root: "全体", l1[0]: "左半分", l1[1]: "右半分"}
    for i, (x, y) in pos.items():
        for k in nodes[i]["kids"]:
            kx, ky = pos[k]
            s.line(x, y + 18, kx, ky - 18, LINE, 1.5)
    for i, (x, y) in pos.items():
        st = status.get(i, "none")
        dep = nodes[i]["depth"]
        name = names.get(i, "三角形×4")
        if st == "visit":
            s.rect(x - 42, y - 18, 84, 36, fill=PANEL, stroke=col_of[dep], sw=2, rx=6)
            s.text(x, y + 5, name, 14, FG)
        elif st == "skip":
            s.rect(x - 42, y - 18, 84, 36, stroke=SUB, sw=1.5, rx=6, dash="5 4", opacity=0.7)
            s.text(x, y + 5, name, 14, SUB)
        else:
            s.rect(x - 42, y - 18, 84, 36, stroke=SUB, sw=1.2, rx=6, dash="5 4", opacity=0.35)
            s.text(x, y + 5, name, 14, SUB, opacity=0.45)
    s.rect(476, 296, 26, 16, fill=PANEL, stroke=MINT, sw=2, rx=4)
    s.text(510, 309, "中を調べた", 13, FG, "start")
    s.rect(620, 296, 26, 16, stroke=SUB, sw=1.5, rx=4, dash="4 3")
    s.text(654, 309, "スキップ（中は計算しない）", 13, SUB, "start")
    s.rich(610, 350, [("16枚のうち  ", 15, SUB, 400), (str(len(tested)), 26, AMBER, 700), (" 枚だけ計算", 15, SUB, 400)])
    s.text(400, 388, "長い軸で半分に分割 ／ 葉は三角形4つまで ／ 頂点が動いたら箱だけ更新", 13, SUB)
    return s, len(tested)


# ---------------------------------------------------------------- p.9 カバーAI
def fig09():
    s = Svg(680, 430)
    E, P = (220, 220), (600, 190)
    boards = [board_poly((340, 165), (340, 270), 10),
              board_poly((78, 106), (236, 68), 10),
              board_poly((150, 352), (300, 352), 10)]
    R, BACK = 200, 20   # 20m 先まで / 壁の1.5m手前（図では見やすく少し広げている）
    cands = []
    for i in range(16):
        a = i * 2 * math.pi / 16
        d = (math.cos(a), math.sin(a))
        t = None
        for b in boards:
            tt = ray_poly(E, d, b)
            if tt is not None and tt <= R and (t is None or tt < t):
                t = tt
        if t is None:
            s.line(E[0], E[1], E[0] + d[0] * R, E[1] + d[1] * R, LINE, 1.2, dash="4 4", opacity=0.7)
        else:
            h = (E[0] + d[0] * t, E[1] + d[1] * t)
            s.line(E[0], E[1], h[0], h[1], BLUE, 1.6, opacity=0.85)
            c = (h[0] - d[0] * BACK, h[1] - d[1] * BACK)
            safe = any(seg_hits_poly(c, P, b) for b in boards)
            cands.append((math.dist(c, E), c, safe))
    for b in boards:
        draw_board(s, b)
    safe_c = sorted([c for c in cands if c[2]])
    expo_c = sorted([c for c in cands if not c[2]])
    best = safe_c[0][1]
    # 視線（1つずつだけ描く）
    if expo_c:
        ec = expo_c[0][1]
        s.line(P[0], P[1], ec[0], ec[1], RED, 1.5, dash="5 5", opacity=0.8)
    L = math.dist(P, best)
    dd = ((best[0] - P[0]) / L, (best[1] - P[1]) / L)
    tb = min(ray_poly(P, dd, b) or 1e9 for b in boards)
    s.line(P[0], P[1], P[0] + dd[0] * tb, P[1] + dd[1] * tb, SUB, 1.5, dash="5 5")
    s.text((P[0] + P[0] + dd[0] * tb) / 2, (P[1] + P[1] + dd[1] * tb) / 2 - 12, "壁で遮られる", 12, SUB)
    if expo_c:
        s.text((P[0] + ec[0]) / 2 + 14, (P[1] + ec[1]) / 2 + 22, "見えている", 12, RED, "start")
    for _, c, safe in cands:
        if safe:
            s.circle(c[0], c[1], 6, fill=MINT)
        else:
            s.cross(c[0], c[1], 5)
    s.circle(best[0], best[1], 13, stroke=MINT, sw=2)
    s.line(E[0] + 16, E[1] + 2, best[0] - 15, best[1], MINT, 2.5, dash="6 4", end="a-mint")
    s.circle(E[0], E[1], 14, fill=RED, stroke=RED_L, sw=2)
    s.text(E[0] - 22, E[1] + 5, "敵", 15, FG, "end")
    s.circle(P[0], P[1], 14, fill=BLUE, stroke=BLUE_L, sw=2)
    s.text(P[0], P[1] + 36, "プレイヤー", 15, FG)
    s.text(20, 26, "レイ16本・最大20m・当たった壁の1.5m手前を候補にする", 13, SUB, "start")
    lx, ly = 30, 408
    s.line(lx, ly - 4, lx + 30, ly - 4, BLUE, 1.6)
    s.text(lx + 38, ly, "レイ", 13, FG, "start")
    s.circle(lx + 110, ly - 4, 6, fill=MINT)
    s.text(lx + 122, ly, "隠れられる", 13, FG, "start")
    s.cross(lx + 222, ly - 4, 5)
    s.text(lx + 234, ly, "見えてしまう", 13, FG, "start")
    s.rect(lx + 340, ly - 10, 22, 12, fill=WOOD, stroke=WOOD_S, sw=1.5)
    s.text(lx + 370, ly, "遮蔽物", 13, FG, "start")
    s.line(lx + 440, ly - 4, lx + 470, ly - 4, MINT, 2.5, dash="6 4")
    s.text(lx + 478, ly, "一番近い安全な場所へ", 13, FG, "start")
    return s, len(cands), len(safe_c)


# ---------------------------------------------------------------- p.10 NavMesh / A*
def fig10():
    s = Svg(680, 430)
    cols, rows, cell = 27, 15, 23
    ox, oy = 30, 50
    px_per_m = cell / 0.5
    agent_r = 0.85 * px_per_m

    def npos(i, j):
        return (ox + i * cell, oy + j * cell)

    walls = []

    def wall(ci, cj, wm, hm):
        cx, cy = npos(ci, cj)
        w, h = wm * px_per_m, hm * px_per_m
        walls.append((cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2))

    wall(11, 7.5, 1.0, 5.0)
    wall(19, 4.5, 3.5, 0.8)

    def walkable(p):
        for (x0, y0, x1, y1) in walls:
            dx = max(x0 - p[0], 0, p[0] - x1)
            dy = max(y0 - p[1], 0, p[1] - y1)
            if math.hypot(dx, dy) < agent_r:
                return False
        return True

    W = [[walkable(npos(i, j)) for j in range(rows)] for i in range(cols)]

    for i in range(cols):
        for j in range(rows):
            x, y = npos(i, j)
            if W[i][j]:
                s.rect(x - cell / 2 + 1, y - cell / 2 + 1, cell - 2, cell - 2, stroke=GRID, sw=1)
            else:
                s.rect(x - cell / 2 + 1, y - cell / 2 + 1, cell - 2, cell - 2, fill=DIM)
    for (x0, y0, x1, y1) in walls:
        s.rect(x0, y0, x1 - x0, y1 - y0, fill=WOOD, stroke=WOOD_S, sw=2, rx=2)

    S, G = (3, 12), (23, 1)
    assert W[S[0]][S[1]] and W[G[0]][G[1]]

    def nbrs(i, j):
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                if dx == 0 and dz == 0:
                    continue
                a, b = i + dx, j + dz
                if not (0 <= a < cols and 0 <= b < rows) or not W[a][b]:
                    continue
                if dx and dz and (not W[i + dx][j] or not W[i][j + dz]):
                    continue   # 角抜け防止（エンジンと同じ）
                yield a, b

    g = {S: 0.0}
    parent = {S: None}
    openq = [(math.dist(S, G), 0, S)]
    closed = set()
    cnt = 0
    while openq:
        _, _, cur = heapq.heappop(openq)
        if cur in closed:
            continue
        closed.add(cur)
        if cur == G:
            break
        for nb in nbrs(*cur):
            if nb in closed:
                continue
            ng = g[cur] + math.dist(cur, nb)
            if ng < g.get(nb, 1e18):
                g[nb] = ng
                parent[nb] = cur
                cnt += 1
                heapq.heappush(openq, (ng + math.dist(nb, G), cnt, nb))
    raw = []
    c = G
    while c is not None:
        raw.append(c)
        c = parent[c]
    raw.reverse()
    rawp = [npos(*c) for c in raw]

    def seg_rect(a, b, r):
        x0, y0, x1, y1 = r
        t0, t1 = 0.0, 1.0
        dx, dy = b[0] - a[0], b[1] - a[1]
        for p, q in ((-dx, a[0] - x0), (dx, x1 - a[0]), (-dy, a[1] - y0), (dy, y1 - a[1])):
            if abs(p) < 1e-12:
                if q < 0:
                    return False
            else:
                t = q / p
                if p < 0:
                    t0 = max(t0, t)
                else:
                    t1 = min(t1, t)
                if t0 > t1:
                    return False
        return True

    # 経路の間引き（エンジンと同じ：次の次の点まで壁に当たらなければ途中を飛ばす）
    path = [rawp[0]]
    for i in range(1, len(rawp) - 1):
        if any(seg_rect(path[-1], rawp[i + 1], w) for w in walls):
            path.append(rawp[i])
    path.append(rawp[-1])

    for p in rawp:
        s.circle(p[0], p[1], 2.6, fill=SUB, opacity=0.9)
    s.polyline(path, MINT, 3)
    for p in path[1:-1]:
        s.circle(p[0], p[1], 4.5, fill=MINT)
    sp, gp = npos(*S), npos(*G)
    s.circle(sp[0], sp[1], 11, fill=RED, stroke=RED_L, sw=2)
    s.text(sp[0], sp[1] + 30, "敵", 14, FG)
    s.circle(gp[0], gp[1], 11, fill=BLUE, stroke=BLUE_L, sw=2)
    s.text(gp[0] + 18, gp[1] + 5, "目的地", 14, FG, "start")
    s.text(30, 28, "0.5m ごとのマス目（上から見た図）", 13, SUB, "start")
    ly = 418
    s.rect(30, ly - 12, 14, 14, fill=DIM)
    s.text(50, ly, "通れないマス（壁 ＋ 敵の半径0.85m）", 13, FG, "start")
    s.circle(300, ly - 5, 2.6, fill=SUB)
    s.circle(310, ly - 5, 2.6, fill=SUB)
    s.text(320, ly, "A*で選んだマス", 13, FG, "start")
    s.line(440, ly - 5, 470, ly - 5, MINT, 3)
    s.text(478, ly, "直線で行ける点を間引いた経路", 13, FG, "start")
    return s, len(raw), len(path)


# ---------------------------------------------------------------- p.11 ビヘイビアツリー
def fig11():
    s = Svg(810, 380)
    # 左の列：エディタ → JSON → Factory
    s.rect(20, 34, 230, 54, fill=PANEL, stroke=MINT, sw=1.8, rx=6)
    s.text(135, 57, "ノードエディタ（ImGui）", 14, FG, weight=700)
    s.text(135, 77, "ノードをつないで編集", 12, SUB)
    s.line(135, 92, 135, 116, MINT, 2, end="a-mint", start="a-mint")
    s.text(145, 108, "保存 ／ 読み込み", 12, MINT, "start")
    s.rect(20, 120, 230, 170, fill=PANEL, stroke=LINE, sw=1.5, rx=6)
    s.text(32, 141, "test_cover_tree.json", 12, AMBER, "start", family=MONO)
    js = ['{ "Type": "SequenceNode",',
          '  "Children": [',
          '    { "Type": "DetectCoverNode",',
          '      "RayCount": 16, ... },',
          '    { "Type": "FindPathNode" },',
          '    { "Type": "FollowPathNode",',
          '      "Speed": 4.5 } ] }']
    for k, l in enumerate(js):
        s.text(32, 164 + k * 17, l, 11.5, FG if '"Type"' in l else SUB, "start", family=MONO)
    s.line(135, 294, 135, 316, SUB, 2, end="a-sub")
    s.text(145, 309, "読み込み", 12, SUB, "start")
    s.rect(20, 320, 230, 50, fill=PANEL, stroke=BLUE, sw=1.8, rx=6)
    s.text(135, 342, "BehaviorNodeFactory", 13, FG, weight=700, family=MONO)
    s.text(135, 360, "型名の文字列からノードを生成", 12, SUB)
    s.polyline([(252, 345), (282, 345), (282, 73), (436, 73)], SUB, 2, end="a-sub")
    s.text(290, 110, "ツリーを組み立て", 12, SUB, "start")
    # 右：ツリー
    s.rect(440, 50, 220, 46, fill=PANEL, stroke=FG, sw=1.8, rx=6)
    s.text(550, 70, "Sequence", 14, FG, weight=700, family=MONO)
    s.text(550, 88, "上から順に実行・失敗したら中断", 12, SUB)
    xs = [385, 550, 715]
    s.line(550, 96, 550, 122, LINE, 1.8)
    s.line(xs[0], 122, xs[2], 122, LINE, 1.8)
    kids = [("① DetectCover", "隠れ場所を探す", "レイ16本・20m", "壁の1.5m手前"),
            ("② FindPath", "A*で経路を作る", "敵の半径 0.85m", ""),
            ("③ FollowPath", "経路にそって移動", "速さ 4.5m/s", "到着判定 0.3m")]
    for x, (n, d, p1, p2) in zip(xs, kids):
        s.line(x, 122, x, 150, LINE, 1.8)
        s.rect(x - 75, 150, 150, 112, fill=PANEL, stroke=MINT, sw=1.8, rx=6)
        s.text(x, 174, n, 13, FG, weight=700, family=MONO)
        s.text(x, 200, d, 14, FG)
        s.text(x, 224, p1, 12, SUB)
        if p2:
            s.text(x, 242, p2, 12, SUB)
    s.rect(310, 300, 480, 52, fill=PANEL, stroke=LINE, sw=1.5, rx=6, dash="5 4")
    s.text(470, 323, "CoverPosition", 12, AMBER, family=MONO)
    s.text(645, 323, "CoverPath", 12, AMBER, family=MONO)
    s.text(322, 343, "Blackboard（ノード間で共有するデータ）", 12, SUB, "start")
    s.line(410, 264, 410, 300, AMBER, 2, end="a-amber")
    s.line(530, 300, 530, 266, SUB, 2, end="a-sub")
    s.line(585, 264, 585, 300, AMBER, 2, end="a-amber")
    s.line(705, 300, 705, 266, SUB, 2, end="a-sub")
    s.text(790, 373, "↓ 書き込む　↑ 読み取る", 12, SUB, "end")
    return s


# ---------------------------------------------------------------- 出力
def render(name, s):
    SVG_DIR.mkdir(exist_ok=True)
    PNG_DIR.mkdir(exist_ok=True)
    PREVIEW_DIR.mkdir(exist_ok=True)
    svg_path = SVG_DIR / f"{name}.svg"
    svg_path.write_text(s.svg(), encoding="utf-8")
    png_path = PNG_DIR / f"{name}.png"
    subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                    "--default-background-color=00000000", "--force-device-scale-factor=2",
                    f"--window-size={s.w},{s.h}", f"--screenshot={png_path}", svg_path.as_uri()],
                   check=True, capture_output=True)
    img = Image.open(png_path).convert("RGBA")
    bg = Image.new("RGBA", img.size, BG)
    bg.alpha_composite(img)
    bg.convert("RGB").save(PREVIEW_DIR / f"{name}.png")
    corner_alpha = img.getpixel((0, 0))[3]
    print(f"{name}: {img.size[0]}x{img.size[1]}  corner alpha={corner_alpha}")


if __name__ == "__main__":
    render("fig03_overview", fig03())
    render("fig04_ccd_problem", fig04())
    render("fig05_ccd_solution", fig05())
    render("fig06_fence_obb", fig06())
    s7, n_all, n_hash = fig07()
    render("fig07_spatial_hash", s7)
    s8, n_tested = fig08()
    render("fig08_bvh", s8)
    s9, n_c, n_safe = fig09()
    render("fig09_cover_ai", s9)
    s10, n_raw, n_path = fig10()
    render("fig10_navmesh_astar", s10)
    render("fig11_behavior_tree", fig11())
    print(f"pairs all={n_all} hash={n_hash} / bvh tested={n_tested} / cover cands={n_c} safe={n_safe} / astar raw={n_raw} smoothed={n_path}")
