"""World at War's Seelow as CoD3 world cells (2026-11-02): its drawn world
(seelow_world.py), materials (seelow_materials.py's materials.tsv) and
lightmaps, from the user's own mp_seelow.ff and WaW install, handed to
Carentan's cell writer (tools/cod2import/cod2cells.py write_cells) in CoD2's
shapes. Game data: the output stays local.

  vertices: WaW's 44 bytes (xyz, binormal sign, colour, uv, lightmap uv,
      packed normal, packed tangent) to CoD2's 68 (xyz, normal, colour, uv,
      lightmap uv, tangent, binormal). A packed unit vector (CoD4's): bytes
      x, y, z, w; scale (w + 192) / 32385; each (b - 127) * scale.
  materials: a blend ("*a_b") is drawn with one layer a triangle: layer 1
      where the triangle's vertex colour alpha (WaW's blend weight) averages
      over half, else layer 0; a decal image is never a triangle's whole
      texture (its layer 0 instead). Named world materials: decals and ivy
      blended (overlay), foliage, fences and windows alpha-tested, sky left
      out (CoD3 draws its own).
  lightmaps: *lightmapN_secondary (A8R8G8B8, B G R A in the file) holds two
      maps stacked; a surface's lightmap uv addresses each half (checked by
      picture: the lower half is the smooth lit map, light ground with the
      buildings' shading; the upper one dark, the full height noise). The
      lower half is CoD3's lightmap colour; alpha (sun) 0 (WaW's map holds the
      sun already: 255 bleached the open ground in game). The primary
      (L8, twice as wide) is not used yet: no layout tried gave a smooth
      picture.

usage: python -I seelow_cells.py <mp_seelow.zone> <materials.tsv> <WaW main> <level prefix> <out folder>
  e.g. ... mp_rouen "D:\\Call of Duty 3 (USA, Europe)\\seelow-work\\cells"
"""
import math
import os
import struct
import sys
import zipfile
from array import array
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "cod2import"))
import seelow_world as sw   # noqa: E402
import cod2cells   # noqa: E402
import xetex   # noqa: E402

LIST_BLOCK = 0x195A154
LIGHTMAPS = ("*lightmap0_secondary", "*lightmap1_secondary")
# images with no file (R, G, B, alpha): the water (WaW's own water shader; here murky
# and see-through on a blend: opaque on a white lightmap it drew bright cyan,
# test seelow4views) and WaW's inline-pixel images
FLAT = {"water_seelow1_c": (46, 50, 36, 170), "case64blue": (46, 50, 36, 170), "global_black_c": (0, 0, 0, 255),
        "__grey__": (96, 94, 88, 255)}   # (models whose image could not be told)
# the world model's bounds (the clip map's cmodel 0): the backdrop hills and pine
# walls inside, the sky left out by its material
BOX = (-4161, -7425, -961, 14066, 11791, 5313)
UNLIT = (110, 110, 110)   # the lightmap of surfaces with none (31: water, pine walls)
NIGHT = tuple(float(x) for x in os.environ["SEELOW_NIGHT"].split(",")) if os.environ.get("SEELOW_NIGHT") else None
if NIGHT:
    UNLIT = tuple(int(v * k + 0.5) for v, k in zip(UNLIT, NIGHT))


def unpack_unit(u):
    b = u.to_bytes(4, "little")
    scale = (b[3] + 192.0) / 32385.0
    v = [(b[k] - 127.0) * scale for k in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / n for x in v]


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def convert_vertices(z, vstart, nv):
    out = bytearray(68 * nv)
    for i in range(nv):
        x, y, zz, sign, colour, u, v, lu, lv, pn, pt = struct.unpack_from("<4fI4fII", z, vstart + 44 * i)
        n = unpack_unit(pn)
        if os.environ.get("SEELOW_FLIP") == "1":   # (test: every triangle the other way round)
            n = [-c for c in n]
        t = unpack_unit(pt)
        b = [sign * c for c in cross(n, t)]
        struct.pack_into("<3f3fI2f2f3f3f", out, 68 * i, x, y, zz, *n, colour, u, v, lu, lv, *t, *b)
    return bytes(out)


def lightmap_size(z, name):
    at = z.find(name.encode() + b"\0")
    if at < 0:
        raise ValueError(f"{name} not in the zone")
    w, hh = struct.unpack_from("<2xHH", z, at + len(name) + 1)
    return w, hh // 2


# Seelow Night's lamps (the user: "there is no lighting from lamps or street lights or
# anything"): WaW's lamp light is baked in its lightmaps, and the night scale dimmed it
# with everything. Texels of the world near a lit lamp model keep most of their baked
# light: LAMP_KEEP within LAMP_NEAR of one, fading to the night scale at LAMP_FAR.
LAMP_MODELS = ("lantern_on", "lantern_hang", "lights_tinhatlamp_on", "lights_indlight", "lights_sconce2",
               "lights_hang_lamp4_off")
LAMP_NEAR, LAMP_FAR, LAMP_KEEP = 120.0, 350.0, 0.9
# (2026-11-02 evening, the user: "this night map needs any lamps it can give us", rooms
# lit more is fine): every lamp lit at night, WaW's 10 switched-off ceiling lamps too,
# and the pools at least LAMP_WARM (lightmap texels)
# even where WaW baked a dark room, reaching further
if os.environ.get("SEELOW_LAMPS_WARM", "1") == "1":
    # (150, 118, 72) within 160 to 420: the bulbs glowed, the tunnels stayed dim brown (test lamps1)
    # 220 / 520 / (235, 190, 125): "really bright", lighting the buildings' outsides
    # (the user's play, 22:30): a little less, and walls now stop it (lamp_sees)
    LAMP_NEAR, LAMP_FAR = 180.0, 450.0
    LAMP_WARM = (215, 172, 112)
else:
    LAMP_WARM = (0, 0, 0)
# The lamps' own lit parts drawn bright (GLOW_LIGHTMAP) in place of the props' flat
# grey (UNLIT, dimmed at night: the bulbs looked switched off). By day the ones WaW
# has on; at night every lamp's.
GLOW_ON = {"mc/mtl_lights_tinhathood_on", "mc/mtl_glass_v2_on"}
GLOW_NIGHT = GLOW_ON | {"mc/mtl_glass_v2_off", "mc/mtl_lights_tinhatlamp_off", "mc/mtl_lights_tinhathood_off",
                        "mc/mtl_lights_hang_lamp4_off", "mc/mtl_lights_hang_lamp4_alpha", "mc/mtl_lights_sconce2"}
GLOW_NIGHT |= {"mc/mtl_static_seelow_fireplace_off_ember"}
GLOW_LIGHTMAP = (240, 214, 160)
# Fires light the night too: the 8 fireplaces and 4 potbelly stoves, and WaW's own
# burning fires (its effects script in the zone, createfx: mp_fire_medium x 5 and the
# rest named fire); and the user's own light spots: seelow-work\my_lights.txt, one
# "x y z" a line (# comments), a pool each where the map is too dark.
FIRE_MODELS = ("static_seelow_fireplace_off", "static_berlin_stove_potbelly")
MY_LIGHTS = Path(os.environ.get("SEELOW_MY_LIGHTS", r"D:\Call of Duty 3 (USA, Europe)\seelow-work\my_lights.txt"))


def night_lights(z, draws):
    import re
    lamps = [tuple(p["origin"]) for p in draws if p["model"] in LAMP_MODELS]
    fires = [tuple(p["origin"]) for p in draws if p["model"] in FIRE_MODELS]
    at = z.find(b"createOneshotEffect")
    script = z[max(0, at - 4000):at + 400000].decode("latin-1", "replace") if at >= 0 else ""
    effects = [(float(x), float(y), float(h)) for name, x, y, h in re.findall(
        r'create\w*Effect\( "(\w*fire\w*)" \);\s*ent\.v\[ "origin" \] = \( ([-\d.e]+), ([-\d.e]+), ([-\d.e]+) \)', script)]
    # (the user's lamp models are among the draws, so among the lamps)
    mine = [p for p, model, _lean in my_lights() if model is None]
    print(f"night lights: {len(lamps)} lamps, {len(fires)} fireplaces and stoves, {len(effects)} WaW fires, "
          f"{len(mine)} pools of the user's own ({MY_LIGHTS})")
    return lamps + fires + effects + mine


def my_lights():
    """The user's own lights: "x y z" a pool of light; "x y z <lamp model>" that
    WaW lamp placed there (upright, its base at z) and lit as WaW's are;
    "x y z <lamp model> <degrees> <dx> <dy>" leaning that far toward (dx, dy)."""
    out = []
    if MY_LIGHTS.exists():
        for line in MY_LIGHTS.read_text().splitlines():
            line = line.split("#")[0].split()
            if len(line) >= 3:
                lean = tuple(float(v) for v in line[4:7]) if len(line) >= 7 else (0.0, 1.0, 0.0)
                out.append((tuple(float(v) for v in line[:3]), line[3] if len(line) > 3 else None, lean))
    return out


def lamp_axes(lean):
    """Rows forward, left, up of a lamp leaning lean[0] degrees toward (lean[1], lean[2])."""
    deg, dx, dy = lean
    n = math.hypot(dx, dy) or 1.0
    dx, dy = dx / n, dy / n
    a = math.radians(deg)
    up = (math.sin(a) * dx, math.sin(a) * dy, math.cos(a))
    forward = (math.cos(a) * dx, math.cos(a) * dy, -math.sin(a))   # toward the lean, square to up
    left = (up[1] * forward[2] - up[2] * forward[1], up[2] * forward[0] - up[0] * forward[2],
            up[0] * forward[1] - up[1] * forward[0])
    return [forward, left, up]


def place_my_lamps():
    import seelow_models
    seelow_models.EXTRA_PLACEMENTS[:] = [
        {"model": model, "origin": p, "axis": lamp_axes(lean), "scale": 1.0}
        for p, model, lean in my_lights() if model is not None]
    print(f"the user's own lamp models: {[(p['model'], p['origin']) for p in seelow_models.EXTRA_PLACEMENTS]}")


def lamp_occluders(z, lamps):
    """WaW's solid brushes (walls, floors, roofs: the clip map's, contents 1) near
    the lamps, as (box, planes) for lamp_sees, and each lamp moved out of any it
    hangs in (a ceiling lamp's origin is its mount, in the ceiling): down 4 at a
    time, 32 at most."""
    import seelow_clipmap
    mats, _terrain, brushes, _world, _report = seelow_clipmap.read(z)
    solid = [(b[2], b[3]) for b in brushes if mats[b[0]][2] & 1]
    near = [o for o in solid if any(o[0][0] - LAMP_FAR <= l[0] <= o[0][1] + LAMP_FAR and o[0][2] - LAMP_FAR <= l[1] <= o[0][3] + LAMP_FAR
                                     and o[0][4] - LAMP_FAR <= l[2] <= o[0][5] + LAMP_FAR for l in lamps)]

    def inside(p, o):
        box, planes = o
        return (all(box[2 * a] < p[a] < box[2 * a + 1] for a in range(3))
                and all(nx * p[0] + ny * p[1] + nz * p[2] < d for nx, ny, nz, d in planes))
    moved = []
    for l in lamps:
        l = list(l)
        for _ in range(8):
            if not any(inside(l, o) for o in near):
                break
            l[2] -= 4.0
        moved.append(tuple(l))
    return near, moved


def lamp_sees(lamp, p, occluders):
    """Whether the segment from a lit spot to the lamp is clear of every occluder
    (a convex brush: its box shrunk 0.5, then its own planes, n.x <= d inside),
    its first and last 2 units not counted (the spot's own wall, the lamp's mount)."""
    dx, dy, dz = lamp[0] - p[0], lamp[1] - p[1], lamp[2] - p[2]
    ln = math.sqrt(dx * dx + dy * dy + dz * dz) or 1.0
    e = 2.0 / ln
    lo_x, hi_x = min(p[0], lamp[0]), max(p[0], lamp[0])
    lo_y, hi_y = min(p[1], lamp[1]), max(p[1], lamp[1])
    lo_z, hi_z = min(p[2], lamp[2]), max(p[2], lamp[2])
    for box, planes in occluders:
        if box[1] < lo_x or box[0] > hi_x or box[3] < lo_y or box[2] > hi_y or box[5] < lo_z or box[4] > hi_z:
            continue
        t0, t1 = e, 1.0 - e
        for o, dv, a, b in ((p[0], dx, box[0] + 0.5, box[1] - 0.5), (p[1], dy, box[2] + 0.5, box[3] - 0.5),
                            (p[2], dz, box[4] + 0.5, box[5] - 0.5)):
            if abs(dv) < 1e-9:
                if o <= a or o >= b:
                    t0 = 2.0
                    break
                continue
            ta, tb = (a - o) / dv, (b - o) / dv
            if ta > tb:
                ta, tb = tb, ta
            t0, t1 = max(t0, ta), min(t1, tb)
            if t0 >= t1:
                break
        if t0 >= t1:
            continue
        for nx, ny, nz, d in planes:
            den = nx * dx + ny * dy + nz * dz
            dist = d - (nx * p[0] + ny * p[1] + nz * p[2])
            if abs(den) < 1e-9:
                if dist < 0:
                    t0 = 2.0
                    break
                continue
            t = dist / den
            if den > 0:
                t1 = min(t1, t)
            else:
                t0 = max(t0, t)
            if t0 >= t1:
                break
        if t0 < t1:
            return False
    return True


def lamp_masks(lamps, verts, soups, indices, sizes, occluders=()):
    """Per world lightmap, each texel's lamp weight (0..1), from the world triangles
    (their lightmap uvs rasterised, each texel's place by its barycentrics). A
    lamp lights a texel only from in front of its surface and in its sight
    (lamp_sees: the user, 2026-11-02, the light should stay in the lamp's room,
    not light the buildings' outsides)."""
    masks = [bytearray(w * h) for w, h in sizes]
    if not lamps:
        return masks
    reach = LAMP_FAR
    seen = {}
    for _mat, lmi, fv, _vc, count, start in soups:
        if lmi >= len(sizes):
            continue
        w, h = sizes[lmi]
        mask = masks[lmi]
        for t in range(start, start + count, 3):
            vs = [fv + indices[t + j] for j in range(3)]
            ps = [struct.unpack_from("<3f", verts, 68 * v) for v in vs]
            lo = [min(p[c] for p in ps) - reach for c in range(3)]
            hi = [max(p[c] for p in ps) + reach for c in range(3)]
            near = [l for l in lamps if all(lo[c] <= l[c] <= hi[c] for c in range(3))]
            if not near:
                continue
            ns = [struct.unpack_from("<3f", verts, 68 * v + 12) for v in vs]
            nrm = [ns[0][c] + ns[1][c] + ns[2][c] for c in range(3)]
            uv = [struct.unpack_from("<2f", verts, 68 * v + 36) for v in vs]
            tx = [(u * w - 0.5, v * h - 0.5) for u, v in uv]
            x0, x1 = max(0, int(min(p[0] for p in tx))), min(w - 1, int(max(p[0] for p in tx)) + 1)
            y0, y1 = max(0, int(min(p[1] for p in tx))), min(h - 1, int(max(p[1] for p in tx)) + 1)
            (ax, ay), (bx, by), (cx, cy) = tx
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9 or (x1 - x0) * (y1 - y0) > 65536:
                continue
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1):
                    l1 = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / den
                    l2 = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / den
                    l3 = 1 - l1 - l2
                    if min(l1, l2, l3) < -0.02:
                        continue
                    p = [l1 * ps[0][c] + l2 * ps[1][c] + l3 * ps[2][c] for c in range(3)]
                    d = reach + 1.0
                    for l in sorted(near, key=lambda l: math.dist(p, l)):
                        dl = math.dist(p, l)
                        if dl >= reach:
                            break
                        if sum(nrm[c] * (l[c] - p[c]) for c in range(3)) <= 0:
                            continue     # (the surface faces away: a wall's far side)
                        key = (l, int(p[0] // 8), int(p[1] // 8), int(p[2] // 8))
                        if key not in seen:
                            seen[key] = lamp_sees(l, p, occluders)
                        if seen[key]:
                            d = dl
                            break
                    k = 1.0 if d <= LAMP_NEAR else max(0.0, (LAMP_FAR - d) / (LAMP_FAR - LAMP_NEAR))
                    v = int(k * 255 + 0.5)
                    if v > mask[y * w + x]:
                        mask[y * w + x] = v
    return masks


def lightmap_texture(z, name, mask=None):
    at = z.find(name.encode() + b"\0")
    if at < 0:
        raise ValueError(f"{name} not in the zone")
    h = at + len(name) + 1
    w, hh, _depth, fmt, size = struct.unpack_from("<2xHHHII", z, h)
    if fmt != 21 or size != w * hh * 4:
        raise ValueError(f"{name}: format {fmt}, {w}x{hh}, {size} bytes: not the A8R8G8B8 expected")
    data = z[h + 16:h + 16 + size]
    half = data[size // 2:]                  # the lower map
    texels = bytearray(len(half))
    # A: CoD3's sun visibility, sunlight added on top. WaW's map already holds the
    # sun: with 255 (test seelow2bots, 2026-11-02) open ground was bleached white.
    # 0 by default: the baked light alone (SEELOW_SUN=0..255 to compare)
    texels[0::4] = bytes([int(os.environ.get("SEELOW_SUN", "0"))]) * (len(half) // 4)
    texels[1::4] = half[2::4]                # R
    texels[2::4] = half[1::4]                # G
    texels[3::4] = half[0::4]                # B
    # SEELOW_NIGHT="r,g,b" (Seelow Night, 2026-11-02): the baked light scaled to
    # moonlight (the world and the props' lightmaps alike)
    if NIGHT:
        for c in range(3):
            table = bytes(min(255, int(v * NIGHT[c] + 0.5)) for v in range(256))
            day = bytes(texels[1 + c::4])
            night = bytearray(day.translate(table))
            if mask is not None:
                # the lamp pools: towards LAMP_KEEP of the baked light (and LAMP_WARM at
                # least) by the texel's lamp weight
                for i, k in enumerate(mask):
                    if k:
                        f = k / 255.0
                        lit = max(day[i] * LAMP_KEEP, LAMP_WARM[c])
                        night[i] = min(255, int(day[i] * NIGHT[c] * (1 - f) + lit * f + 0.5))
            texels[1 + c::4] = bytes(night)
    desc, refs = xetex.tex_desc(w, hh // 2, 0x06, 0)
    return desc, refs, xetex.tex_gpu(desc, {0: bytes(texels)}), (w, hh // 2)


def is_decal(image):
    return "decal" in image


def flat_texture(r, g, b, a, size=32):
    """One colour with its alpha (8888, stored A R G B, as cod2cells' plain_texture)."""
    desc, refs = xetex.tex_desc(size, size, 0x06, 0)
    return desc, refs, xetex.tex_gpu(desc, {0: bytes([a, r, g, b]) * (size * size)})


ALPHA_WORDS = ("foliage", "fence", "barbed", "ivy", "pinewall", "branch", "leaf", "leaves", "needle", "grass",
               "flower", "wheat", "hedgerow", "ceiling_wood_burnt", "chainlink")
TWO_SIDED_WORDS = ("chainlink", "barbed", "leaf", "leaves", "needle", "grass")   # WaW's cull-none ones


def shader_for(name, image):
    """(shader, overlay) for a material drawn with this image: Carentan's set
    (cod2cells.py C mode, e_blend's parameters with normal and specular maps).
    e_decal_simple, used before, is CoD3's decal shader: it does not write depth,
    so whatever was drawn later covered nearer surfaces (the user's see-through
    walls, roofs and props, 2026-11-02; a house's front wall missing from outside,
    test seelowhouseout, whole when every triangle was turned round but the ground
    gone: drawing order, not facing). WaW's own cull modes (state bits, bits 14-15
    of the first word: 0x4000 none, 0x8000 back): two-sided only the chain-link and
    barbed fences, the puddle and the trees' leaves, needles and grass."""
    low = (name + " " + image).lower()
    if "water" in low:
        return "e_trans_cullnone", 0
    if is_decal(image) or ("ivy" in low and "decal" in low):
        return "e_decal", 1
    if any(w in low for w in TWO_SIDED_WORDS):
        return "e_trans_cullnone", 0
    if any(w in low for w in ALPHA_WORDS):
        return "e_decal_punchthru", 0
    return "e_blend", 0


def main():
    zone, tsv, waw_main, prefix, out = sys.argv[1:6]
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    z = open(zone, "rb").read()
    vstart, nv = sw.find_vertices(z)
    istart = sw.find_draw(z, nv) + 758
    sstart, ns = sw.find_surfaces(z, nv)
    world_mats = [line.rstrip("\n").split("\t") for line in open(tsv)]
    print(f"vertices {nv}, surfaces {ns}, materials {len(world_mats)}")
    verts = convert_vertices(z, vstart, nv)

    # the images, from WaW's archives (a later archive wins)
    index = {}
    for iwd in sorted(Path(waw_main).glob("iw_*.iwd")):
        with zipfile.ZipFile(iwd) as zf:
            for n in zf.namelist():
                low = n.lower()
                if low.startswith("images/") and low.endswith(".iwi"):
                    index[low[7:-4]] = (iwd, n)
    textures, tex_of = {}, {}

    def texture(image):
        if image not in tex_of:
            hit = index.get(image.lower())
            name = f"c2t_{len(tex_of):03d}"
            if image in FLAT:   # (no file: the water, WaW's inline-pixel images)
                textures[name] = flat_texture(*FLAT[image])
                tex_of[image] = name
                return name
            try:
                if not hit:
                    raise FileNotFoundError(image)
                with zipfile.ZipFile(hit[0]) as zf:
                    textures[name] = cod2cells.dxt_texture(zf.read(hit[1]))
            except (ValueError, FileNotFoundError) as e:
                print(f"  {image}: not converted ({e}); white instead")
                name = "c2white"
            tex_of[image] = name
        return tex_of[image]

    # CoD2-shaped materials: one a (world material, layer)
    mats, material_info, key_of = [], {}, {}

    def material(m, layer):
        if (m, layer) not in key_of:
            name, images = world_mats[m][1], world_mats[m][2].split()
            image = images[layer]
            k = len(mats)
            sky = name.startswith("wc/sky")
            mats.append((f"{name}#{layer}", 0x4 if sky else 0, 0))
            shader, overlay = shader_for(name, image)
            material_info[k] = {"shader": shader, "overlay": overlay,
                                "tex": "c2white" if sky else texture(image), "normal": "c2flatn", "specular": "c2flats"}
            key_of[(m, layer)] = k
        return key_of[(m, layer)]

    soups, indices = [], []
    out_of_box = 0
    for i in range(ns):
        _vld, fv, vc, tc, bi, _w, mat, lmi = struct.unpack_from("<iiHHiIIB", z, sstart + 52 * i)
        m = (((mat - 1) & 0x1FFFFFFF) - LIST_BLOCK) // 8
        images = world_mats[m][2].split()
        a = array("H")
        a.frombytes(z[istart + 2 * bi:istart + 2 * (bi + 3 * tc)])
        by_layer = {}
        for k in range(0, len(a), 3):
            tri = a[k:k + 3]
            if max(tri) >= vc:
                continue
            ps = [struct.unpack_from("<3f", verts, 68 * (fv + j)) for j in tri]
            if any(not (BOX[0] <= p[0] <= BOX[3] and BOX[1] <= p[1] <= BOX[4] and BOX[2] <= p[2] <= BOX[5]) for p in ps):
                out_of_box += 1
                continue
            layer = 0
            if len(images) > 1 and not is_decal(images[1]):
                alpha = sum(verts[68 * (fv + j) + 27] for j in tri) / (3 * 255)
                layer = 1 if alpha > 0.5 else 0
            by_layer.setdefault(layer, []).extend(tri)
        for layer, tris in sorted(by_layer.items()):
            soups.append((material(m, layer), min(lmi, len(LIGHTMAPS)), fv, vc, len(tris), len(indices)))
            indices.extend(tris)
    print(f"soups {len(soups)}, triangles {len(indices) // 3}, left out (outside the level) {out_of_box}")

    # the placed static models (seelow_models.py): one soup an image, after the
    # world's vertices, on the unlit lightmap (WaW lights them by its light grid)
    if os.environ.get("SEELOW_MODELS", "1") == "1":
        import seelow_models
        from collections import Counter
        report = Counter()
        if NIGHT:
            place_my_lamps()
        model_tris, _places = seelow_models.world_triangles(z, set(index), report)
        groups = {}
        glowing = GLOW_NIGHT if NIGHT else GLOW_ON
        for img, mat, tri in model_tris:
            if mat and "gobo" in mat:          # (the pines' fake shadow cards)
                continue
            groups.setdefault((img or "__grey__", mat in glowing), []).append((mat or "", tri))
        print(f"glowing lamp parts: {sum(len(v) for (_i, g), v in groups.items() if g)} triangles")
        extra = bytearray()
        first = nv
        for (img, glow), items in sorted(groups.items()):
            k = len(mats)
            name = items[0][0] or img
            mats.append((f"model {img}", 0, 0))
            shader, overlay = shader_for(name, img)
            material_info[k] = {"shader": shader, "overlay": overlay,
                                "tex": texture(img) if img != "__grey__" else texture("__grey__"),
                                "normal": "c2flatn", "specular": "c2flats"}
            # (shared vertices: three of its own a triangle made a group of four
            # submeshes 3.2 MB, over the 3 MB bank)
            start_index = len(indices)
            v0 = first
            seen = {}
            for _mat, tri in items:
                for p, n, uv in tri:
                    t = [n[1], -n[0], 0.0] if abs(n[2]) < 0.9 else [1.0, 0.0, 0.0]
                    b = cross(n, t)
                    data = struct.pack("<3f3fI2f2f3f3f", *p, *n, 0xFFFFFFFF, *uv, 0.5, 0.5, *t, *b)
                    if data not in seen:
                        seen[data] = first - v0
                        extra += data
                        first += 1
                    indices.append(seen[data])
            soups.append((k, len(LIGHTMAPS) + glow, v0, first - v0, len(indices) - start_index, start_index))
        verts = verts + bytes(extra)
        print(f"static models: {sum(len(v) for v in groups.values())} triangles in {len(groups)} images; "
              f"{dict(report)}")

    textures["c2white"] = cod2cells.plain_texture((255, 255, 255))
    masks = [None] * len(LIGHTMAPS)
    if NIGHT and os.environ.get("SEELOW_LAMPS", "1") == "1":
        import seelow_models
        lamps = night_lights(z, seelow_models.draw_placements(z))
        sizes = [lightmap_size(z, name) for name in LIGHTMAPS]
        occluders, lamps = lamp_occluders(z, lamps)
        print(f"lamp occluders: {len(occluders)} solid brushes near the lamps")
        masks = lamp_masks(lamps, verts, soups, indices, sizes, occluders)
        print(f"lamps: {len(lamps)} lit lamp models; lamp-lit texels per lightmap "
              f"{[sum(1 for k in m if k) for m in masks]}")
    for k, name in enumerate(LIGHTMAPS):
        desc, refs, gpu, size = lightmap_texture(z, name, masks[k])
        textures[f"c2lm{k}"] = (desc, refs, gpu)
        print(f"lightmap {k}: {size[0]}x{size[1]}")
    # the unlit surfaces' (WaW's 31) lightmap: grey, no sun (a white one doubled them)
    textures[f"c2lm{len(LIGHTMAPS)}"] = flat_texture(*UNLIT, 0)
    textures[f"c2lm{len(LIGHTMAPS) + 1}"] = flat_texture(*GLOW_LIGHTMAP, 0)
    # e_blend's normal and specular maps: flat for now (x 128 in both alpha and red,
    # y 128 in green, z 255 in blue: flat read either way) and black
    textures["c2flatn"] = flat_texture(128, 128, 255, 128)
    textures["c2flats"] = flat_texture(0, 0, 0, 255)
    print(f"textures: {len(textures)} ({len(tex_of)} source images, {len(LIGHTMAPS) + 1} lightmaps)")
    os.environ["C2_TEST"] = "C"   # (write_cells: e_blend's parameters, as Carentan)
    # one cell: without it the writer made many cells, 50 blocks for zn01
    # (49 free): Seelow loaded invisible (test seelow15mantle, 2026-11-02)
    os.environ.setdefault("C2_ONE_CELL", "1")
    cod2cells.write_cells(out, prefix, mats, soups, verts, indices, material_info, textures,
                          len(LIGHTMAPS) + 2, BOX)
    # the game draws a cell's _SPLIT0.._SPLIT31 only: more is geometry never seen
    import pickle
    with open(out / "cells.pkl", "rb") as f:
        splits = {c: len(v) for c, v in pickle.load(f)["cells"].items()}
    print(f"splits a cell: {splits}")
    if max(splits.values()) > 32:
        raise SystemExit(f"a cell has {max(splits.values())} splits: over the game's 32 (lower the models' LOD)")


if __name__ == "__main__":
    main()
