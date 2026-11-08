"""The M2 flamethrower's place in CoD3's first-person view, through the fit
the game uses (mp_fixes.cpp MakeFlameView: the M2's mesh, in World at War's
j_gun space, put into the Thompson gun model's space), the Thompson gun
model at the arms' tag_weapon, and the arms' model space (its camera at the
origin, x forward, y left, z up: the pose dump's tag_camera is the identity).

usage: python -I flamefit.py <arms pose dump> <skeleton json> [x y]
  prints where the M2's nozzle (tag_flash, j_gun +17.7) is in the camera's
  space at rest, and with a measured screen place x y (fractions of a 16:9
  picture) the horizontal field of view that puts it there.
"""
import json
import math
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parents[1] / "cod2import"))
import retarget as rt  # noqa: E402

SCALE, PITCH = 0.532, math.radians(5.70)
GRIP = (-10.26, 0.06, -3.18)            # World at War's rear grip, in j_gun's space
OFFSET = (0.02, -4.60, -2.03)           # where it goes: the Thompson's pistol grip
TAG_FLASH = (17.71, 0.39, 0.38)         # the nozzle in j_gun's space (wawpose: 33.02-15.31 at idle)


def fit_rot():
    """F's rotation (columns: where j_gun's x, y, z go in the gun model)."""
    c, s = math.cos(PITCH), math.sin(PITCH)
    def f(d):
        rx, rz = d[0] * c - d[2] * s, d[0] * s + d[2] * c
        return [d[1], -rx, rz]
    cols = [f((1, 0, 0)), f((0, 1, 0)), f((0, 0, 1))]
    return [[cols[j][i] for j in range(3)] for i in range(3)]


def fit_point(m):
    """A point of the M2 (j_gun space) in the Thompson gun model's space."""
    r = fit_rot()
    d = [m[k] - GRIP[k] for k in range(3)]
    return [SCALE * v + o for v, o in zip(rt.mvec(r, d), OFFSET)]


def main():
    dump, skel = sys.argv[1:3]
    names = [b["name"] for b in json.load(open(skel))]
    pose = rt.read_pose_dump(dump)
    idx = {n: i for i, n in enumerate(names[:len(pose)])}
    tr_, tp = pose[idx["tag_weapon"]]
    cr, cp = pose[idx["tag_camera"]]
    tip = rt.add(rt.mvec(tr_, fit_point(TAG_FLASH)), tp)
    tip = rt.mvec(rt.tr(cr), rt.sub(tip, cp))
    print(f"the nozzle at rest, camera space: {tip[0]:.2f} ahead, {-tip[1]:.2f} right, {-tip[2]:.2f} down")
    if len(sys.argv) > 4:
        x, y = float(sys.argv[3]), float(sys.argv[4])
        aspect = 16 / 9
        tan_h = (-tip[1] / tip[0]) / (2 * x - 1)
        tan_v = (-tip[2] / tip[0]) / (2 * y - 1)
        print(f"from x: tan(h/2) {tan_h:.3f} = {2 * math.degrees(math.atan(tan_h)):.1f} degrees across; "
              f"from y: tan(v/2) {tan_v:.3f}, x {aspect:.2f} = {2 * math.degrees(math.atan(tan_v * aspect)):.1f} degrees across")


if __name__ == "__main__":
    main()
