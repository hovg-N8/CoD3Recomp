"""Draw a .mesh (cod2import.py's) from the side and from above as a PNG, flat
shaded, each bone's triangles tinted, to check a conversion by eye.

usage: python -I meshview.py <in.mesh> <out.png> [bone names to leave out, comma separated]
"""
import struct
import sys
import zlib
from pathlib import Path


def load(path):
    d = Path(path).read_bytes()
    if d[:4] != b"C2MS":
        raise ValueError("not a .mesh")
    _, nv, nt, ns = struct.unpack_from("<IIII", d, 4)
    o = 20
    verts = []
    for _ in range(nv):
        v = struct.unpack_from("<3f3f2fI", d, o)
        verts.append(v)
        o += 36
    tris = [struct.unpack_from("<3H", d, o + 6 * i) for i in range(nt)]
    o += 6 * nt
    for _ in range(ns):
        _, _, n = struct.unpack_from("<III", d, o)
        o += 12 + n
    nb = struct.unpack_from("<I", d, o)[0]
    o += 4
    bones = []
    for _ in range(nb):
        n = struct.unpack_from("<I", d, o)[0]
        bones.append(d[o + 4:o + 4 + n].decode("latin-1"))
        o += 4 + n + 12
    return verts, tris, bones


def draw(verts, tris, axes, w, h, keep):
    img = bytearray([24, 24, 28] * (w * h))
    depth = [1e9] * (w * h)
    xs = [v[axes[0]] for v in verts]
    ys = [v[axes[1]] for v in verts]
    lo_x, hi_x, lo_y, hi_y = min(xs), max(xs), min(ys), max(ys)
    s = min((w - 20) / max(hi_x - lo_x, 1e-6), (h - 20) / max(hi_y - lo_y, 1e-6))
    light = (0.4, 0.5, 0.75)
    palette = [(200, 170, 120), (120, 180, 220), (220, 120, 120), (140, 220, 140), (220, 220, 120), (200, 140, 220)]

    def project(v):
        return (10 + (v[axes[0]] - lo_x) * s, h - 10 - (v[axes[1]] - lo_y) * s, -v[axes[2]] if len(axes) > 2 else 0)

    for t in tris:
        if any(not keep(verts[i][8]) for i in t):
            continue
        p = [project(verts[i]) for i in t]
        n = verts[t[0]][3:6]
        shade = max(0.25, abs(n[0] * light[0] + n[1] * light[1] + n[2] * light[2]))
        col = palette[verts[t[0]][8] % len(palette)]
        col = tuple(int(c * shade) for c in col)
        minx, maxx = int(max(0, min(q[0] for q in p))), int(min(w - 1, max(q[0] for q in p)))
        miny, maxy = int(max(0, min(q[1] for q in p))), int(min(h - 1, max(q[1] for q in p)))
        (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = p
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-9:
            continue
        for y in range(miny, maxy + 1):
            for x in range(minx, maxx + 1):
                a = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / den
                b = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / den
                c = 1 - a - b
                if a < 0 or b < 0 or c < 0:
                    continue
                z = a * z0 + b * z1 + c * z2
                i = y * w + x
                if z < depth[i]:
                    depth[i] = z
                    img[i * 3:i * 3 + 3] = bytes(col)
    return img


def png(path, w, h, rgb):
    def chunk(k, b):
        return struct.pack(">I", len(b)) + k + b + struct.pack(">I", zlib.crc32(k + b) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                          chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main():
    verts, tris, bones = load(sys.argv[1])
    skip = set(sys.argv[3].split(",")) if len(sys.argv) > 3 and sys.argv[3] else set()
    keep = lambda b: (bones[b] if b < len(bones) else "") not in skip
    w, h = 640, 300
    side = draw(verts, tris, (0, 2, 1), w, h, keep)   # x along, z up, seen along y
    top = draw(verts, tris, (0, 1, 2), w, h, keep)    # x along, y across, seen from above
    png(sys.argv[2], w, 2 * h, side + top)
    print(f"{sys.argv[2]}: side view above, top view below")


if __name__ == "__main__":
    main()
