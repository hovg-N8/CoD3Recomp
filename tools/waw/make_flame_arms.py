"""World at War's flamethrower animations on CoD3's first-person arms: the
table custom\\flamethrower\\flame_arms.bin that mp_fixes.cpp plays while the
M2 is held (0.5.709). From the user's own World at War (the arms model
viewmodel_usa_marine_arms, the M2's bones and its animations, read by
wawimport.py / xanim.py / wawpose.py) and pose dumps of CoD3's arms and the
Thompson gun while the M2 is held (COD3_POSE_DUMP, custom\\webley\\dump).

The mapping (camera space both sides: x ahead, y left, z up; CoD3's arms'
model space is its camera's, the dump's tag_camera the identity):
  - World at War's whole view is scaled by the M2's fit (0.532: the M2's mesh
    is drawn that size in CoD3) about the eye. The game squeezes it along the
    view by tan(32.5)/tan(fov/2) when it plays it, so it is seen as World at
    War's 65 degree view shows it (CoD3 draws the arms with its world fov).
  - tag_weapon goes where the M2 (drawn in the Thompson's body place by the
    fit: flamefit.py) lands exactly on World at War's j_gun.
  - each hand keeps CoD3's grip on the M2 (its place on the gun at rest,
    where the fit put the M2 in CoD3's hands) at World at War's first idle
    frame, then follows World at War's wrist; its fingers stay as at rest.
  - each elbow: a two-bone solve with CoD3's lengths from CoD3's shoulder,
    bent toward World at War's elbow; the forearm twists follow the forearm.
  - the Thompson gun's own bones are held at their rest (its animations would
    move the M2's body).

usage: python -I make_flame_arms.py <waw arms zone> <common_mp zone>
           <arms pose dump> <thompson pose dump> <skeleton json> <out.bin> [check.png]

out.bin (little-endian): 'FLAM', u32 2, u32 bones, u32 anims, u32 gun bones;
  bones x (u8 played, char[31] name); the gun's rest: gun bones x 12 f32;
  the arms' pelvis at rest, 12 f32;
  then each animation: char[32] name, u32 frames, f32 fps, u32 loop, then
  frames x (bones x 12 f32 (x, y, z axes, position: camera space, before the
  squeeze), then 3 f32: the nozzle (tag_flash) in camera space).
"""
import json
import math
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.append(str(HERE.parent / "cod2import"))   # (after ours: both folders have an xanim.py)
import wawimport  # noqa: E402
import xanim  # noqa: E402   (World at War's, before retarget.py brings CoD2's of the same name)
import wawpose  # noqa: E402
import flamefit  # noqa: E402
import retarget as rt  # noqa: E402

VIEW = flamefit.SCALE
# 0.5.711: CoD3's finger joints and World at War's bone each follows (from a
# joint to the next); a CoD3 tip joint takes World at War's last bone. And
# each CoD3 joint's own bone at rest (to its child; a tip: from its parent).
_CHAINS = {"0": "thumb", "1": "index", "2": "mid", "3": "ring", "4": "pinky"}
FINGER_MAP, FINGER_CHILD = {}, {}
for _k, _w in _CHAINS.items():
    _j3 = [f"bip01 {{s3}} finger{_k}", f"bip01 {{s3}} finger{_k}1", f"bip01 {{s3}} finger{_k}2"]
    for _n, _name in enumerate(_j3):
        for _s3 in ("l", "r"):
            FINGER_MAP[_name.format(s3=_s3)] = ("j_" + _w + "_{s}_" + str(_n), "j_" + _w + "_{s}_" + str(_n + 1))
        FINGER_CHILD[_name.replace("{s3}", "{s}")] = _j3[_n + 1].replace("{s3}", "{s}") if _n < 2 else None

ANIMS = [("idle", "viewmodel_flamethrower_idle", 1), ("fire", "viewmodel_flamethrower_fire", 1),
         ("raise", "viewmodel_flamethrower_pullout", 0), ("drop", "viewmodel_flamethrower_putaway", 0)]


def qmat(q):
    x, y, z, w = q
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
            [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]]


def compose(a, b):
    """(R, p) a then b's frame inside it."""
    return rt.mmul(a[0], b[0]), rt.add(rt.mvec(a[0], b[1]), a[1])


def inverse(a):
    rinv = rt.tr(a[0])
    return rinv, rt.scale(rt.mvec(rinv, a[1]), -1.0)


def main():
    arms_zone, mp_zone, arms_dump, gun_dump, skel_json, out_path = sys.argv[1:7]
    check = sys.argv[7] if len(sys.argv) > 7 else None
    za, zm = open(arms_zone, "rb").read(), open(mp_zone, "rb").read()
    strings = wawimport.script_strings(zm)
    skel = wawpose.skeleton(wawimport.read_xmodel(za, "viewmodel_usa_marine_arms"), wawimport.read_xmodel(zm, "viewmodel_usa_flamethrower"))
    anims = [(key, xanim.read(zm, name, strings=strings), loop) for key, name, loop in ANIMS]

    names3 = [b["name"] for b in json.load(open(skel_json))]
    parents3 = [b["parent"] for b in json.load(open(skel_json))]
    rest3 = rt.read_pose_dump(arms_dump)
    n3 = len(rest3)
    names3, parents3 = names3[:n3], parents3[:n3]
    idx = {n: i for i, n in enumerate(names3)}
    gun_rest = rt.read_pose_dump(gun_dump)
    cam = rest3[idx["tag_camera"]]
    if max(abs(cam[0][i][j] - (1.0 if i == j else 0.0)) for i in range(3) for j in range(3)) > 1e-3 or max(abs(v) for v in cam[1]) > 1e-3:
        raise SystemExit("the arms' tag_camera is not the identity in this dump")

    rf = flamefit.fit_rot()
    grip, off = flamefit.GRIP, flamefit.OFFSET

    def tag_weapon_for(jr, jp):
        """tag_weapon (camera space) that puts the fitted M2 on j_gun (jr; jp already scaled by VIEW, as waw() gives it)."""
        r = rt.mmul(jr, rt.tr(rf))
        p = rt.sub(rt.add(jp, rt.scale(rt.mvec(jr, grip), VIEW)), rt.mvec(r, off))
        return r, p

    def gun_frame(tw):
        """The M2's rigid frame (j_gun's axes, its origin) in camera space from tag_weapon."""
        r = rt.mmul(tw[0], rf)
        p = rt.add(rt.mvec(tw[0], flamefit.fit_point((0.0, 0.0, 0.0))), tw[1])
        return r, p

    def nozzle(tw):
        return rt.add(rt.mvec(tw[0], flamefit.fit_point(flamefit.TAG_FLASH)), tw[1])

    def waw(anim, f):
        w = wawpose.pose(skel, anim, f)
        return {n: (qmat(q), rt.scale(list(p), VIEW)) for n, (q, p) in w.items()}

    # 0.5.711: the hands by their shape, World at War's against CoD3's at rest
    def hand_shape_w(w, s2):
        return rt.frame_from_axes(rt.sub(w[f"j_mid_{s2}_0"][1], w[f"j_wrist_{s2}"][1]),
                                  rt.sub(w[f"j_index_{s2}_0"][1], w[f"j_pinky_{s2}_0"][1]))

    def palm_w(w, s2):
        return rt.cross(rt.sub(w[f"j_mid_{s2}_0"][1], w[f"j_wrist_{s2}"][1]), rt.sub(w[f"j_index_{s2}_0"][1], w[f"j_pinky_{s2}_0"][1]))

    def at3(name):
        return rest3[idx[name]][1]

    shape3_in_hand, knuckle3, palm3 = {}, {}, {}
    for side in ("l", "r"):
        h = rest3[idx[f"bip01 {side} hand"]]
        g3 = rt.frame_from_axes(rt.sub(at3(f"bip01 {side} finger2"), h[1]), rt.sub(at3(f"bip01 {side} finger1"), at3(f"bip01 {side} finger4")))
        shape3_in_hand[side] = rt.mmul(rt.tr(g3), h[0])
        knuckle3[side] = rt.mvec(rt.tr(h[0]), rt.sub(at3(f"bip01 {side} finger2"), h[1]))
        palm3[side] = rt.cross(rt.sub(at3(f"bip01 {side} finger2"), h[1]), rt.sub(at3(f"bip01 {side} finger1"), at3(f"bip01 {side} finger4")))
    # each CoD3 finger joint: its rest against its own bone (toward its child)
    # and the palm, so World at War's bone and palm turn it
    finger3_in_frame = {}
    for n3name, child in FINGER_CHILD.items():
        for side in ("l", "r"):
            i = idx[n3name.format(s=side)]
            if child:
                d3 = rt.sub(at3(child.format(s=side)), rest3[i][1])
            else:   # a tip: its bone from its parent to it
                d3 = rt.sub(rest3[i][1], rest3[parents3[i]][1])
            finger3_in_frame[i] = rt.mmul(rt.tr(rt.frame_from_axes(d3, palm3[side])), rest3[i][0])

    played = set()

    def frame_of(anim, f):
        w = waw(anim, f)
        rot = [None] * n3
        pos = [None] * n3
        for i in range(n3):
            rot[i], pos[i] = [row[:] for row in rest3[i][0]], rest3[i][1][:]
        tw = tag_weapon_for(*w["j_gun"])
        for name in ("tag_weapon", "gizmo_weapon"):
            i = idx[name]
            rel = compose(inverse(rest3[idx["tag_weapon"]]), rest3[i])
            rot[i], pos[i] = compose(tw, rel)
            played.add(i)
        for side, s2 in (("l", "le"), ("r", "ri")):
            hi, fi, ui = idx[f"bip01 {side} hand"], idx[f"bip01 {side} forearm"], idx[f"bip01 {side} upperarm"]
            # 0.5.711: the hand as World at War's holds the M2 (the user, 710:
            # "still not holding it properly": CoD3's Thompson grip left the
            # tube across an open left palm): turned as World at War's hand is,
            # by its shape (the wrist to the middle knuckle, the index knuckle
            # to the little finger's), and placed by its middle knuckle on
            # World at War's (CoD3's hand is 25% longer: 2.53 against 2.02)
            rot[hi] = rt.mmul(hand_shape_w(w, s2), shape3_in_hand[side])
            pos[hi] = rt.sub(w[f"j_mid_{s2}_0"][1], rt.mvec(rot[hi], knuckle3[side]))
            pole = w["j_elbow_" + s2][1]
            S, H = rest3[ui][1], pos[hi]
            l1, l2 = math.dist(rest3[ui][1], rest3[fi][1]), math.dist(rest3[fi][1], rest3[hi][1])
            d = min(math.dist(S, H), l1 + l2 - 1e-4)
            axis = rt.norm(rt.sub(H, S))
            bend = rt.norm(rt.sub(rt.sub(pole, S), rt.scale(axis, rt.dot(rt.sub(pole, S), axis))))
            ca = max(-1.0, min(1.0, (l1 * l1 + d * d - l2 * l2) / (2 * l1 * d)))
            E = rt.add(S, rt.add(rt.scale(axis, l1 * ca), rt.scale(bend, l1 * math.sqrt(max(0.0, 1 - ca * ca)))))
            for bi, a_, b_ in ((ui, S, E), (fi, E, H)):
                ref_dir = rt.sub(rest3[fi if bi == ui else hi][1], rest3[bi][1])
                ref_n = rt.cross(rt.sub(rest3[fi][1], rest3[ui][1]), rt.sub(rest3[hi][1], rest3[fi][1]))
                new_n = rt.cross(rt.sub(E, S), rt.sub(H, E))
                rot[bi] = rt.mmul(rt.frame_from_axes(rt.sub(b_, a_), new_n), rt.mmul(rt.tr(rt.frame_from_axes(ref_dir, ref_n)), rest3[bi][0]))
                pos[bi] = a_
            for t in ("foretwist", "foretwist1", "foretwist2"):
                ti = idx[f"bip01 {side} {t}"]
                rot[ti], pos[ti] = compose((rot[fi], pos[fi]), compose(inverse(rest3[fi]), rest3[ti]))
                played.add(ti)
            played.update((hi, fi, ui))
        # 0.5.711: each finger joint along World at War's matching finger bone
        # (against the palm: its bend toward the palm kept), placed at its rest
        # offset from its parent (CoD3's lengths); a joint not matched (the
        # thumb's nub, the ammo gizmo) keeps its place on its parent
        for i in range(n3):
            n = names3[i]
            if "finger" not in n and n != "gizmo_ammo_b":
                continue
            p = parents3[i]
            if n in FINGER_MAP:
                side, s2 = ("l", "le") if " l " in n else ("r", "ri")
                a, b = FINGER_MAP[n]
                dir_w = rt.sub(w[b.format(s=s2)][1], w[a.format(s=s2)][1])
                frame_w = rt.frame_from_axes(dir_w, palm_w(w, s2))
                rot[i] = rt.orthonormal(rt.mmul(frame_w, finger3_in_frame[i]))
                pos[i] = rt.add(pos[p], rt.mvec(rot[p], rt.mvec(rt.tr(rest3[p][0]), rt.sub(rest3[i][1], rest3[p][1]))))
            else:
                rot[i], pos[i] = compose((rot[p], pos[p]), compose(inverse(rest3[p]), rest3[i]))
            played.add(i)
        # the nozzle: World at War's own tag_flash this frame (the fitted mesh's
        # tip is within 0.4 of it: nozzle(tw))
        return rot, pos, w["tag_flash"][1]

    tables = []
    for key, anim, loop in anims:
        frames = [frame_of(anim, f) for f in range(anim["frames"])]
        tables.append((key, anim, loop, frames))
        nz = frames[0][2]
        print(f"{key}: {anim['frames']} frames at {anim['header']['framerate']:.0f} fps; nozzle at frame 0: "
              f"{nz[0]:.2f} ahead, {-nz[1]:.2f} right, {-nz[2]:.2f} down "
              f"(on a 16:9 picture at World at War's 65 degrees: {0.5 + 0.5 * (-nz[1] / nz[0]) / (math.tan(math.radians(32.5)) * 4 / 3):.3f} "
              f"{0.5 + 0.5 * (-nz[2] / nz[0]) / (math.tan(math.radians(32.5)) * 4 / 3 / (16 / 9)):.3f})")

    with open(out_path, "wb") as f:
        f.write(b"FLAM" + struct.pack("<IIII", 2, n3, len(tables), len(gun_rest)))
        for i in range(n3):
            f.write(struct.pack("<B31s", 1 if i in played else 0, names3[i].encode()[:30]))
        for r, p in gun_rest:
            f.write(struct.pack("<12f", *[r[j][k] for k in range(3) for j in range(3)], *p))
        # (version 2, 0.5.711) the arms' pelvis at rest: the game sways and bobs
        # the arms by their pelvis, so the table goes under its move from here
        pr, pp = rest3[idx["bip01 pelvis"]]
        f.write(struct.pack("<12f", *[pr[j][k] for k in range(3) for j in range(3)], *pp))
        for key, anim, loop, frames in tables:
            f.write(struct.pack("<32sIfI", key.encode(), len(frames), float(anim["header"]["framerate"]), loop))
            for rot, pos, nz in frames:
                for i in range(n3):
                    r, p = rot[i], pos[i]
                    f.write(struct.pack("<12f", *[r[j][k] for k in range(3) for j in range(3)], *p))
                f.write(struct.pack("<3f", *nz))
    print(f"{out_path}: {n3} bones, {len(played)} played, {len(tables)} animations, the gun's {len(gun_rest)} bones held")

    if check:
        draw(tables, names3, parents3, check)


def draw(tables, names, parents, path):
    """The idle and a fire frame as World at War's 65 degree view shows them
    (bones as lines, the nozzle a dot), next to CoD3's projection."""
    from PIL import Image, ImageDraw
    W, H = 960, 540
    im = Image.new("RGB", (W * 2, H), (30, 30, 34))
    d = ImageDraw.Draw(im)
    tan_h = math.tan(math.radians(32.5)) * 4 / 3

    def proj(p, ox):
        if p[0] < 0.5:
            return None
        return (ox + W * (0.5 + 0.5 * (-p[1] / p[0]) / tan_h), H * (0.5 + 0.5 * (-p[2] / p[0]) / (tan_h * 9 / 16)))

    for k, (key, f) in enumerate((("idle", 0), ("fire", 3))):
        frames = next(t[3] for t in tables if t[0] == key)
        rot, pos, nz = frames[f]
        for i, p in enumerate(parents):
            if p < 0:
                continue
            a, b = proj(pos[p], k * W), proj(pos[i], k * W)
            if a and b:
                colour = (240, 200, 90) if "weapon" in names[i] else (120, 200, 255) if " r " in names[i] else (140, 255, 140)
                d.line([a, b], fill=colour, width=2)
        n = proj(nz, k * W)
        if n:
            d.ellipse([n[0] - 6, n[1] - 6, n[0] + 6, n[1] + 6], outline=(255, 80, 40), width=3)
        d.text((k * W + 10, 10), f"{key} frame {f}: World at War's 65 degree view (nozzle: red ring)", fill=(255, 255, 255))
    im.save(path)
    print(path)


if __name__ == "__main__":
    main()
