import math, os

def create_pine_tree():
    positions = [] # (x, y, z)
    texcoords = [] # (u, v)
    normals = []   # (nx, ny, nz)
    faces = []     # list of 3 tuples (p_idx, t_idx, n_idx) 1-based

    def add_vert(pos, uv, norm):
        positions.append(pos)
        texcoords.append(uv)
        normals.append(norm)
        return len(positions) # 1-based

    # -------------------------------------------------------------
    # 1. TRUNK (8-sided tapered cylinder, 4 vertical segments)
    # -------------------------------------------------------------
    trunk_levels = [
        (-0.20, 0.28, 0.0),
        ( 1.30, 0.22, 0.25),
        ( 2.60, 0.17, 0.50),
        ( 3.90, 0.12, 0.75),
        ( 5.20, 0.08, 1.0)
    ]
    n_trunk = 8
    trunk_ring_indices = []

    for y, r, v in trunk_levels:
        ring = []
        for i in range(n_trunk):
            angle = i * (2.0 * math.pi / n_trunk)
            cos_a = math.cos(angle)
            sin_a = math.sin(angle)
            x = cos_a * r
            z = sin_a * r
            u = 0.02 + (i / float(n_trunk)) * 0.30
            idx = add_vert((x, y, z), (u, v), (cos_a, 0.0, sin_a))
            ring.append(idx)
        trunk_ring_indices.append(ring)

    # Build trunk quads (as 2 triangles)
    for l in range(len(trunk_levels) - 1):
        r0 = trunk_ring_indices[l]
        r1 = trunk_ring_indices[l + 1]
        for i in range(n_trunk):
            i_next = (i + 1) % n_trunk
            v0 = r0[i]
            v1 = r0[i_next]
            v2 = r1[i_next]
            v3 = r1[i]
            # Triangle 1: v0 -> v1 -> v2 (CCW from outside)
            faces.append([(v0, v0, v0), (v1, v1, v1), (v2, v2, v2)])
            # Triangle 2: v0 -> v2 -> v3
            faces.append([(v0, v0, v0), (v2, v2, v2), (v3, v3, v3)])

    # Bottom cap of trunk at Y = -0.20
    b_center = add_vert((0.0, -0.20, 0.0), (0.16, 0.0), (0.0, -1.0, 0.0))
    r_bot = trunk_ring_indices[0]
    for i in range(n_trunk):
        i_next = (i + 1) % n_trunk
        faces.append([(b_center, b_center, b_center), (r_bot[i_next], r_bot[i_next], r_bot[i_next]), (r_bot[i], r_bot[i], r_bot[i])])

    # -------------------------------------------------------------
    # 2. CANOPY TIERS (4 drooping coniferous needle skirts)
    # -------------------------------------------------------------
    tiers = [
        # (skirt_y, outer_r, inner_r, apex_y, apex_r, n_teeth, trunk_attach_y)
        (1.25, 1.85, 1.45, 3.20, 0.35, 12, 1.45),
        (2.50, 1.55, 1.20, 4.30, 0.25, 12, 2.70),
        (3.75, 1.20, 0.92, 5.30, 0.16, 10, 3.95),
        (4.85, 0.85, 0.62, 6.45, 0.00,  8, 5.05),
    ]

    for skirt_y, outer_r, inner_r, apex_y, apex_r, n_teeth, trunk_attach_y in tiers:
        skirt_verts = []
        underside_skirt_verts = []
        apex_verts = []

        total_pts = n_teeth * 2
        for i in range(total_pts):
            angle = i * (2.0 * math.pi / total_pts)
            is_tip = (i % 2 == 0)
            r = outer_r if is_tip else inner_r
            # Outer tips droop downward slightly for realistic conifer needle weight
            y = skirt_y - (0.15 if is_tip else 0.05)
            x = math.cos(angle) * r
            z = math.sin(angle) * r

            # Slope normal (angled outward and upward)
            nx = math.cos(angle) * 0.70
            ny = 0.55
            nz = math.sin(angle) * 0.70
            n_len = math.sqrt(nx*nx + ny*ny + nz*nz)
            norm = (nx/n_len, ny/n_len, nz/n_len)

            # UV coordinates on needle area [0.38, 0.98]
            u = 0.38 + 0.55 * (0.5 + 0.5 * math.cos(angle))
            v = 0.10 + 0.55 * (0.5 + 0.5 * math.sin(angle))
            idx = add_vert((x, y, z), (u, v), norm)
            skirt_verts.append(idx)

            # Underside normal (angled downward)
            und_norm = (math.cos(angle) * 0.3, -0.85, math.sin(angle) * 0.3)
            und_uv = (u, 0.92) # dark shaded shadow texture region
            und_idx = add_vert((x, y, z), und_uv, und_norm)
            underside_skirt_verts.append(und_idx)

        # Apex points
        if apex_r > 0.001:
            for i in range(total_pts):
                angle = i * (2.0 * math.pi / total_pts)
                x = math.cos(angle) * apex_r
                z = math.sin(angle) * apex_r
                y = apex_y
                norm = (math.cos(angle) * 0.65, 0.60, math.sin(angle) * 0.65)
                u = 0.65 + 0.10 * math.cos(angle)
                v = 0.40 + 0.10 * math.sin(angle)
                idx = add_vert((x, y, z), (u, v), norm)
                apex_verts.append(idx)

            # Side slope triangles connecting skirt to apex ring
            for i in range(total_pts):
                i_next = (i + 1) % total_pts
                s0 = skirt_verts[i]
                s1 = skirt_verts[i_next]
                a0 = apex_verts[i]
                a1 = apex_verts[i_next]
                faces.append([(s0, s0, s0), (s1, s1, s1), (a1, a1, a1)])
                faces.append([(s0, s0, s0), (a1, a1, a1), (a0, a0, a0)])
        else:
            # Single apex vertex for the tree top
            apex_idx = add_vert((0.0, apex_y, 0.0), (0.65, 0.40), (0.0, 1.0, 0.0))
            for i in range(total_pts):
                i_next = (i + 1) % total_pts
                s0 = skirt_verts[i]
                s1 = skirt_verts[i_next]
                faces.append([(s0, s0, s0), (s1, s1, s1), (apex_idx, apex_idx, apex_idx)])

        # Underside cap connecting skirt to trunk center
        und_center = add_vert((0.0, trunk_attach_y, 0.0), (0.65, 0.95), (0.0, -1.0, 0.0))
        for i in range(total_pts):
            i_next = (i + 1) % total_pts
            u0 = underside_skirt_verts[i]
            u1 = underside_skirt_verts[i_next]
            # Viewed from below, winding must face down
            faces.append([(und_center, und_center, und_center), (u1, u1, u1), (u0, u0, u0)])

    # Output OBJ
    lines = [
        "# Duckov Military Pine Tree Model",
        "mtllib duckov_pine_tree.mtl",
        "o DuckovPineTree",
    ]
    for p in positions:
        lines.append(f"v {p[0]:.4f} {p[1]:.4f} {p[2]:.4f}")
    for t in texcoords:
        lines.append(f"vt {t[0]:.4f} {t[1]:.4f}")
    for n in normals:
        lines.append(f"vn {n[0]:.4f} {n[1]:.4f} {n[2]:.4f}")
    
    lines.append("usemtl PineTreeMat")
    lines.append("s 1")
    for f in faces:
        lines.append(f"f {f[0][0]}/{f[0][1]}/{f[0][2]} {f[1][0]}/{f[1][1]}/{f[1][2]} {f[2][0]}/{f[2][1]}/{f[2][2]}")

    obj_content = "\n".join(lines) + "\n"

    # MTL content
    mtl_content = (
        "# Material for Duckov Pine Tree\n"
        "newmtl PineTreeMat\n"
        "Ka 0.4 0.4 0.4\n"
        "Kd 1.0 1.0 1.0\n"
        "Ks 0.1 0.1 0.1\n"
        "Ns 10.0\n"
        "illum 2\n"
        "map_Kd duckov_pine_tree.png\n"
    )

    for target_dir in ['project/Resources', 'generated/output/Release/Resources', 'generated/output/Debug/Resources', 'generated/output/Development/Resources']:
        if os.path.exists(target_dir):
            with open(os.path.join(target_dir, 'duckov_pine_tree.obj'), 'w', encoding='utf-8') as f:
                f.write(obj_content)
            with open(os.path.join(target_dir, 'duckov_pine_tree.mtl'), 'w', encoding='utf-8') as f:
                f.write(mtl_content)

    print(f"Generated duckov_pine_tree.obj ({len(positions)} verts, {len(faces)} tris) successfully.")

if __name__ == '__main__':
    create_pine_tree()
