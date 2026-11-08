"""What CoD2 Carentan's collision has near a point: brushes (material,
contents, box, angled planes) and collision triangles (material, slope),
from the user's own CoD2 (tools/cod2import/cod2collision.read). For finding
why bots stick somewhere.

usage: python -I probe.py <x> <y> <z> [radius 96] [map mp_carentan]"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "cod2import"))
import cod2collision  # noqa: E402
import cod2import  # noqa: E402

MAIN = r"C:/Program Files (x86)/Activision/Call of Duty 2/main"


def main():
    x, y, z = (float(a) for a in sys.argv[1:4])
    r = float(sys.argv[4]) if len(sys.argv) > 4 else 96.0
    name = sys.argv[5] if len(sys.argv) > 5 else "mp_carentan"
    d = cod2import.Archives(MAIN).read(f"maps/mp/{name}.d3dbsp")
    materials, brushes, terrain, _world = cod2collision.read(d)
    lo, hi = (x - r, y - r, z - r), (x + r, y + r, z + r)
    print(f"brushes touching the box {r:.0f} around {x:.0f} {y:.0f} {z:.0f}:")
    for k, (mat, side_mats, box, angled) in enumerate(brushes):
        bl, bh = (box[0], box[2], box[4]), (box[1], box[3], box[5])
        if any(bh[a] < lo[a] or bl[a] > hi[a] for a in range(3)):
            continue
        mname, surf, cont = materials[mat]
        slopes = ",".join(f"{math.degrees(math.acos(max(-1, min(1, p[2])))):.0f}" for p in angled if abs(p[2]) > 0.01)
        print(f"  #{k} {mname} cont {cont:08X} surf {surf:08X} box {bl[0]:.0f} {bl[1]:.0f} {bl[2]:.0f} .. "
              f"{bh[0]:.0f} {bh[1]:.0f} {bh[2]:.0f}, {len(angled)} angled (tilts {slopes or '-'})")
    print("collision triangles:")
    shown = 0
    for mat, plane, pts in terrain:
        if all(p[a] < lo[a] for p in pts for a in (0,)) or all(p[0] > hi[0] for p in pts) or \
           all(p[1] < lo[1] for p in pts) or all(p[1] > hi[1] for p in pts) or \
           all(p[2] < lo[2] for p in pts) or all(p[2] > hi[2] for p in pts):
            continue
        tilt = math.degrees(math.acos(max(-1, min(1, plane[2]))))
        if shown < 40:
            print(f"  {materials[mat][0]} cont {materials[mat][2]:08X} tilt {tilt:.0f}: " +
                  " | ".join(f"{p[0]:.0f} {p[1]:.0f} {p[2]:.0f}" for p in pts))
        shown += 1
    print(f"  ({shown} triangles)")


if __name__ == "__main__":
    main()
