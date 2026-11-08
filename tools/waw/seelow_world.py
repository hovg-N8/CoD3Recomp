"""World at War's Seelow world geometry read from the user's own mp_seelow.ff
(decompressed with wawimport.py ff), for CoD3's port (2026-11-02). Found by
the data's own shapes, not a full zone reader (seelow_scan.py):

  GfxWorldDraw (part):  u32 vertexCount (230609), -1 (vertices inline),
      0, u32 vertexLayerDataSize, -1, 0, u32 264836, ... ; the world's 16-bit
      indices begin 758 bytes after it (0x146CC20 in this zone: every sampled
      surface's indices then lie inside its own vertices, none at +-2 bytes)
  vertices (44 bytes): f32 xyz, f32 binormal sign (+-1), u32 colour, f32 uv[2],
      f32 lightmap uv[2], u32 packed normal, u32 packed tangent
  surfaces (52 bytes, 6973): i32 vertexLayerData, i32 firstVertex, u16
      vertexCount, u16 triCount, i32 baseIndex (in indices), u32, u32 material
      (a zone pointer), u8 lightmap, reflection probe, primary light, flags,
      f32 bounds min xyz, max xyz. A surface's indices are relative to its
      firstVertex (the world has more than 65536 vertices).

usage: python -I seelow_world.py <mp_seelow.zone> <out folder>
  writes seelow_world.obj and seelow_top.png (the world from above)
"""
import math
import struct
import sys
from array import array
from pathlib import Path

ONE, MINUS = struct.pack("<f", 1.0), struct.pack("<f", -1.0)


def find_vertices(z):
    """The longest run of 44-byte world vertices (binormal sign +-1)."""
    best = (0, 0)
    seen = set()
    for sign in (ONE, MINUS):
        p = z.find(sign)
        while p >= 0:
            s = p - 12
            if s >= 0 and s not in seen and z[s + 12:s + 16] in (ONE, MINUS):
                while s >= 44 and z[s - 32:s - 28] in (ONE, MINUS):
                    s -= 44
                e = s
                while e + 44 <= len(z) and z[e + 12:e + 16] in (ONE, MINUS):
                    seen.add(e)
                    e += 44
                if (e - s) // 44 > best[1]:
                    best = (s, (e - s) // 44)
            p = z.find(sign, p + 1)
    return best


def find_draw(z, count):
    key = struct.pack("<Ii", count, -1)
    at = z.find(key)
    if at < 0:
        raise SystemExit("the world's draw struct (its vertex count) not found")
    return at


def find_surfaces(z, nv):
    def tri(at):
        return struct.unpack_from("<iiHHi", z, at)

    def ok(at):
        vld, fv, vc, tc, bi = tri(at)
        return 0 <= fv < nv and 0 < vc <= nv - fv and 0 < tc < 40000 and 0 <= bi < 2000000

    best = (0, 0)
    at = 0
    while at < len(z) - 52 * 64:
        if ok(at):
            e = at
            while ok(e):
                e += 52
            if (e - at) // 52 > best[1]:
                best = (at, (e - at) // 52)
            at = e if e > at else at + 2
        else:
            at += 2
    return best


def main():
    zone, out = sys.argv[1], Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    z = open(zone, "rb").read()
    vstart, nv = find_vertices(z)
    print(f"vertices: {nv} at {vstart:#x}")
    draw = find_draw(z, nv)
    istart = draw + 758
    print(f"draw struct at {draw:#x}, indices from {istart:#x}")
    sstart, ns = find_surfaces(z, nv)
    print(f"surfaces: {ns} at {sstart:#x}")
    verts = [struct.unpack_from("<3f4x4x2f2f", z, vstart + 44 * i) for i in range(nv)]
    surfaces = [struct.unpack_from("<iiHHiIIBBBB", z, sstart + 52 * i) for i in range(ns)]
    tris = []
    bad = 0
    for vld, fv, vc, tc, bi, _w, mat, lm, rp, pl, fl in surfaces:
        a = array("H")
        a.frombytes(z[istart + 2 * bi:istart + 2 * (bi + 3 * tc)])
        for k in range(0, len(a), 3):
            if max(a[k], a[k + 1], a[k + 2]) >= vc:
                bad += 1
                continue
            tris.append((fv + a[k], fv + a[k + 1], fv + a[k + 2]))
    print(f"triangles: {len(tris)} ({bad} out of their surface's range)")
    with open(out / "seelow_world.obj", "w") as f:
        for x, y, zz, u, v, lu, lv in verts:
            f.write(f"v {x:.2f} {y:.2f} {zz:.2f}\n")
        for x, y, zz, u, v, lu, lv in verts:
            f.write(f"vt {u:.5f} {v:.5f}\n")
        for a, b, c in tris:
            f.write(f"f {a + 1}/{a + 1} {b + 1}/{b + 1} {c + 1}/{c + 1}\n")
    draw_top(verts, tris, out / "seelow_top.png")


def draw_top(verts, tris, path):
    from PIL import Image, ImageDraw
    # the box of the vertices triangles use, the outer half percent each way
    # left out (a few stray values stretched it to a strip)
    used = sorted({i for t in tris for i in t})
    xs = sorted(verts[i][0] for i in used)
    ys = sorted(verts[i][1] for i in used)
    cut = len(xs) // 200
    x0, x1, y0, y1 = xs[cut], xs[-1 - cut], ys[cut], ys[-1 - cut]
    print(f"the world from above: x {x0:.0f}..{x1:.0f}, y {y0:.0f}..{y1:.0f}")
    size = 2048
    k = size / max(x1 - x0, y1 - y0)
    im = Image.new("RGB", (int((x1 - x0) * k) + 1, int((y1 - y0) * k) + 1), (20, 20, 24))
    d = ImageDraw.Draw(im)
    # (the sky and any stray triangles left out of the picture: any corner
    # outside the box, or an edge over 3000)
    def inside(t):
        ps = [verts[i] for i in t]
        if any(not (x0 - 50 <= p[0] <= x1 + 50 and y0 - 50 <= p[1] <= y1 + 50) for p in ps):
            return False
        return max(math.dist(ps[a][:3], ps[b][:3]) for a, b in ((0, 1), (1, 2), (2, 0))) < 3000
    kept = [t for t in tris if inside(t)]
    print(f"drawn: {len(kept)} of {len(tris)} triangles")
    order = sorted(kept, key=lambda t: max(verts[i][2] for i in t))   # lowest first, roofs last
    for a, b, c in order:
        pa, pb, pc = verts[a], verts[b], verts[c]
        ux, uy, uz = pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]
        vx, vy, vz = pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        nl = math.sqrt(nx * nx + ny * ny + nz * nz) or 1
        if nz < 0:   # (World at War's winding faces the other way)
            nx, ny, nz = -nx, -ny, -nz
        light = max(0.15, (0.4 * nx - 0.3 * ny + 0.85 * nz) / nl)
        h = (max(pa[2], pb[2], pc[2]) + 700) / 1400
        col = (int(255 * light * (0.5 + 0.5 * h)), int(255 * light * 0.85), int(255 * light * (0.7 - 0.3 * h)))
        pts = [((p[0] - x0) * k, (y1 - p[1]) * k) for p in (pa, pb, pc)]
        d.polygon(pts, fill=col)
    im.save(path)
    print(path)


if __name__ == "__main__":
    main()
