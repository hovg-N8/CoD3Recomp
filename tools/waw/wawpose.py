"""World at War's first-person rig posed by its own animations (xanim.py):
the arms model (viewmodel_usa_marine_arms: tag_view, the shoulders, elbows,
wrists and fingers) with the gun model's bones (viewmodel_usa_flamethrower:
j_gun, tag_flash, the hose) hung from the arms' tag_weapon, as the game puts
the gun in the hands. For the flamethrower's arms in CoD3's port.

An animation's keys are each bone's local turn and place against its parent
(x, y, z, w quaternions; a bone the animation names with no keys for a part
keeps none: no turn, no offset); a bone it does not name keeps its model's
rest (local, from the model's base pose).

usage: python -I wawpose.py <arms zone> <gun zone> <anim zone> <anim> [frame]
       (prints where the main bones are in tag_view's space)
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wawimport  # noqa: E402
import xanim  # noqa: E402


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qconj(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    x, y, z, w = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qconj(q))
    return (x, y, z)


def qnorm(q):
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    return tuple(c / n for c in q)


def slerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b, d = tuple(-x for x in b), -d
    if d > 0.9995:
        return qnorm(tuple(x + (y - x) * t for x, y in zip(a, b)))
    th = math.acos(d)
    s = math.sin(th)
    wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
    return tuple(wa * x + wb * y for x, y in zip(a, b))


def skeleton(arms, gun):
    """[(name, parent index, rest local quat, rest local pos)] for the arms
    then the gun's bones (its root under the arms' tag_weapon)."""
    out = []

    def add_model(m, root_parent):
        nroot = len(m["bones"]) - len(m["parents"])
        first = len(out)
        world = []
        for i, name in enumerate(m["bones"]):
            q = qnorm(tuple(m["base"][i][0:4]))
            p = tuple(m["base"][i][4:7])
            world.append((q, p))
            parent = (first + i - m["parents"][i - nroot]) if i >= nroot else root_parent
            if i >= nroot:
                pq, pp = world[i - m["parents"][i - nroot]]
                lq = qmul(qconj(pq), q)
                lp = qrot(qconj(pq), tuple(a - b for a, b in zip(p, pp)))
            else:
                lq, lp = (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)   # a root: at its parent (the gun at tag_weapon)
            out.append((name, parent, lq, lp))

    add_model(arms, -1)
    tag_weapon = [b[0] for b in out].index("tag_weapon")
    add_model(gun, tag_weapon)
    return out


def sample(keys, frame, default):
    if not keys:
        return default
    if len(keys) == 1 or frame <= keys[0][0]:
        return keys[0][1]
    for (f0, v0), (f1, v1) in zip(keys, keys[1:]):
        if f0 <= frame <= f1:
            t = (frame - f0) / max(1, f1 - f0)
            if len(v0) == 4:
                return slerp(v0, v1, t)
            return tuple(a + (b - a) * t for a, b in zip(v0, v1))
    return keys[-1][1]


def pose(skel, anim, frame):
    """{bone: (world quat, world pos)} in tag_view's space."""
    by_name = {n: i for i, n in enumerate(anim["names"])}
    world = []
    for name, parent, lq, lp in skel:
        if name in by_name:
            # (a part the animation has no keys for keeps the model's rest:
            # with zero, tag_flash fell onto j_gun, 17.7 short of the nozzle)
            i = by_name[name]
            q = sample(anim["rot"].get(i), frame, lq)
            p = sample(anim["trans"].get(i), frame, lp)
        else:
            q, p = lq, lp
        q = qnorm(q)
        if parent < 0:
            world.append((q, p))
        else:
            pq, pp = world[parent]
            world.append((qnorm(qmul(pq, q)), tuple(a + b for a, b in zip(pp, qrot(pq, p)))))
    return {skel[i][0]: world[i] for i in range(len(skel))}


def main():
    arms_zone, gun_zone, anim_zone, anim_name = sys.argv[1:5]
    frame = float(sys.argv[5]) if len(sys.argv) > 5 else 0.0
    za = open(arms_zone, "rb").read()
    zg = za if gun_zone == arms_zone else open(gun_zone, "rb").read()
    zn = zg if anim_zone == gun_zone else open(anim_zone, "rb").read()
    skel = skeleton(wawimport.read_xmodel(za, "viewmodel_usa_marine_arms"), wawimport.read_xmodel(zg, "viewmodel_usa_flamethrower"))
    anim = xanim.read(zn, anim_name, strings=wawimport.script_strings(zn))
    w = pose(skel, anim, frame)
    for n in ("tag_view", "tag_camera", "tag_torso", "j_shoulder_ri", "j_elbow_ri", "j_wrist_ri", "j_shoulder_le", "j_elbow_le",
              "j_wrist_le", "tag_weapon", "j_gun", "tag_flash", "hose1"):
        if n in w:
            q, p = w[n]
            fwd = qrot(q, (1.0, 0.0, 0.0))
            print(f"{n:16s} at {p[0]:7.2f} {p[1]:7.2f} {p[2]:7.2f}   its x {fwd[0]:5.2f} {fwd[1]:5.2f} {fwd[2]:5.2f}")
    unknown = [n for n in anim["names"] if n not in w]
    print("animated bones not in the rig:", unknown or "none")


if __name__ == "__main__":
    main()
