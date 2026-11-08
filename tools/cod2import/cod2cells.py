"""Call of Duty 2's drawn level (maps/mp/<map>.d3dbsp) as CoD3 world cells:
textures, lightmaps, materials, meshes and the cells' MLTI bundles, packed
into APKF blocks (tools/kapf/block.py) with their directory entries, from
the user's own CoD2. Game data: the output stays local.

CoD2 (after riicchhaarrd/bsp.c lump.h, checked on mp_carentan):
  soup (16): u16 material, u16 lightmap (0, 1, ...; 31: none), u32 first
  vertex, u16 vertices, u16 indices, u32 first index
  vertex (68): xyz, normal, u32 colour, uv, lightmap uv, tangent, binormal
  lightmaps (lump 1): 4 MB each: planes r, g, b (512x512 RGBA: one colour
  channel for four directions each) and a 1024x1024 shadow map; a surface's
  plain colour is each plane's four channels averaged (checked by picture:
  neutral greys and warm lamps, colour spread 2.1 against 12.5 read the
  other way)
  material: byte 0x0D sort (0x0B decal), word 0x28 technique (1 opaque,
  2 alpha test, 4 blend)
CoD3 (tools/kapf: cellmesh.py, cellmat.py, xetex.py): vertex 52 bytes
  (position, DEC3N normal x in the low bits, uv0 the surface texture, uv1 the
  lightmap, uv2 0, colour FFFFFFFF, DEC3N tangent, binormal); front faces
  counter-clockwise (vertex normals agree with (b-a)x(c-a) in Rouen's cells).
First test shaders: e_decal_simple (diffuse x lightmap), its _punchthru for
alpha test, e_trans_cullnone for blends; the overlay flag for decals.

usage: python -I cod2cells.py <cod2 main> <map> <level prefix> <out folder>
  writes <out>/cells.pkl: {blocks: [bytes], entries: [(name, path, size,
  block, index)], cells: {number: [split names]}, textures: [names]}
"""
import math
import os
import pickle
import struct
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "kapf"))
import block  # noqa: E402
import cellmat  # noqa: E402
import cellmesh  # noqa: E402
import cod2assets  # noqa: E402
import cod2collision  # noqa: E402
import cod2import  # noqa: E402
import xetex  # noqa: E402

BANK = 0x300000
BLOCK_BUDGET = 0x2C0000  # room for headers, materials and alignment in a 3 MB bank (every block takes a whole bank)
SUB_VERTS = 16384
SUBS_A_MESH = 4


def dec3n(v):
    out = 0
    for k, x in enumerate(v):
        q = max(-511, min(511, int(round(x * 511))))
        out |= (q & 0x3FF) << (10 * k)
    return out


def read(d):
    lump = cod2collision.lumps(d)
    mats = [(m[:64].split(b"\0")[0].decode("latin-1"), *struct.unpack_from("<II", m, 64))
            for m in (lump(0)[i:i + 72] for i in range(0, len(lump(0)), 72))]
    soups = cod2collision.records(lump(7), "<HHIHHI")
    verts = lump(8)
    indices = struct.unpack_from(f"<{len(lump(9)) // 2}H", lump(9))
    models = cod2collision.records(lump(35), "<6f6I")
    return mats, soups, verts, indices, lump(1), models[0]


def lightmap_texture(lm, k):
    """Lightmap k as an 8888 512x512 TEX (stored A R G B a texel, no mips).
    Alpha is the sun's visibility, as in CoD3's own lightmaps (Rouen's lm*
    alpha: white on open ground with tree-shadow blobs, 0 indoors): CoD2's
    1024x1024 sun map (lightmapSamplerSun, same lightmap uvs), 2x2 averaged."""
    plane, size = 512 * 512 * 4, 3 * 512 * 512 * 4 + 1024 * 1024
    P = [lm[k * size + c * plane:k * size + (c + 1) * plane] for c in range(3)]
    sun = lm[k * size + 3 * plane:(k + 1) * size]
    # Each plane's 4 bytes are the light from 4 directions in the surface's
    # tangent space: byte 0 along the normal, then 3 tilted 54.7 degrees
    # (CoD2's gfx_d3d_mp_x86_s.dll: basis at file 0x1938C4; the $lightmapweights
    # builder at 0x1000FE90 and its helper 0x1000FD70 give an unbumped normal
    # the weights 1, 0, 0, 0; lib/lightmap.hlsl dots coefficients with weights).
    # So CoD2 lights a flat surface with byte 0 alone. C2_LM_AVG=1: the old
    # average of all four (about 20% darker).
    average = os.environ.get("C2_LM_AVG") == "1"
    texels = bytearray(512 * 512 * 4)
    for i in range(512 * 512):
        y, x = divmod(i, 512)
        at = 2 * y * 1024 + 2 * x
        texels[4 * i] = (sun[at] + sun[at + 1] + sun[at + 1024] + sun[at + 1025]) // 4
        for c in range(3):
            q = P[c][4 * i:4 * i + 4]
            texels[4 * i + 1 + c] = (q[0] + q[1] + q[2] + q[3]) // 4 if average else q[0]
    desc, refs = xetex.tex_desc(512, 512, 0x06, 0)
    return desc, refs, xetex.tex_gpu(desc, {0: bytes(texels)})


def plain_texture(rgb, size=32):
    texels = bytes([255, *rgb]) * (size * size)
    desc, refs = xetex.tex_desc(size, size, 0x06, 0)
    return desc, refs, xetex.tex_gpu(desc, {0: texels})


def dxt_texture(raw, largest=None):
    """largest: drop the top levels until the width is at most this (CoD2's own
    next level, as its lower texture settings do)."""
    fmt, w, h, levels = cod2import.iwi_levels(raw)
    while largest and w > largest and len(levels) > 1:
        levels, w, h = levels[1:], max(1, w // 2), max(1, h // 2)
    cod3 = cod2import.IWI_DXT[fmt][1]
    desc, refs = xetex.tex_desc(w, h, cod3, len(levels) - 1)
    return desc, refs, xetex.tex_gpu(desc, {k: xetex.swap_pairs(b) for k, b in enumerate(levels)})


def tex_refs(refs, name):
    out = {}
    for off, r in refs.items():
        out[off] = ("name",) if r == "name" else ("self", r[1])
    return out


def main():
    main_dir, mapname, prefix, out = sys.argv[1:5]
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    arch = cod2import.Archives(main_dir)
    d = arch.read(f"maps/mp/{mapname}.d3dbsp")
    mats, soups, verts, indices, lm, world = read(d)
    nlm = len(lm) // (3 * 512 * 512 * 4 + 1024 * 1024)
    if os.environ.get("C2_MODELS") == "1":
        # CoD2's placed models as drawn surfaces (cod2models.py): without them their
        # clip brushes stand as invisible walls
        import cod2models
        mats, soups, verts, indices = cod2models.add_models(arch, d.decode("latin-1"), mats, soups, verts, indices, lm)

    # textures: each material's colour map, the lightmaps, a white one
    textures = {}  # cod3 name -> (desc, refs, gpu)
    material_info = {}
    image_names = {}
    for m in sorted({s[0] for s in soups}):
        name = mats[m][0]
        info = {"shader": "e_decal_simple", "overlay": 0, "tex": "c2white"}
        try:
            raw = arch.read(f"materials/{name}")
            sort, tech = raw[0x0D], struct.unpack_from("<I", raw, 0x28)[0]
            info["overlay"] = 1 if sort == 0x0B else 0
            info["shader"] = {2: "e_decal_simple_punchthru", 4: "e_trans_cullnone"}.get(tech, "e_decal_simple")
            maps = cod2assets.material_maps(raw)
            cm = maps.get("colorMap")
            if cm:
                if cm not in image_names:
                    tname = f"c2t_{len(image_names):03d}"
                    try:
                        textures[tname] = dxt_texture(arch.read(f"images/{cm}.iwi"))
                        image_names[cm] = tname
                    except (ValueError, FileNotFoundError, KeyError) as e:
                        print(f"  {name}: colour map {cm} not converted ({e}); white instead")
                        image_names[cm] = "c2white"
                info["tex"] = image_names[cm]
            if os.environ.get("C2_TEST") == "C":
                for slot, field, tag in (("normalMap", "normal", "n"), ("specularMap", "specular", "s")):
                    image = maps.get(slot, "$identitynormalmap" if field == "normal" else "$black")
                    if image:
                        if image not in image_names:
                            tname = f"c2{tag}_{len(image_names):03d}"
                            # normal/specular maps at most 512 wide: the level's archives share
                            # CoD3's 96 banks of 3 MB with the game's own (test carentan-models-ctf690:
                            # with full-size maps the zone did not fit and nothing was drawn)
                            textures[tname] = dxt_texture(arch.read(f"images/{image}.iwi"), 512)
                            image_names[image] = tname
                        info[field] = image_names[image]
                info["shader"] = {2: "e_decal_punchthru", 4: "e_trans_cullnone"}.get(tech, "e_blend")
                # the technique set's name decides where the word at 0x28 is not a plain
                # 1/2/4 (models' foliage: 0x20030000 with ambient_alphatest, drawn solid
                # on e_blend in test carentan-modelsK-ctf690): alpha-tested foliage
                # (ambient_*) cull-none, other alpha test punch-through, blends cull-none
                tname_set = cod2assets.cstr(raw, struct.unpack_from("<I", raw, 0x38)[0])
                if "alphatest" in tname_set:
                    info["shader"] = "e_trans_cullnone" if tname_set.startswith("ambient") else "e_decal_punchthru"
                elif "_blend" in tname_set and not tname_set.startswith("ambient"):
                    # 2026-11-01: blended wall paint and posters (phong_blend:
                    # mtl_caen_decal_camot, v_poster*, v_sign*) drew their see-through
                    # parts black on e_trans_cullnone (the user's black walls with
                    # "CAMO"): Rouen's own blended decals (wall stains, bullet holes)
                    # are e_decal, e_blend's nine parameters, the decal flag set
                    info["shader"] = "e_decal"
                    info["overlay"] = 1
                elif "_blend" in tname_set or "_add" in tname_set:
                    info["shader"] = "e_trans_cullnone"
                # CoD2's technique set (the name at word 0x38): effect_multiply
                # decals darken what is under them (dst * texture; the dark
                # stains: near-white DXT1, no alpha). CoD3 has no multiply
                # world shader. Made black with alpha 1 - brightness on
                # e_decal_simple (the game's own decal shader, same material
                # layout) they drew solid black (test carentan-onecell690), so
                # they are left out until a CoD3 shader that blends is found.
                tset = cod2assets.cstr(raw, struct.unpack_from("<I", raw, 0x38)[0])
                if os.environ.get("C2_SHADERS") == "eblend":
                    # test E: every material on e_blend with its colour map,
                    # as test B, to tell C's shader choice from its own maps
                    info["shader"] = "e_blend"
                elif tset == "effect_multiply":
                    info["skip"] = True
                elif "_add" in tset and not tset.startswith("ambient"):
                    # 2026-11-01: additive decals (phong_add: mtl_caen_decal_lightstain_04,
                    # nearly black DXT1, mean grey 8, 106k units of one facade) ADD to the
                    # wall in CoD2: almost nothing. Opaque here they made the facade black
                    # (the user's black house at 1580 619). CoD3 has no additive world
                    # shader: left out, as the multiply stains are.
                    info["skip"] = True
        except FileNotFoundError:
            print(f"  material {name}: no file; white, simple")
        material_info[m] = info
    textures["c2white"] = plain_texture((255, 255, 255))
    for k in range(nlm):
        textures[f"c2lm{k}"] = lightmap_texture(lm, k)
    print(f"textures: {len(textures)} ({len(image_names)} source images, {nlm} lightmaps)")
    write_cells(out, prefix, mats, soups, verts, indices, material_info, textures, nlm, world)


def write_cells(out, prefix, mats, soups, verts, indices, material_info, textures, nlm, world):
    """The level's triangles as cells and their blocks, written to <out>/cells.pkl
    (also Seelow's way in, tools/waw/seelow_cells.py, 2026-11-02): mats [(name,
    surface flags, contents)], soups CoD2's (material, lightmap, first vertex,
    vertices, indices, first index), verts CoD2's 68-byte vertices, indices
    relative to a soup's first vertex, material_info {material: shader, overlay,
    tex ...}, textures {name: (desc, refs, gpu)} with c2lm<k> for lightmap k,
    world the box (mins, maxs)."""
    # triangles into cells: a 4 x 3 grid over the world model
    lo, hi = world[0:3], world[3:6]
    cols, rows = 4, 3
    cells = defaultdict(lambda: defaultdict(list))  # cell -> (material, lightmap) -> triangles of vertex indices
    skipped = left_out = 0
    for mat, lmi, first_vertex, _vcount, icount, first_index in soups:
        name, surf, _cont = mats[mat]
        if surf & 0x4 or name.startswith("sky") or name == "$default":
            skipped += icount // 3  # CoD2's sky box: CoD3 draws its own sky
            continue
        if material_info[mat].get("skip"):
            left_out += icount // 3  # effect_multiply decals (see the materials above)
            continue
        for t in range(first_index, first_index + icount, 3):
            tri = [first_vertex + indices[t + k] for k in range(3)]
            p = [struct.unpack_from("<3f", verts, 68 * v) for v in tri]
            cx = sum(q[0] for q in p) / 3
            cy = sum(q[1] for q in p) / 3
            col = min(cols - 1, max(0, int((cx - lo[0]) / (hi[0] - lo[0]) * cols)))
            row = min(rows - 1, max(0, int((cy - lo[1]) / (hi[1] - lo[1]) * rows)))
            # C2_ONE_CELL=N: everything in cell N. In the Rouen harness our grid's
            # cells 5 and 6 (the whole town) took the numbers of Rouen's small
            # one-storey rooms (002-006, 008-011: 200-650 units across), which
            # Rouen's own visibility draws only from nearby; 000/001/007 are
            # its big outdoor cells (001: the centre, 45303 triangles, in zn01).
            one = os.environ.get("C2_ONE_CELL")
            cells[int(one) if one else row * cols + col][(mat, lmi)].append(tri)

    def vertex(v):
        x = struct.unpack_from("<3f3fI2f2f3f3f", verts, 68 * v)
        pos, nrm, _col, uv, lmuv, tan, bin_ = x[0:3], x[3:6], x[6], x[7:9], x[9:11], x[11:14], x[14:17]
        return (struct.pack(">3f", *pos) + struct.pack(">I", dec3n(nrm)) + struct.pack(">2f", *uv)
                + struct.pack(">2f", *lmuv) + struct.pack(">2f", 0.0, 0.0) + b"\xFF\xFF\xFF\xFF"
                + struct.pack(">I", dec3n(tan)) + struct.pack(">I", dec3n(bin_))), pos, nrm

    decl = None
    resources_by_cell = {}
    for cell in sorted(cells):
        subs = []  # (material key, sub dict)
        # 2026-11-01: drawn in the cell's order: solid surfaces first, then alpha
        # test, then decals, then blends. By material number alone, CoD2's painted
        # ads (v_soze, v_lyon, camot: e_decal) could come before the wall behind
        # them: blended over nothing, then hiding the wall by their depth: a black
        # facade (the user's "dark buildings")
        rank = {"e_blend": 0, "e_decal_simple": 0, "e_blend_alpha": 0, "e_decal_simple_punchthru": 1,
                "e_decal_punchthru": 1, "e_trans_cullnone": 2, "e_decal": 3}
        for (mat, lmi), tris in sorted(cells[cell].items(),
                                       key=lambda kv: (rank.get(material_info[kv[0][0]]["shader"], 0), kv[0])):
            for start in range(0, len(tris), SUB_VERTS // 3):
                chunk = tris[start:start + SUB_VERTS // 3]
                remap, vb, ib, pts = {}, bytearray(), [], []
                for tri in chunk:
                    data = [vertex(v) for v in tri]
                    a, b, c = (q[1] for q in data)
                    fn = ((b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]),
                          (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
                          (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
                    n = [sum(q[2][k] for q in data) for k in range(3)]
                    order = [0, 1, 2] if sum(fn[k] * n[k] for k in range(3)) >= 0 else [0, 2, 1]
                    for k in order:
                        v = tri[k]
                        if v not in remap:
                            remap[v] = len(remap)
                            vb += data[k][0]
                            pts.append(data[k][1])
                        ib.append(remap[v])
                plo = [min(q[k] for q in pts) for k in range(3)]
                phi = [max(q[k] for q in pts) for k in range(3)]
                centre = [(plo[k] + phi[k]) / 2 for k in range(3)]
                radius = math.dist(plo, phi) / 2
                area = 0.0
                for i in range(0, len(ib), 3):
                    A, B, C = (pts[j] for j in ib[i:i + 3])
                    u = [B[k] - A[k] for k in range(3)]
                    w = [C[k] - A[k] for k in range(3)]
                    area += 0.5 * math.sqrt((u[1] * w[2] - u[2] * w[1]) ** 2 + (u[2] * w[0] - u[0] * w[2]) ** 2
                                            + (u[0] * w[1] - u[1] * w[0]) ** 2)
                if decl is None:
                    decl = REF_DECL
                subs.append(((mat, lmi), {"sphere": (*centre, radius), "lo": (*plo, 0.0), "hi": (*phi, 0.0),
                                           "f30": math.sqrt(area / math.pi), "vcount": len(remap),
                                           "icount": len(ib), "stride": 52, "decl": decl, "vertices": bytes(vb),
                                           "indices": struct.pack(f">{len(ib)}H", *ib)}))
        resources_by_cell[cell] = subs
    print(f"sky/default triangles left out: {skipped}; multiply decal triangles left out: {left_out}")
    print(f"cells: {len(resources_by_cell)}, submeshes: {sum(len(s) for s in resources_by_cell.values())}")

    # blocks: cells (MLTI + its MATs and MESHes), then textures
    blocks, entries, cell_names = [], [], defaultdict(list)

    def flush(resources, mlti_name=None, members=None):
        data = block.build(resources)
        b = len(blocks)
        blocks.append(data)
        p = __import__("apkf").read(data)
        for f in p.files:
            kind = p.types[f["type"]]["name"]
            if kind == "TEX":
                entries.append((f"{f['name']}.xetex", f"c:\\cod2\\{f['name']}.xetex",
                                f["sizes"][1] + 0x10010, b, f["type"] << 16 | f["entry"]))
            elif kind == "MLTI":
                total = sum(x["sizes"][0] + (x["sizes"][1] if len(x["sizes"]) > 1 else 0) for x in p.files)
                entries.append((mlti_name, f"c:\\cod2\\{mlti_name}", total + 0x11000, b, f["type"] << 16 | f["entry"]))
        print(f"  block {b}: {len(data):X} bytes, {len(p.files)} files")
        if len(data) > BANK:
            raise ValueError(f"block {b} is {len(data):X} bytes: over the bank")

    for cell, subs in sorted(resources_by_cell.items()):
        number = f"{cell:03d}"
        split, i = 0, 0
        while i < len(subs):
            resources, mats_here, meshes, size = [], {}, [], 0
            while i < len(subs):
                group = subs[i:i + SUBS_A_MESH]
                need = sum(len(s["vertices"]) + len(s["indices"]) + 0x200 for _k, s in group) + 0x400
                # fewer submeshes a mesh when four would not fit one bank (Seelow's
                # grass: 5350 clumps sharing no vertices, 2026-11-02; Carentan never)
                while len(group) > 1 and need > BLOCK_BUDGET:
                    group = group[:-1]
                    need = sum(len(s["vertices"]) + len(s["indices"]) + 0x200 for _k, s in group) + 0x400
                if meshes and size + need > BLOCK_BUDGET:
                    break  # the next block (split) takes it
                i += len(group)
                mesh_name = f"c2_{number}_m{len(meshes):03d}_{split}"
                for key, _s in group:
                    if key not in mats_here:
                        mats_here[key] = f"c2m_{number}_{len(mats_here):03d}_{split}"
                tris = sum(s["icount"] // 3 for _k, s in group)
                lo3 = [min(s["lo"][k] for _k, s in group) for k in range(3)]
                hi3 = [max(s["hi"][k] for _k, s in group) for k in range(3)]
                sphere = (*[(lo3[k] + hi3[k]) / 2 for k in range(3)], math.dist(lo3, hi3) / 2)
                raw, ptrs, gpu = cellmesh.build([s for _k, s in group], tris, sphere)
                refs = {}
                for off, r in ptrs.items():
                    if r[0] == "name":
                        refs[off] = ("name",)
                    elif r[0] == "self":
                        refs[off] = ("self", r[1])
                    else:  # the k-th submesh's material
                        refs[off] = ("res", mats_here[group[r[1]][0]], 0)
                meshes.append((mesh_name, raw, refs, gpu))
                size += len(raw) + len(gpu)
            for (mat, lmi), mname in mats_here.items():
                info = material_info[mat]
                lmtex = f"c2lm{lmi}" if lmi < nlm else "c2white"
                params = [("position_light0", cellmat.INT, 0), ("cmn_lightmap_texture", cellmat.TEXTURE, lmtex),
                          ("lightmap_texture", cellmat.TEXTURE, lmtex), ("diffusemap_texture", cellmat.TEXTURE, info["tex"]),
                          ("technique", cellmat.INT, 0)]
                test = os.environ.get("C2_TEST")  # A: Rouen's shader and textures; B: Rouen's shader, our textures
                if test in ("A", "B"):
                    info = dict(info, shader="e_blend")
                    diffuse, lmtex = ("rock$br_02_wall01_c", "lm0001") if test == "A" else (info["tex"], lmtex)
                    params = [("position_light0", cellmat.INT, 0), ("cmn_lightmap_texture", cellmat.TEXTURE, lmtex),
                              ("lightmap_texture", cellmat.TEXTURE, lmtex),
                              ("normalmap_texture", cellmat.TEXTURE, "rock$br_02_wall01_n"),
                              ("normal_scale", cellmat.FLOAT, 0.5), ("spec_power", cellmat.FLOAT, 300.0),
                              ("diffusemap_texture", cellmat.TEXTURE, diffuse),
                              ("specularmap_texture", cellmat.TEXTURE, "rock$br_02_wall01_s"), ("technique", cellmat.INT, 0)]
                if test == "C" and info["shader"] not in ("e_trans_cullnone", "e_decal_simple"):
                    # Stock e_blend parameter layout; the source maps' DXT
                    # channels/mips are preserved. Numeric shader tuning is
                    # still the stock CoD3 reference pending CoD2 comparison.
                    params = [("position_light0", cellmat.INT, 0), ("cmn_lightmap_texture", cellmat.TEXTURE, lmtex),
                              ("lightmap_texture", cellmat.TEXTURE, lmtex),
                              ("normalmap_texture", cellmat.TEXTURE, info["normal"]),
                              ("normal_scale", cellmat.FLOAT, 0.5), ("spec_power", cellmat.FLOAT, 300.0),
                              ("diffusemap_texture", cellmat.TEXTURE, info["tex"]),
                              # C2_SPEC=rouen (test F): Rouen's specular map as test B, our normals
                              ("specularmap_texture", cellmat.TEXTURE,
                               "rock$br_02_wall01_s" if os.environ.get("C2_SPEC") == "rouen" else info["specular"]),
                              ("technique", cellmat.INT, 0)]
                raw, mrefs = cellmat.build(params, info["overlay"])
                refs = {}
                for off, r in mrefs.items():
                    refs[off] = {"name": ("name",), "pool": ("pool", r[1]), "fx": ("import", "FX", info["shader"]),
                                 "tex": ("import", "TEX", r[1]), "self": ("self", r[1])}[r[0]]
                resources.append(("MAT", mname, raw, refs, None))
            for mesh_name, raw, refs, gpu in meshes:
                resources.append(("MESH", mesh_name, raw, refs, gpu))
            # the bundle: MATs and MESHes by name record and place
            names = [("MAT", n) for n in mats_here.values()] + [("MESH", m[0]) for m in meshes]
            nmat, nmesh = len(mats_here), len(meshes)
            mlti = bytearray(struct.pack(">II", 2, 8) + struct.pack(">III", 0x0054414D, nmat, 0x20)
                             + struct.pack(">III", 0x4853454D, nmesh, 0x20 + 8 * nmat))
            mrefs = {4: ("self", 8), 0x10: ("self", 0x20), 0x1C: ("self", 0x20 + 8 * nmat)}
            for k, (_kind, n) in enumerate(names):
                at = 0x20 + 8 * k
                mlti += bytes(8)
                mrefs[at] = ("pool", n)
                mrefs[at + 4] = ("res", n, 0)
            split_name = f"{prefix}_{number}_SPLIT{split}"
            resources.append(("MLTI", f"{split_name.lower()}.xecell", bytes(mlti), mrefs, None))
            flush(resources, f"{split_name}.xecell")
            cell_names[cell].append(split_name)
            split += 1
    tex_resources, size = [], 0
    for name, (desc, refs, gpu) in sorted(textures.items()):
        if tex_resources and size + len(gpu) > BLOCK_BUDGET:
            flush(tex_resources)
            tex_resources, size = [], 0
        tex_resources.append(("TEX", name, desc, tex_refs(refs, name), gpu))
        size += len(gpu) + len(desc)
    if tex_resources:
        flush(tex_resources)
    with open(out / "cells.pkl", "wb") as f:
        pickle.dump({"blocks": blocks, "entries": entries, "cells": dict(cell_names),
                     "textures": sorted(textures)}, f)
    print(f"{out / 'cells.pkl'}: {len(blocks)} blocks, {len(entries)} directory entries, "
          f"{sum(len(b) for b in blocks) / 1e6:.1f} MB")


# the vertex format of Rouen's 52-byte cells (8 elements and the end), from rouen/zn01.cod block 0
REF_DECL = bytes.fromhex(
    "00000000002a23b9000000cd" "0000000c002a2187000300cd" "00000010002c23a5000500cd" "00000018002c23a5000501cd"
    "00000020002c23a5000502cd" "0000002800182886000a00cd" "0000002c002a2187000600cd" "00000030002a2187000700cd"
    "00ff0000ffffffff00000000")

if __name__ == "__main__":
    main()
