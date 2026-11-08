"""Read a Call of Duty 2 map (maps/mp/<name>.d3dbsp, IBSP version 4) from the
user's own CoD2 install: its drawn geometry, materials and entities (spawns,
placed models), for bringing CoD2's maps (Carentan first) into the CoD3 port.

0.5.666. The layout follows mauserzjeh/cod-asset-importer (GPL-3.0;
templates/ibsp_template_v4.bt, rust/.../assets/ibsp.rs), written again here:
  header: "IBSP", i32 version 4, then 39 lumps (u32 length, u32 offset)
  lump 0  materials: 72 bytes (name[64], i64 flags)
  lump 7  triangle soups: 16 bytes (u16 material, u16 draw order,
          u32 first vertex, u16 vertex count, u16 index count, u32 first index)
  lump 8  vertices: 68 bytes (f32 position[3], f32 normal[3], u8 colour[4],
          f32 uv[2], 32 more: lightmap uv, tangents)
  lump 9  triangles: u16 indices (3 a triangle, relative to the soup's first vertex)
  lump 37 entities: text
Collision (brushes, planes) is not read yet.

usage:
  python -I cod2map.py <cod2 main folder> <map name> <out folder>
writes <out>/<map>.mesh (cod2import's C2MS layout, one surface a material, bone 0),
<out>/<map>_entities.txt, <out>/<map>_spawns.txt (classname origin angles),
<out>/<map>_top.png (seen from above, coloured by height, team spawns marked; needs PIL).
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cod2import  # noqa: E402  (Archives, write_png)


def read_map(data):
    if data[:4] != b"IBSP":
        raise ValueError("not an IBSP map")
    version = struct.unpack_from("<i", data, 4)[0]
    if version != 4:
        raise ValueError(f"IBSP version {version}, not CoD2's 4")
    lumps = [struct.unpack_from("<II", data, 8 + 8 * i) for i in range(39)]

    def lump(i):
        length, offset = lumps[i]
        return data[offset:offset + length]

    mats = lump(0)
    materials = [mats[i:i + 64].split(b"\0")[0].decode("latin-1") for i in range(0, len(mats) - 71, 72)]
    soups_raw = lump(7)
    soups = [struct.unpack_from("<HHIHHI", soups_raw, i) for i in range(0, len(soups_raw) - 15, 16)]
    verts_raw = lump(8)
    vertices = [struct.unpack_from("<3f3f4B2f", verts_raw, i) for i in range(0, len(verts_raw) - 67, 68)]
    tris_raw = lump(9)
    indices = struct.unpack_from(f"<{len(tris_raw) // 2}H", tris_raw)
    entities = lump(37).split(b"\0")[0].decode("latin-1")
    return materials, soups, vertices, indices, entities, lumps


def parse_entities(text):
    out, cur = [], None
    for line in text.splitlines():
        line = line.strip()
        if line == "{":
            cur = {}
        elif line == "}":
            if cur is not None:
                out.append(cur)
            cur = None
        elif cur is not None and line.startswith('"'):
            parts = line.split('"')
            if len(parts) >= 4:
                cur[parts[1]] = parts[3]
    return out


def main():
    main_dir, name, out = sys.argv[1:4]
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    arch = cod2import.Archives(main_dir)
    materials, soups, vertices, indices, entities, lumps = read_map(arch.read(f"maps/mp/{name}.d3dbsp"))
    print(f"{name}: {len(materials)} materials, {len(soups)} triangle soups, {len(vertices)} vertices, {len(indices) // 3} triangles")
    print("lump sizes: " + ", ".join(f"{i}:{l}" for i, (l, _) in enumerate(lumps) if l))

    # one surface a material, in material order
    by_material = {}
    for mat, _order, first_vertex, vcount, icount, first_index in soups:
        by_material.setdefault(mat, []).append((first_vertex, vcount, first_index, icount))
    # only the vertices the triangles use (the soups' vertex counts are not
    # to be trusted), each triangle turned round as cod-asset-importer does
    # (a, c, b), as the models' are
    surfaces = []
    for mat in sorted(by_material):
        sv, st, seen = [], [], {}
        for first_vertex, _vcount, first_index, icount in by_material[mat]:
            for t in range(first_index, first_index + icount - 2, 3):
                tri = []
                for k in range(3):
                    vi = indices[t + k] + first_vertex
                    if vi >= len(vertices):
                        break
                    if vi not in seen:
                        v = vertices[vi]
                        seen[vi] = len(sv)
                        sv.append(((v[0], v[1], v[2]), (v[3], v[4], v[5]), (v[10], v[11]), 0))
                    tri.append(seen[vi])
                if len(tri) == 3:
                    st.append((tri[0], tri[2], tri[1]))
        surfaces.append((sv, st))
    names = [materials[m] if m < len(materials) else f"material{m}" for m in sorted(by_material)]
    total_v = sum(len(s[0]) for s in surfaces)
    if total_v > 65535:
        # the C2MS layout's indices are 16-bit: one .mesh a surface group instead
        print(f"{total_v} vertices: written as one .mesh a material (16-bit indices)")
        for (sv, st), mat in zip(surfaces, names):
            safe = "".join(ch if ch.isalnum() or ch in "_-" else "_" for ch in mat)
            cod2import.write_mesh(str(out / f"{name}__{safe}.mesh"), [(sv, st)], [mat], [{"name": "root", "wpos": (0.0, 0.0, 0.0)}])
    else:
        cod2import.write_mesh(str(out / f"{name}.mesh"), surfaces, names, [{"name": "root", "wpos": (0.0, 0.0, 0.0)}])
    (out / f"{name}_materials.txt").write_text("\n".join(names) + "\n")

    (out / f"{name}_entities.txt").write_text(entities)
    ents = parse_entities(entities)
    kinds = {}
    for e in ents:
        kinds[e.get("classname", "?")] = kinds.get(e.get("classname", "?"), 0) + 1
    print("entities: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items(), key=lambda kv: -kv[1])))
    spawns = [e for e in ents if "spawn" in e.get("classname", "") and e.get("classname") != "worldspawn"]
    (out / f"{name}_spawns.txt").write_text("".join(f"{e['classname']} {e.get('origin', '')} {e.get('angles', '')}\n" for e in spawns))
    models = [e for e in ents if e.get("classname") == "misc_model"]
    (out / f"{name}_models.txt").write_text("".join(f"{e.get('model', '')} {e.get('origin', '')} {e.get('angles', '')} {e.get('modelscale', '')}\n" for e in models))

    # from above (PIL): the triangles by height, sky left out, framed on the
    # spawns; team spawns white
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("no PIL: no picture from above")
        return
    sky = {m for m in sorted(by_material) if "sky" in (materials[m] if m < len(materials) else "").lower()}
    tris = []
    for mat, _o, fv, _vc, fi, ic in soups:
        if mat in sky:
            continue
        for t in range(fi, fi + ic - 2, 3):
            if max(fv + indices[t + k] for k in range(3)) < len(vertices):
                tris.append([vertices[fv + indices[t + k]] for k in range(3)])
    pts = [(float(e["origin"].split()[0]), float(e["origin"].split()[1])) for e in spawns if e.get("classname") != "worldspawn" and len(e.get("origin", "").split()) == 3]
    if not pts or not tris:
        return
    zs = [p[2] for t in tris for p in t]
    lo = (min(p[0] for p in pts) - 1200, min(p[1] for p in pts) - 1200, min(zs))
    hi = (max(p[0] for p in pts) + 1200, max(p[1] for p in pts) + 1200, lo[2] + 550)
    size = 1400
    scale = (size - 20) / max(hi[0] - lo[0], hi[1] - lo[1])
    to = lambda x, y: (10 + (x - lo[0]) * scale, size - 10 - (y - lo[1]) * scale)
    img = Image.new("RGB", (size, size), (18, 18, 24))
    dr = ImageDraw.Draw(img)
    for t in sorted(tris, key=lambda t: t[0][2] + t[1][2] + t[2][2]):
        h = min(1.0, max(0.0, ((t[0][2] + t[1][2] + t[2][2]) / 3 - lo[2]) / (hi[2] - lo[2])))
        dr.polygon([to(p[0], p[1]) for p in t], fill=(int(40 + 200 * h), int(90 + 130 * h), int(70 + 50 * (1 - h))))
    for e in spawns:
        w = e.get("origin", "").split()
        if len(w) == 3 and "tdm" in e.get("classname", ""):
            x, y = to(float(w[0]), float(w[1]))
            dr.ellipse((x - 5, y - 5, x + 5, y + 5), fill=(255, 255, 255))
    img.save(out / f"{name}_top.png")
    print(f"{out / (name + '_top.png')}: from above ({len(spawns)} spawns, {len(models)} placed models)")


if __name__ == "__main__":
    main()
