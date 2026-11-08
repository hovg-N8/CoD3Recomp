"""Holes: places in a region where a .cgb has no floor at all under a height
(nothing a player would stand on), on a grid. For finding where players
fall through Carentan.

usage: python -I holes.py <file.cgb> <x0> <y0> <x1> <y1> [grid 32] [below 400] [above: floors over this only]"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "kapf"))
import cgb  # noqa: E402

COLLIDE = 0x1 | 0x10000
CELL = 128


def main():
    c = cgb.Cgb(Path(sys.argv[1]).read_bytes())
    x0, y0, x1, y1 = (float(a) for a in sys.argv[2:6])
    step = float(sys.argv[6]) if len(sys.argv) > 6 else 32.0
    below = float(sys.argv[7]) if len(sys.argv) > 7 else 400.0
    above = float(sys.argv[8]) if len(sys.argv) > 8 else -1e9   # floors whose top is over this only
    buckets = {}

    def add(k, lo, hi):
        for gx in range(int(lo[0] // CELL), int(hi[0] // CELL) + 1):
            for gy in range(int(lo[1] // CELL), int(hi[1] // CELL) + 1):
                buckets.setdefault((gx, gy), []).append(k)

    for k in range(c.nboxes + c.nbrushes):
        v = c.volumes[k]
        if v[0] & COLLIDE:
            add(k, [v[2 + a] - v[5 + a] for a in range(3)], [v[2 + a] + v[5 + a] for a in range(3)])
    m0 = c.nboxes + c.nbrushes
    tris = []
    for m, pk in enumerate(c.tris):
        v = c.volumes[m0 + m]
        if not v[0] & COLLIDE:
            continue
        ox, oy, oz, pf, pn = c.point_sets[m]
        pts = [cgb.point(r, (ox, oy, oz)) for r in c.points_raw[pf:pf + pn]]
        idx = c.indices[pk >> 16:(pk >> 16) + (pk & 0xFFFF)]
        for i in range(0, len(idx), 3):
            t = [pts[j] for j in idx[i:i + 3]]
            tris.append(t)
            add(-len(tris), [min(p[a] for p in t) for a in range(2)], [max(p[a] for p in t) for a in range(2)])

    def has_floor(x, y):
        for k in buckets.get((int(x // CELL), int(y // CELL)), ()):
            if k >= 0:
                v = c.volumes[k]
                if abs(x - v[2]) <= v[5] and abs(y - v[3]) <= v[6] and v[4] - v[7] < below and v[4] + v[7] > above:
                    if k < c.nboxes:
                        return True
                    pk = c.brushes[k - c.nboxes]
                    if all(nx * x + ny * y + nz * (v[4] + v[7]) <= d + 1 or nz > 0.01 or nz < -0.01
                           for nx, ny, nz, d in c.planes[pk >> 16:(pk >> 16) + (pk & 0xFFFF)]):
                        return True
            else:
                a, b, d = tris[-k - 1]
                den = (b[1] - d[1]) * (a[0] - d[0]) + (d[0] - b[0]) * (a[1] - d[1])
                if abs(den) < 1e-9:
                    continue
                l1 = ((b[1] - d[1]) * (x - d[0]) + (d[0] - b[0]) * (y - d[1])) / den
                l2 = ((d[1] - a[1]) * (x - d[0]) + (a[0] - d[0]) * (y - d[1])) / den
                if min(l1, l2, 1 - l1 - l2) >= -1e-4 and max(a[2], b[2], d[2]) > above:
                    return True
        return False

    holes = []
    y = y0
    while y <= y1:
        x = x0
        while x <= x1:
            if not has_floor(x, y):
                holes.append((x, y))
            x += step
        y += step
    print(f"{len(holes)} grid points of {int((x1 - x0) / step + 1) * int((y1 - y0) / step + 1)} with no floor")
    for h in holes[:400]:
        print(f"  {h[0]:.0f} {h[1]:.0f}")


if __name__ == "__main__":
    main()
