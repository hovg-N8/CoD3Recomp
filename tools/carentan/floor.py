"""Floor height under (x, y) below z in one or more .cgb files: the highest
top of a box, brush or mesh triangle met by a vertical line, at or below z.
usage: python -I floor.py <kapf dir> <x> <y> <z> <file.cgb> [more]"""
import sys
sys.path.insert(0, sys.argv[1])
import cgb

COLLIDE = 0x1 | 0x10000  # solid or player clip


def floors(c, x, y):
    out = []
    for k in range(c.nboxes + c.nbrushes):
        v = c.volumes[k]
        if not v[0] & COLLIDE:
            continue
        lo = [v[2 + a] - v[5 + a] for a in range(3)]
        hi = [v[2 + a] + v[5 + a] for a in range(3)]
        if not (lo[0] <= x <= hi[0] and lo[1] <= y <= hi[1]):
            continue
        top, bottom = hi[2], lo[2]
        if k >= c.nboxes:
            pk = c.brushes[k - c.nboxes]
            ok = True
            for nx, ny, nz, d in c.planes[pk >> 16:(pk >> 16) + (pk & 0xFFFF)]:
                rest = d - nx * x - ny * y
                if abs(nz) < 1e-6:
                    if rest < 0:
                        ok = False
                elif nz > 0:
                    top = min(top, rest / nz)
                else:
                    bottom = max(bottom, rest / nz)
            if not ok or top < bottom:
                continue
        out.append(top)
    m0 = c.nboxes + c.nbrushes
    for m, pk in enumerate(c.tris):
        v = c.volumes[m0 + m]
        if not v[0] & COLLIDE:
            continue
        ox, oy, oz, pf, pn = c.point_sets[m]
        pts = [cgb.point(r, (ox, oy, oz)) for r in c.points_raw[pf:pf + pn]]
        idx = c.indices[pk >> 16:(pk >> 16) + (pk & 0xFFFF)]
        for i in range(0, len(idx), 3):
            a, b, d = (pts[j] for j in idx[i:i + 3])
            den = (b[1] - d[1]) * (a[0] - d[0]) + (d[0] - b[0]) * (a[1] - d[1])
            if abs(den) < 1e-9:
                continue
            l1 = ((b[1] - d[1]) * (x - d[0]) + (d[0] - b[0]) * (y - d[1])) / den
            l2 = ((d[1] - a[1]) * (x - d[0]) + (a[0] - d[0]) * (y - d[1])) / den
            l3 = 1 - l1 - l2
            if min(l1, l2, l3) >= -1e-6:
                out.append(l1 * a[2] + l2 * b[2] + l3 * d[2])
    return out


def floor_below(files, x, y, z):
    hits = [t for c in files for t in floors(c, x, y) if t <= z + 18]  # a step up is fine
    return max(hits) if hits else None


if __name__ == "__main__":
    x, y, z = (float(a) for a in sys.argv[2:5])
    files = [cgb.Cgb(open(p, "rb").read()) for p in sys.argv[5:]]
    print(floor_below(files, x, y, z))
