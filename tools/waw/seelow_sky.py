"""Seelow's own World at War sky in its CoD3 level (2026-11-02; the user:
"not Rouen's skybox ... World at War Seelow's actual skybox"; Carentan keeps
Rouen's by the user's earlier call).

WaW (mp_seelow.ff): the sky is a model, skybox_see1 (its header at zone
0x36EFDC5, its name a pointer, so wawimport.read_xmodel(start=...)): a dome
(radius 780, 128 triangles) with seelow_1_dome_bkgd (1024 DXT1), a lower cloud
ring with see1_cl_bottom_c and an upper cloud layer with see1_cl_top_c (512
DXT5 each, scrolling: their materials' uvScroll), drawn round the camera.
CoD3 (the level archive's block 17): the same plan, sky.xemesh = a dome mesh
"sky" and cloud meshes cloud_btm01 / cloud_top01, effect cdsky, textures
mp_rouen_bkgd (2048 DXT1, block 0), mp_rouen_cl_btm (512x256 DXT5, block 7),
mp_rouen_cl_sun (512x256 DXT5, block 4).

So the textures are swapped, the meshes and effect kept. Both domes paint
the sky as a fisheye (the zenith in the middle, the horizon on the circle):
by both meshes' vertices (direction against uv) the layouts are
  Rouen: uv angle = -azimuth - 75 deg; radius 0.497 at the horizon, 0.312 at
         13.4 deg up, 0.180 at 28.2, 0.090 at 50.2, 0 at the zenith
  WaW:   uv angle = azimuth + 165 deg (mirrored); radius 0.496 at 0.9 deg,
         0.343 at 13.1, 0.198 at 27.9, 0.099 at 49.9, 0 at the zenith
and WaW's painting is resampled into Rouen's layout through them. The cloud
textures tile in both games: copied, resized to 512x256. Each new texture is
written over the old one's pixels at the same size, format and levels (the
archive's structure untouched).

usage: python -I seelow_sky.py <seelow.cod (changed in place)> <WaW main>
"""
import math
import os
import struct
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "kapf"))
import apkf   # noqa: E402
import kapf   # noqa: E402
import xetex   # noqa: E402
import seelow_textures   # noqa: E402
from wawimport import decode_iwi   # noqa: E402

ROUEN_R = [(-90.0, 0.497), (-1.8, 0.497), (13.4, 0.312), (28.2, 0.180), (50.2, 0.090), (90.0, 0.0)]
WAW_R = [(-90.0, 0.496), (0.9, 0.496), (13.1, 0.343), (27.9, 0.198), (49.9, 0.099), (90.0, 0.0)]
SWAPS = {"mp_rouen_bkgd": "seelow_1_dome_bkgd", "mp_rouen_cl_btm": "see1_cl_bottom_c",
         "mp_rouen_cl_sun": "see1_cl_top_c"}


def interp(table, x, key=0, val=1):
    pts = sorted(table, key=lambda p: p[key])
    if x <= pts[0][key]:
        return pts[0][val]
    for a, b in zip(pts, pts[1:]):
        if a[key] <= x <= b[key]:
            t = (x - a[key]) / ((b[key] - a[key]) or 1)
            return a[val] + t * (b[val] - a[val])
    return pts[-1][val]


def resample_dome(src, size):
    """WaW's dome painting in Rouen's layout, size x size."""
    from PIL import Image
    s = src.convert("RGB")
    sw, sh = s.size
    px = s.load()
    out = Image.new("RGB", (size, size))
    op = out.load()
    for y in range(size):
        for x in range(size):
            u, v = (x + 0.5) / size - 0.5, (y + 0.5) / size - 0.5
            r = math.hypot(u, v)
            if r >= 0.497:
                el = -1.8
            else:
                # radius -> elevation (radius falls as it rises), the horizon the
                # last point (the table's -90 shares the rim's radius)
                el = interp(ROUEN_R[1:], r, key=1, val=0)
            az = -math.degrees(math.atan2(v, u)) - 75.0
            ph = math.radians(az + 165.0)
            rw = interp(WAW_R, el)
            su, sv = 0.5 + rw * math.cos(ph), 0.5 + rw * math.sin(ph)
            op[x, y] = px[min(sw - 1, max(0, int(su * sw))), min(sh - 1, max(0, int(sv * sh)))]
    return out


def rgb565(c):
    return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | (c[2] >> 3)


def unpack565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def colour_block(pixels):
    """DXT1 colour part (8 bytes, little-endian PC order) of 16 RGB(A) pixels:
    the box's corners along its widest channel, four colours."""
    lo = [min(p[k] for p in pixels) for k in range(3)]
    hi = [max(p[k] for p in pixels) for k in range(3)]
    c0, c1 = rgb565(hi), rgb565(lo)
    if c0 < c1:
        c0, c1 = c1, c0
    if c0 == c1:
        return struct.pack("<HHI", c0, c1, 0)
    a, b = unpack565(c0), unpack565(c1)
    pal = [a, b, tuple((2 * a[k] + b[k]) // 3 for k in range(3)), tuple((a[k] + 2 * b[k]) // 3 for k in range(3))]
    bits = 0
    for i, p in enumerate(pixels):
        k = min(range(4), key=lambda j: sum((p[q] - pal[j][q]) ** 2 for q in range(3)))
        bits |= k << (2 * i)
    return struct.pack("<HHI", c0, c1, bits)


def alpha_block(alphas):
    a0, a1 = max(alphas), min(alphas)
    if a0 == a1:
        return struct.pack("<BB", a0, a1) + bytes(6)
    pal = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    bits = 0
    for i, a in enumerate(alphas):
        k = min(range(8), key=lambda j: abs(a - pal[j]))
        bits |= k << (3 * i)
    return struct.pack("<BB", a0, a1) + bits.to_bytes(6, "little")


def encode(img, fmt):
    """PC DXT1 (0x12) or DXT5 (0x14) blocks of an image, row by row."""
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    out = bytearray()
    for by in range(0, max(h, 4), 4):
        for bx in range(0, max(w, 4), 4):
            pixels = [px[min(w - 1, bx + i), min(h - 1, by + j)] for j in range(4) for i in range(4)]
            if fmt == 0x14:
                out += alpha_block([p[3] for p in pixels])
            out += colour_block(pixels)
    return bytes(out)


def new_gpu(desc, img):
    t = xetex.describe(desc)
    fmt = t["format"]
    blocks = {}
    for lv in xetex.levels(desc):
        level = lv["level"]
        if level not in blocks:
            im = img.resize((max(1, t["width"] >> level), max(1, t["height"] >> level)))
            blocks[level] = xetex.swap_pairs(encode(im, fmt))
    return xetex.tex_gpu(desc, blocks)


# Seelow Night (2026-11-02, the user: "switch the skybox with the one from the
# night map in cod 3's mp"): Longyard's (Bravo pack) night sky painted into the
# same textures. Its own sky mesh, grafted, drew white (tests night1-3): the
# way Longyard draws it lives in its level. Its dome painting is CoD3's fisheye
# too (resized as it is); its night clouds on both cloud layers; its fog ramp
# (the same 256x8 8888) over Rouen's.
NIGHT_SWAPS = {"mp_rouen_bkgd": "chatillon_bkgdjpg", "mp_rouen_cl_btm": "falaise_sky_cloud",
               "mp_rouen_cl_sun": "falaise_sky_cloud", "fogramp": "fogramp"}
# The moon (the user: "why cant i see the moon ... from our other cod 3 night
# map"): Longyard draws it as its own quad, its sky mesh's 4-vertex submesh
# (material moon.sky, texture falaise_sky_moon 512x512 DXT5 with alpha) at
# azimuth -9.7, elevation 25.1, 662 out, about 13 degrees across. Rouen's sky
# has no such quad, so the moon is painted into the dome: each dome texel's
# direction (Rouen's layout, checked against its dome mesh's own vertices)
# cast onto Longyard's quad, the moon's colour laid over by its alpha.
MOON = "falaise_sky_moon"


def moon_quad(longyard):
    """Longyard's moon quad: [(position, uv)] of its sky mesh's 4-vertex submesh."""
    import xemesh
    for _b, p, f in xemesh.meshes(kapf.Archive(longyard)):
        if f["name"] != "sky":
            continue
        m = xemesh.Mesh(p, f)
        for sm in m.submeshes:
            if sm["vertices"] == 4:
                pos, uv = m.element(sm, 0), m.element(sm, 5)
                return [(m.read(sm, v, pos), m.read(sm, v, uv)) for v in range(4)]
    raise SystemExit(f"{longyard}: no moon quad in its sky mesh")


def paint_moon(dome, moon, quad):
    """Longyard's moon over a dome painting in Rouen's layout (RGB, square)."""
    size = dome.size[0]
    dp, mp = dome.load(), moon.load()
    mw, mh = moon.size
    # the quad as origin + s*U + t*V, s and t its uv (0..1)
    by_uv = {(round(t[0]), round(t[1])): p for p, t in quad}
    o = by_uv[(0, 0)]
    eu = [a - b for a, b in zip(by_uv[(1, 0)], o)]
    ev = [a - b for a, b in zip(by_uv[(0, 1)], o)]
    n = (eu[1] * ev[2] - eu[2] * ev[1], eu[2] * ev[0] - eu[0] * ev[2], eu[0] * ev[1] - eu[1] * ev[0])
    nd = sum(a * b for a, b in zip(n, o))
    uu, vv, uv_ = (sum(a * a for a in eu), sum(a * a for a in ev), sum(a * b for a, b in zip(eu, ev)))
    det = uu * vv - uv_ * uv_
    painted = 0
    for y in range(size):
        for x in range(size):
            u, v = (x + 0.5) / size - 0.5, (y + 0.5) / size - 0.5
            r = math.hypot(u, v)
            if r >= 0.497:
                continue
            el = math.radians(interp(ROUEN_R[1:], r, key=1, val=0))
            az = math.radians(-math.degrees(math.atan2(v, u)) - 75.0)
            d = (math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))
            dn = sum(a * b for a, b in zip(n, d))
            if dn * nd <= 0:
                continue
            hit = [c * nd / dn for c in d]
            q = [a - b for a, b in zip(hit, o)]
            qu, qv = sum(a * b for a, b in zip(q, eu)), sum(a * b for a, b in zip(q, ev))
            s, t = (vv * qu - uv_ * qv) / det, (uu * qv - uv_ * qu) / det
            if not (0 <= s < 1 and 0 <= t < 1):
                continue
            mr, mg, mb, ma = mp[min(mw - 1, int(s * mw)), min(mh - 1, int(t * mh))]
            if ma == 0:
                continue
            k = ma / 255.0
            dr, dg, db = dp[x, y][:3]
            dp[x, y] = (round(dr + (mr - dr) * k), round(dg + (mg - dg) * k), round(db + (mb - db) * k))
            painted += 1
    return painted


def night_sources(longyard):
    out = {}
    parts = kapf.decompose(kapf.Archive(longyard))
    for blk in parts["sections"][0]["blocks"]:
        p = apkf.read(blk["data"])
        for f in p.files:
            if p.types[f["type"]]["name"] == "TEX" and f["name"].lower() in list(NIGHT_SWAPS.values()) + [MOON]:
                if f["name"].lower() == "fogramp":
                    out["fogramp"] = p.gpu(f)   # (8888, the same layout as Rouen's: its bytes as they are)
                else:
                    out[f["name"].lower()] = xetex.to_image(p.cpu(f), p.gpu(f)).convert("RGBA")
    out["moon quad"] = moon_quad(longyard)
    return out


def main():
    from PIL import Image
    global SWAPS
    cod, waw_main = sys.argv[1], sys.argv[2]
    night = waw_main == "--night"
    src = {}
    if night:
        SWAPS = NIGHT_SWAPS
        src = night_sources(sys.argv[3])
    else:
        index = seelow_textures.iwd_index(waw_main)
        for name in SWAPS.values():
            iwd, entry = index[name.lower()]
            with zipfile.ZipFile(iwd) as zf:
                src[name] = decode_iwi(zf.read(entry))
    parts = kapf.decompose(kapf.Archive(cod))
    blocks = parts["sections"][0]["blocks"]
    done = 0
    for bi, blk in enumerate(blocks):
        d = blk["data"]
        if d[:4] != b"APKF":
            continue
        p = apkf.read(d)
        changed = bytearray(d)
        for f in p.files:
            if p.types[f["type"]]["name"] != "TEX" or f["name"] not in SWAPS:
                continue
            desc = p.cpu(f)
            t = xetex.describe(desc)
            image = src[SWAPS[f["name"]]]
            if isinstance(image, bytes):
                if len(image) != f["sizes"][1]:
                    raise SystemExit(f"{f['name']}: {len(image)} bytes against {f['sizes'][1]}")
                changed[f["gpu"]:f["gpu"] + len(image)] = image
                print(f"block {bi}: {f['name']} <- Longyard's, its bytes")
                done += 1
                continue
            if night:
                image = image.resize((t["width"], t["height"]), Image.BILINEAR)
                if f["name"] == "mp_rouen_bkgd" and os.environ.get("MOON", "1") == "1":
                    image = image.convert("RGB")
                    print(f"block {bi}: the moon painted into the dome ({paint_moon(image, src[MOON], src['moon quad'])} texels)")
            elif f["name"] == "mp_rouen_bkgd":
                image = resample_dome(image, 1024).resize((t["width"], t["height"]), Image.BILINEAR)
            else:
                image = image.resize((t["width"], t["height"]), Image.BILINEAR)
            gpu = new_gpu(desc, image)
            old = f["sizes"][1]
            if len(gpu) != old:
                raise SystemExit(f"{f['name']}: new pixels {len(gpu)} bytes, old {old}: not the same layout")
            changed[f["gpu"]:f["gpu"] + old] = gpu
            print(f"block {bi}: {f['name']} {t['width']}x{t['height']} format {t['format']:#x} <- {SWAPS[f['name']]}")
            done += 1
        blk["data"] = bytes(changed)
    if done != len(SWAPS):
        raise SystemExit(f"{done} of {len(SWAPS)} sky textures found")
    Path(cod).write_bytes(kapf.compose(parts))
    print(f"{cod}: Seelow's sky in")


if __name__ == "__main__":
    main()
