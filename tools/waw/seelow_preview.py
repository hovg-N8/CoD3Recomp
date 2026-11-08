"""Seelow from above with its own textures (2026-11-02): the check that every
world surface found its material and image (seelow_world.py, seelow_materials.py,
seelow_textures.py).

A surface's material pointer is block 4's offset of its entry in the world's
material list (list start 0x195A154, 8 bytes an entry). Each triangle is drawn
in its layer 0 image's colour at the triangle's middle uv, a blend's second
layer mixed in by the vertices' colour alpha (World at War's blend weight),
then lit by its slope.

usage: python -I seelow_preview.py <mp_seelow.zone> <materials.tsv> <textures folder> <out.png>
"""
import math
import struct
import sys
from array import array
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import seelow_world as sw   # noqa: E402

LIST_BLOCK = 0x195A154


def main():
    from PIL import Image, ImageDraw
    zone, tsv, tex_dir, out = sys.argv[1], sys.argv[2], Path(sys.argv[3]), sys.argv[4]
    z = open(zone, "rb").read()
    vstart, nv = sw.find_vertices(z)
    istart = sw.find_draw(z, nv) + 758
    sstart, ns = sw.find_surfaces(z, nv)
    mats = [line.rstrip("\n").split("\t") for line in open(tsv)]
    images = {}

    def image(name):
        if name not in images:
            p = tex_dir / f"{name.replace('#', '_')}.png"
            img = Image.open(p).convert("RGB") if p.exists() else Image.new("RGB", (1, 1), (255, 0, 255))
            images[name] = (img.load(), img.size)
        return images[name]

    def sample(name, u, v):
        px, (w, h) = image(name)
        return px[int((u % 1.0) * w) % w, int((v % 1.0) * h) % h]

    verts = [struct.unpack_from("<3f4xI2f", z, vstart + 44 * i) for i in range(nv)]
    tris, bad, used = [], 0, {}
    for i in range(ns):
        vld, fv, vc, tc, bi, _w, mat = struct.unpack_from("<iiHHiII", z, sstart + 52 * i)
        m = (((mat - 1) & 0x1FFFFFFF) - LIST_BLOCK) // 8
        if not 0 <= m < len(mats):
            bad += 1
            continue
        used[m] = used.get(m, 0) + tc
        a = array("H")
        a.frombytes(z[istart + 2 * bi:istart + 2 * (bi + 3 * tc)])
        for k in range(0, len(a), 3):
            if max(a[k], a[k + 1], a[k + 2]) < vc:
                tris.append((fv + a[k], fv + a[k + 1], fv + a[k + 2], m))
    print(f"{ns} surfaces, {bad} with a material outside the list; {len(used)} materials used")
    top = sorted(used.items(), key=lambda kv: -kv[1])[:12]
    for m, n in top:
        print(f"  {n:7} triangles  {mats[m][1]}: {mats[m][2]}")

    used_v = sorted({i for t in tris for i in t[:3]})
    xs = sorted(verts[i][0] for i in used_v)
    ys = sorted(verts[i][1] for i in used_v)
    cut = len(xs) // 200
    x0, x1, y0, y1 = xs[cut], xs[-1 - cut], ys[cut], ys[-1 - cut]
    size = 3072
    k = size / max(x1 - x0, y1 - y0)
    im = Image.new("RGB", (int((x1 - x0) * k) + 1, int((y1 - y0) * k) + 1), (20, 20, 24))
    d = ImageDraw.Draw(im)
    drawn = 0
    for a, b, c, m in sorted(tris, key=lambda t: max(verts[i][2] for i in t[:3])):
        pa, pb, pc = verts[a], verts[b], verts[c]
        if any(not (x0 - 50 <= p[0] <= x1 + 50 and y0 - 50 <= p[1] <= y1 + 50) for p in (pa, pb, pc)):
            continue
        if max(math.dist(p[:3], q[:3]) for p, q in ((pa, pb), (pb, pc), (pc, pa))) > 3000:
            continue
        layers = mats[m][2].split()
        u = (pa[4] + pb[4] + pc[4]) / 3
        v = (pa[5] + pb[5] + pc[5]) / 3
        col = sample(layers[0], u, v)
        if len(layers) > 1:
            alpha = sum((p[3] >> 24) & 255 for p in (pa, pb, pc)) / (3 * 255)
            top_col = sample(layers[1], u, v)
            col = tuple(int(x * (1 - alpha) + y * alpha) for x, y in zip(col, top_col))
        ux, uy, uz = pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]
        vx, vy, vz = pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        nl = math.sqrt(nx * nx + ny * ny + nz * nz) or 1
        if nz < 0:
            nx, ny, nz = -nx, -ny, -nz
        light = max(0.35, (0.35 * nx - 0.25 * ny + 0.9 * nz) / nl)
        col = tuple(min(255, int(x * light * 1.1)) for x in col)
        d.polygon([((p[0] - x0) * k, (y1 - p[1]) * k) for p in (pa, pb, pc)], fill=col)
        drawn += 1
    im.save(out)
    print(f"drew {drawn} triangles: {out}")


if __name__ == "__main__":
    main()
