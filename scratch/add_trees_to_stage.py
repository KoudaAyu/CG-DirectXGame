import json, os, math

# Define realistic military pine tree positions around the tactical compound
tree_configs = [
    # -------------------------------------------------------------
    # 1. SOUTH PERIMETER WOODS (Behind South Fence Z = -9.0m)
    # -------------------------------------------------------------
    {"name": "PineTree_South_01", "pos": (-10.5, 0.0, -11.5), "rotY": 0.45, "scl": (1.20, 1.40, 1.20)},
    {"name": "PineTree_South_02", "pos": ( -7.2, 0.0, -12.8), "rotY": 1.80, "scl": (1.05, 1.15, 1.05)},
    {"name": "PineTree_South_03", "pos": ( -3.8, 0.0, -11.2), "rotY": 0.90, "scl": (1.30, 1.50, 1.30)},
    {"name": "PineTree_South_04", "pos": (  0.0, 0.0, -13.0), "rotY": 2.30, "scl": (1.10, 1.30, 1.10)},
    {"name": "PineTree_South_05", "pos": (  3.5, 0.0, -11.4), "rotY": 1.20, "scl": (1.25, 1.45, 1.25)},
    {"name": "PineTree_South_06", "pos": (  7.0, 0.0, -12.5), "rotY": 2.70, "scl": (1.05, 1.20, 1.05)},
    {"name": "PineTree_South_07", "pos": ( 10.8, 0.0, -11.6), "rotY": 0.60, "scl": (1.30, 1.50, 1.30)},
    # South Deeper Forest Layer
    {"name": "PineTree_South_08", "pos": ( -8.5, 0.0, -15.5), "rotY": 1.10, "scl": (1.40, 1.60, 1.40)},
    {"name": "PineTree_South_09", "pos": ( -4.5, 0.0, -16.2), "rotY": 2.50, "scl": (1.20, 1.35, 1.20)},
    {"name": "PineTree_South_10", "pos": (  1.5, 0.0, -15.8), "rotY": 0.30, "scl": (1.35, 1.55, 1.35)},
    {"name": "PineTree_South_11", "pos": (  5.5, 0.0, -16.0), "rotY": 1.70, "scl": (1.25, 1.40, 1.25)},
    {"name": "PineTree_South_12", "pos": (  9.2, 0.0, -15.2), "rotY": 2.10, "scl": (1.40, 1.65, 1.40)},

    # -------------------------------------------------------------
    # 2. WEST PERIMETER FOREST (Outside West Fence X = -12.5m)
    # -------------------------------------------------------------
    {"name": "PineTree_West_01", "pos": (-14.5, 0.0,  -8.0), "rotY": 0.85, "scl": (1.20, 1.35, 1.20)},
    {"name": "PineTree_West_02", "pos": (-15.2, 0.0,  -3.5), "rotY": 2.10, "scl": (1.35, 1.50, 1.35)},
    {"name": "PineTree_West_03", "pos": (-14.0, 0.0,   1.5), "rotY": 1.40, "scl": (1.15, 1.25, 1.15)},
    {"name": "PineTree_West_04", "pos": (-15.8, 0.0,   6.8), "rotY": 0.50, "scl": (1.30, 1.45, 1.30)},
    {"name": "PineTree_West_05", "pos": (-14.2, 0.0,  12.0), "rotY": 2.80, "scl": (1.25, 1.40, 1.25)},
    {"name": "PineTree_West_06", "pos": (-15.5, 0.0,  18.0), "rotY": 1.90, "scl": (1.40, 1.60, 1.40)},
    {"name": "PineTree_West_07", "pos": (-14.0, 0.0,  24.0), "rotY": 0.35, "scl": (1.20, 1.30, 1.20)},

    # -------------------------------------------------------------
    # 3. EAST PERIMETER FOREST (Outside East Fence X = +12.5m)
    # -------------------------------------------------------------
    {"name": "PineTree_East_01", "pos": ( 14.5, 0.0,  -8.0), "rotY": 1.50, "scl": (1.25, 1.40, 1.25)},
    {"name": "PineTree_East_02", "pos": ( 15.5, 0.0,  -3.0), "rotY": 0.70, "scl": (1.30, 1.50, 1.30)},
    {"name": "PineTree_East_03", "pos": ( 14.2, 0.0,   2.0), "rotY": 2.40, "scl": (1.10, 1.20, 1.10)},
    {"name": "PineTree_East_04", "pos": ( 15.6, 0.0,   7.5), "rotY": 1.10, "scl": (1.35, 1.55, 1.35)},
    {"name": "PineTree_East_05", "pos": ( 14.0, 0.0,  13.0), "rotY": 2.90, "scl": (1.20, 1.35, 1.20)},
    {"name": "PineTree_East_06", "pos": ( 15.8, 0.0,  19.0), "rotY": 0.40, "scl": (1.40, 1.65, 1.40)},
    {"name": "PineTree_East_07", "pos": ( 14.5, 0.0,  25.0), "rotY": 1.80, "scl": (1.25, 1.40, 1.25)},

    # -------------------------------------------------------------
    # 4. LZ PERIMETER CORNERS (Natural edge trees inside perimeter)
    # -------------------------------------------------------------
    {"name": "PineTree_LZ_SW", "pos": (-10.2, 0.0,  -6.5), "rotY": 1.30, "scl": (1.10, 1.25, 1.10)},
    {"name": "PineTree_LZ_SE", "pos": ( 10.2, 0.0,  -6.5), "rotY": 2.00, "scl": (1.10, 1.25, 1.10)},
    {"name": "PineTree_LZ_W",  "pos": (-10.8, 0.0,   0.5), "rotY": 0.90, "scl": (1.05, 1.20, 1.05)},
    {"name": "PineTree_LZ_E",  "pos": ( 10.8, 0.0,   0.5), "rotY": 2.60, "scl": (1.05, 1.20, 1.05)},

    # -------------------------------------------------------------
    # 5. RIVER BANKS & NORTH WOODS
    # -------------------------------------------------------------
    {"name": "PineTree_River_W", "pos": (-10.0, 0.0,  27.5), "rotY": 0.80, "scl": (1.25, 1.45, 1.25)},
    {"name": "PineTree_River_E", "pos": ( 10.0, 0.0,  27.5), "rotY": 2.20, "scl": (1.25, 1.45, 1.25)},
    {"name": "PineTree_Exit_W",  "pos": ( -8.5, 0.0,  68.0), "rotY": 1.15, "scl": (1.30, 1.50, 1.30)},
    {"name": "PineTree_Exit_E",  "pos": (  8.5, 0.0,  68.0), "rotY": 2.45, "scl": (1.30, 1.50, 1.30)},
]

targets = [
    'project/Resources/stage_layout.json',
    'generated/output/Release/Resources/stage_layout.json',
    'generated/output/Debug/Resources/stage_layout.json',
    'generated/output/Development/Resources/stage_layout.json'
]

for tgt in targets:
    if not os.path.exists(tgt):
        continue
    with open(tgt, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Filter out any existing pine tree objects so we can cleanly re-add
    filtered = [obj for obj in data if not obj.get('name', '').startswith('PineTree_')]

    # Add tree objects
    for cfg in tree_configs:
        obj = {
            "name": cfg["name"],
            "type": "Obstacle",
            "modelDirectory": "Resources",
            "modelFilename": "duckov_pine_tree.obj",
            "position": {
                "x": round(cfg["pos"][0], 3),
                "y": round(cfg["pos"][1], 3),
                "z": round(cfg["pos"][2], 3)
            },
            "rotation": {
                "x": 0.0,
                "y": round(cfg["rotY"], 3),
                "z": 0.0
            },
            "scale": {
                "x": round(cfg["scl"][0], 3),
                "y": round(cfg["scl"][1], 3),
                "z": round(cfg["scl"][2], 3)
            },
            "isStatic": True
        }
        filtered.append(obj)

    with open(tgt, 'w', encoding='utf-8') as f:
        json.dump(filtered, f, indent=4, ensure_ascii=False)

    print(f"Updated {tgt}: total {len(filtered)} objects ({len(tree_configs)} pine trees).")
