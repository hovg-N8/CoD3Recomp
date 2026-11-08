"""Invisible walls that are not CoD2's: the bots' logged blocked traces
("bots: completed capsule trace ... hit 00004000", the world) checked against
CoD2 Carentan's own collision. A hit with no CoD2 brush or collision triangle
where the body touched is something else's (Rouen's leftovers, a model's).

The body is the player box from the trace's start (mins -15 -15 0, maxs 15 15
70 assumed); for a wall hit (normal mostly level) the touched face is sampled
at its heights, for a floor/ceiling hit its corners.

usage: python -I ghost_hits.py <CoD3MP.log> [more logs]"""
import math
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "cod2import"))
import cod2collision  # noqa: E402
import cod2import  # noqa: E402

MAIN = r"C:/Program Files (x86)/Activision/Call of Duty 2/main"
LINE = re.compile(r"completed capsule trace slot (\d+) from (\S+) (\S+) (\S+) to (\S+) (\S+) (\S+) fraction (\S+) "
                  r"flags (\d) (\d) normal (\S+) (\S+) (\S+) hit 00004000")
REACH = 4.0
CELL = 64


def main():
    d = cod2import.Archives(MAIN).read("maps/mp/mp_carentan.d3dbsp")
    materials, brushes, terrain, _ = cod2collision.read(d)
    solid = []
    for mat, side_mats, box, angled in brushes:
        if not cod2collision.collides(materials[mat][2]):
            continue
        solid.append(((box[0] - REACH, box[2] - REACH, box[4] - REACH), (box[1] + REACH, box[3] + REACH, box[5] + REACH), angled))
    grid = {}
    for k, (lo, hi, _) in enumerate(solid):
        for gx in range(int(lo[0] // CELL), int(hi[0] // CELL) + 1):
            for gy in range(int(lo[1] // CELL), int(hi[1] // CELL) + 1):
                if (hi[0] - lo[0]) * (hi[1] - lo[1]) < 4e6:
                    grid.setdefault((gx, gy), []).append(k)
    big = [k for k, (lo, hi, _) in enumerate(solid) if (hi[0] - lo[0]) * (hi[1] - lo[1]) >= 4e6]
    tris = {}
    for mat, plane, pts in terrain:
        if not cod2collision.collides(materials[mat][2]):
            continue
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        for gx in range(int(min(xs) // CELL), int(max(xs) // CELL) + 1):
            for gy in range(int(min(ys) // CELL), int(max(ys) // CELL) + 1):
                tris.setdefault((gx, gy), []).append((plane, pts))

    def in_brush(p, k):
        lo, hi, angled = solid[k]
        if not all(lo[a] <= p[a] <= hi[a] for a in range(3)):
            return False
        return all(n[0] * p[0] + n[1] * p[1] + n[2] * p[2] <= n[3] + REACH for n in angled)

    def near_tri(p, plane, pts):
        n = plane[:3]
        dist = sum(n[j] * (p[j] - pts[0][j]) for j in range(3))   # the plane through its first point
        if abs(dist) > REACH:
            return False
        q = [p[a] - dist * n[a] for a in range(3)]
        for i in range(3):   # inside the triangle (edges, with REACH spare)
            a, b = pts[i], pts[(i + 1) % 3]
            e = [b[j] - a[j] for j in range(3)]
            w = [q[j] - a[j] for j in range(3)]
            c = [e[1] * w[2] - e[2] * w[1], e[2] * w[0] - e[0] * w[2], e[0] * w[1] - e[1] * w[0]]
            el = math.sqrt(sum(x * x for x in e)) or 1
            if sum(c[j] * n[j] for j in range(3)) / el < -REACH:
                return False
        return True

    def cod2_has(p):
        key = (int(p[0] // CELL), int(p[1] // CELL))
        if any(in_brush(p, k) for k in list(grid.get(key, [])) + big):
            return True
        return any(near_tri(p, pl, pts) for pl, pts in tris.get(key, ()))

    total, ghosts = 0, Counter()
    examples = {}
    for log in sys.argv[1:]:
        for m in LINE.finditer(Path(log).read_text(errors="replace")):
            v = [float(x) for x in m.groups()[1:8]] + [float(x) for x in m.groups()[10:13]]
            a, b, f, n = v[0:3], v[3:6], v[6], v[7:10]
            if int(m.group(9)) or int(m.group(10)) or f >= 0.999:
                continue   # started solid, or no hit
            c = [a[i] + (b[i] - a[i]) * f for i in range(3)]
            total += 1
            if abs(n[2]) < 0.5:
                h = math.hypot(n[0], n[1]) or 1
                samples = [(c[0] - n[0] / h * 15.5, c[1] - n[1] / h * 15.5, c[2] + z) for z in range(2, 70, 6)]
                for side in (-14, 14):   # along the face too
                    samples += [(c[0] - n[0] / h * 15.5 - n[1] / h * side, c[1] - n[1] / h * 15.5 + n[0] / h * side, c[2] + z) for z in range(2, 70, 12)]
            else:
                z = c[2] - 0.5 if n[2] > 0 else c[2] + 70.5
                samples = [(c[0] + dx, c[1] + dy, z) for dx in (-15, 0, 15) for dy in (-15, 0, 15)]
            if not any(cod2_has(p) for p in samples):
                key = (int(c[0] // 32) * 32, int(c[1] // 32) * 32, int(c[2] // 32) * 32, "wall" if abs(n[2]) < 0.5 else "floor" if n[2] > 0 else "ceiling")
                ghosts[key] += 1
                examples.setdefault(key, m.group(0)[:150])
    print(f"{total} world hits checked; {sum(ghosts.values())} with no CoD2 collision where the body touched")
    for key, count in ghosts.most_common(40):
        print(f"  {count:3d} at {key[0]} {key[1]} {key[2]} ({key[3]}): {examples[key][31:]}")


if __name__ == "__main__":
    main()
