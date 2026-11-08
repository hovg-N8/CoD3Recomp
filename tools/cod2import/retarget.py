"""CoD2's Webley reload on CoD3's first-person arms: a table of the arms'
bones (model-space matrices, as the game's pose function writes them) for
every frame of CoD2's viewmodel_webley_reload, read from the user's own
CoD2 install and a pose dump of CoD3's arms (0.5.672, COD3_POSE_DUMP).

How (0.5.673):
  - one transform A from CoD2's view space into CoD3's arms' model space,
    from the gun: CoD2's gun (j_gun's space) is drawn in the Colt body's
    place (the Webley's fit, mp_fixes.cpp MakeWebley: scale s, CoD2's +x the
    Colt's -y, offset o), the Colt body at its bone in the gun's model, the
    gun's model at the arms' tag_weapon; A is that chain at frame 0, so the
    gun at frame 0 lands on CoD3's gun at rest.
  - tag_weapon (and gizmo_weapon) follow CoD2's j_gun: the gun moves as in
    CoD2 (turned and opened for the reload).
  - the hands follow CoD2's wrists (rotation, and position with CoD3's own
    grip offset kept in the hand's frame); the fingers take CoD2's turns
    from frame 0 onto CoD3's rest; the elbows are solved (two bones, CoD3's
    own lengths, from CoD3's shoulders, bent toward CoD2's elbow); the
    forearm twist bones follow the forearm.
  - everything else (pelvis, spine, clavicles, camera tags) stays as the
    dump has it. The table is relative to the pelvis: the game puts it
    under the pelvis it has each frame (view sway, bob).

usage:
  python -I retarget.py <cod2 main> <arms pose dump> <arms skeleton json>
                        <colt pose dump> <scale> <ox> <oy> <oz> <out.bin>
                        <viewmodel_webley.mesh> [check.png]
  (the fit's numbers are in CoD3MP.log: "webley: made at ... scale S, by OX OY OZ")

out.bin (little-endian): 'WRLD', u32 1, u32 frames, u32 bones, f32 fps;
  bones x (u8 overridden, char[31] name); then frames x bones x 12 f32
  (rows x, y, z axes, then position), relative to the pelvis at rest
  (the pelvis's rest matrix first, 12 f32, before the frames).
"""
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cod2import as c  # noqa: E402
import xanim  # noqa: E402


# ---- small 3D helpers (R as rows of a 3x3: world = R @ local) -------------
def mmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mvec(a, v):
    return [sum(a[i][k] * v[k] for k in range(3)) for i in range(3)]


def tr(a):
    return [[a[j][i] for j in range(3)] for i in range(3)]


def add(a, b):
    return [x + y for x, y in zip(a, b)]


def sub(a, b):
    return [x - y for x, y in zip(a, b)]


def scale(a, s):
    return [x * s for x in a]


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def norm(a):
    n = math.sqrt(dot(a, a)) or 1.0
    return [x / n for x in a]


LEFT_FADE = 10.0   # frames over which the left hand leaves CoD3's rest for CoD2's path
LEFT_NEAR, LEFT_AWAY = 9.5, 13.0   # CoD2 units, the left wrist from j_gun: at the gun (8-9 with the speed-loader), away
# A finger joint's frame, CoD2's to CoD3's. Both bend about z, CoD2's the other
# way round (its curls turn +z, the Biped's -z); CoD3's run along +x on both
# sides, CoD2's left along +x and its right along -x (a mirrored rig): half a
# turn about x on the left, about y on the right.
FINGER_AXES = {"l": [[1, 0, 0], [0, -1, 0], [0, 0, -1]], "r": [[-1, 0, 0], [0, 1, 0], [0, 0, -1]]}
# 0.5.677: the thumbs are the other way: CoD2's thumb joints curl about -z, as
# the Biped's do (at the grip j_thumb_le_1 -0.26, j_thumb_le_2 -0.16; finger01
# -0.27, finger02 -0.28), so no flip; the right one runs along -x still
THUMB_AXES = {"l": [[1, 0, 0], [0, 1, 0], [0, 0, 1]], "r": [[-1, 0, 0], [0, -1, 0], [0, 0, 1]]}


def mat_quat(r):
    t = r[0][0] + r[1][1] + r[2][2]
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        return ((0.25 * s), (r[2][1] - r[1][2]) / s, (r[0][2] - r[2][0]) / s, (r[1][0] - r[0][1]) / s)
    if r[0][0] > r[1][1] and r[0][0] > r[2][2]:
        s = math.sqrt(1.0 + r[0][0] - r[1][1] - r[2][2]) * 2
        return ((r[2][1] - r[1][2]) / s, 0.25 * s, (r[0][1] + r[1][0]) / s, (r[0][2] + r[2][0]) / s)
    if r[1][1] > r[2][2]:
        s = math.sqrt(1.0 + r[1][1] - r[0][0] - r[2][2]) * 2
        return ((r[0][2] - r[2][0]) / s, (r[0][1] + r[1][0]) / s, 0.25 * s, (r[1][2] + r[2][1]) / s)
    s = math.sqrt(1.0 + r[2][2] - r[0][0] - r[1][1]) * 2
    return ((r[1][0] - r[0][1]) / s, (r[0][2] + r[2][0]) / s, (r[1][2] + r[2][1]) / s, 0.25 * s)


def mat_slerp(a, b, t):
    """Rotation a turned toward b by t (0: a, 1: b)."""
    if t <= 0:
        return a
    if t >= 1:
        return b
    return quat_mat(xanim.slerp(mat_quat(a), mat_quat(b), t))


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def quat_mat(q):
    w, x, y, z = q
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
            [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]]


def orthonormal(r):
    """The nearest rotation (Gram-Schmidt on the columns)."""
    x = norm([r[0][0], r[1][0], r[2][0]])
    y = [r[0][1], r[1][1], r[2][1]]
    y = norm(sub(y, scale(x, dot(x, y))))
    z = cross(x, y)
    return [[x[i], y[i], z[i]] for i in range(3)]


def frame_from_axes(x, n):
    """A rotation with x along x and its y in the plane of x and n."""
    x = norm(x)
    y = norm(sub(n, scale(x, dot(x, n))))
    z = cross(x, y)
    return [[x[i], y[i], z[i]] for i in range(3)]


# ---- CoD3's side ------------------------------------------------------------
def read_pose_dump(path):
    d = Path(path).read_bytes()
    _model, _part, _pose, _index, bones = struct.unpack_from(">5I", d, 0)
    o = 20 + 0x200 + 0x40 + 20 + 64
    out = []
    for b in range(bones):
        v = struct.unpack_from(">16f", d, o + 64 * b)
        rows = [v[0:3], v[4:7], v[8:11]]           # the bone's x, y, z axes in model space
        rot = [[rows[j][i] for j in range(3)] for i in range(3)]   # as columns
        out.append((rot, list(v[12:15])))
    return out


# ---- the mapping -------------------------------------------------------------
FINGERS = {"index": "1", "mid": "2", "ring": "3", "pinky": "4", "thumb": "0"}


def cod3_of(cod2):
    """CoD3's bone for a CoD2 hand bone (None: not mapped)."""
    for side2, side3 in (("_le", "l"), ("_ri", "r")):
        if not cod2.endswith(side2) and side2 + "_" not in cod2:
            continue
        if cod2 == "j_wrist" + side2:
            return f"bip01 {side3} hand"
        for f2, f3 in FINGERS.items():
            for k, suffix in enumerate(("", "1", "2")):
                if cod2 == f"j_{f2}{side2}_{k}":
                    return f"bip01 {side3} finger{f3}{suffix}" if not (f3 == "0" and suffix) else f"bip01 {side3} finger0{suffix}"
    return None


def main():
    main_dir, arms_dump, skel_json, colt_dump = sys.argv[1:5]
    s, ox, oy, oz = (float(x) for x in sys.argv[5:9])
    out_path = sys.argv[9]
    mesh_path = sys.argv[10]
    check = sys.argv[11] if len(sys.argv) > 11 else None

    names3 = [b["name"] for b in json.load(open(skel_json))]
    parents3 = [b["parent"] for b in json.load(open(skel_json))]
    rest3 = read_pose_dump(arms_dump)
    n3 = len(rest3)
    names3, parents3 = names3[:n3], parents3[:n3]
    idx3 = {n: i for i, n in enumerate(names3)}
    colt = read_pose_dump(colt_dump)
    colt_rot, colt_pos = colt[3]                      # the Colt's body bone in its model

    arch = c.Archives(main_dir)
    anim = xanim.read_xanim(arch.read("xanim/viewmodel_webley_reload"))
    bones2 = xanim.skeleton(arch, "viewmodel_hands_british", "viewmodel_webley")
    frames = anim["frames"]

    def world2(frame):
        p = xanim.pose(bones2, anim, frame)
        return {n: (quat_mat(q), list(v)) for n, (v, q) in p.items()}

    ref2 = world2(0)
    rf = [[0, 1, 0], [-1, 0, 0], [0, 0, 1]]           # CoD2's +x the Colt's -y
    o = [ox, oy, oz]
    tag_rot_ref, tag_pos_ref = rest3[idx3["tag_weapon"]]
    j2_rot_ref, j2_pos_ref = ref2["j_gun"]

    # A: CoD2 view space -> CoD3 arms model space (rotation a_rot, scale s)
    gun_rot = mmul(tag_rot_ref, mmul(colt_rot, rf))   # the gun's CoD2 axes in CoD3, at rest
    a_rot = mmul(gun_rot, tr(j2_rot_ref))

    def a_point(p):
        local = mvec(tr(j2_rot_ref), sub(p, j2_pos_ref))            # in j_gun's space
        colt_local = add(scale(mvec(rf, local), s), o)               # the fit: the Colt body's space
        gun_model = add(mvec(colt_rot, colt_local), colt_pos)       # the gun's model
        return add(mvec(tag_rot_ref, gun_model), tag_pos_ref)       # the arms' model

    # tag_weapon's origin in j_gun's space (where the arms' tag_weapon sits on the gun)
    x0 = mvec(tr(rf), scale(sub(mvec(tr(colt_rot), scale(colt_pos, -1)), o), 1.0 / s))

    # each mapped CoD3 bone: its rest rotation against CoD2's at frame 0
    mapped = {}
    for b in bones2:
        n3name = cod3_of(b["name"])
        if n3name in idx3:
            r2 = ref2[b["name"]][0]
            mapped[n3name] = (b["name"], mmul(tr(mmul(a_rot, r2)), rest3[idx3[n3name]][0]))

    hand_off = {}
    for side in ("l", "r"):
        hname = f"bip01 {side} hand"
        cod2w = "j_wrist_" + ("le" if side == "l" else "ri")
        hr, hp = rest3[idx3[hname]]
        hand_off[side] = mvec(tr(hr), sub(hp, a_point(ref2[cod2w][1])))

    parent2 = {b["name"]: bones2[b["parent"]]["name"] for b in bones2 if b["parent"] >= 0}
    # CoD3's left hand's shape at rest (the hand to the middle knuckle, the
    # index knuckle to the little finger's), in the hand's own frame
    lh = idx3["bip01 l hand"]
    left_shape3 = mmul(tr(rest3[lh][0]), frame_from_axes(sub(rest3[idx3["bip01 l finger2"]][1], rest3[lh][1]),
                                                         sub(rest3[idx3["bip01 l finger1"]][1], rest3[idx3["bip01 l finger4"]][1])))

    # the gun at rest in CoD3 (where CoD2's j_gun lands) and the scale of its
    # path in the view: the fit's, as the hands on it have (CoD2's whole view
    # scaled alike, turned to CoD3's, anchored at the gun at rest). By the
    # guns' distances from the eye (x2.74: CoD2's eye is only 2.7 from its
    # gun) the gun left the screen for most of the reload.
    gun_rest3 = a_point(j2_pos_ref)
    view_scale = s
    print(f"  the gun at rest: CoD2 {math.dist(j2_pos_ref, ref2['tag_view'][1]):.2f} from the eye, CoD3 {math.dist(gun_rest3, [0, 0, 0]):.2f}: its path x{view_scale:.3f}")

    def local_offset(child, parent):
        pr, pp = rest3[parent]
        return mvec(tr(pr), sub(rest3[child][1], pp))

    pelvis_r, pelvis_p = rest3[idx3["bip01 pelvis"]]
    override = set()
    table = []
    for f in range(frames):
        w2 = world2(f)
        rot = [None] * n3
        pos = [None] * n3
        for i in range(n3):
            rot[i], pos[i] = [row[:] for row in rest3[i][0]], rest3[i][1][:]

        # 0.5.677: the gun moves in CoD3's view as CoD2's moves in CoD2's
        # (both views: the eye at the origin, x forward, y left, z up): its
        # turn since frame 0 as it is (gun_turn), its path scaled by the two
        # guns' distances from the eye (VIEW_SCALE); the hands and fingers
        # keep CoD2's relation to the gun (through the fit: b_point, b_rot).
        # Before, everything went through the gun's rest alone, and CoD2's
        # gun coming up to the eye to throw the cases went away and sideways
        # in CoD3 while the left hand came up into the camera.
        jr, jp = w2["j_gun"]
        gun_turn = mmul(jr, tr(j2_rot_ref))
        gun_at = add(gun_rest3, scale(sub(jp, j2_pos_ref), view_scale))

        def b_point(p):
            back = add(mvec(mmul(j2_rot_ref, tr(jr)), sub(p, jp)), j2_pos_ref)   # p with the gun put back at rest
            return add(gun_at, mvec(gun_turn, sub(a_point(back), gun_rest3)))

        def b_rot(r2):
            return mmul(gun_turn, mmul(a_rot, mmul(j2_rot_ref, mmul(tr(jr), r2))))

        for name in ("gizmo_weapon", "tag_weapon"):
            i = idx3[name]
            rot[i] = orthonormal(mmul(gun_turn, rest3[i][0]))
            pos[i] = add(gun_at, mvec(gun_turn, sub(rest3[i][1], gun_rest3)))
            override.add(i)

        for side, s2 in (("l", "le"), ("r", "ri")):
            hi = idx3[f"bip01 {side} hand"]
            fi = idx3[f"bip01 {side} forearm"]
            ui = idx3[f"bip01 {side} upperarm"]
            # the hand
            r2w, p2w = w2["j_wrist_" + s2]
            rot[hi] = orthonormal(mmul(b_rot(r2w), mapped[names3[hi]][1]))
            # 0.5.674: the left hand goes where CoD2's goes (on the gun, to the
            # speed-loader): CoD3's own offset (its left arm is off-screen at
            # rest, a one-handed pistol) only in the first and last 10 frames
            keep = 1.0 if side == "r" else smooth(1.0 - min(f, frames - 1 - f) / LEFT_FADE)
            # 0.5.677: and while CoD2's left hand is away from the gun (to the
            # pouch for the speed-loader and back: 13-17 units off), CoD3's
            # own (off-screen): mapped into CoD3's view its trip went across
            # the camera and filled the screen (0.5.676's screenshots)
            if side == "l":
                away = math.dist(p2w, w2["j_gun"][1])
                keep = max(keep, smooth((away - LEFT_NEAR) / (LEFT_AWAY - LEFT_NEAR)))
            # 0.5.676: and turned as CoD2's is, by the hand's own shape (the
            # wrist to the middle knuckle, the index knuckle to the little
            # finger's): its rest against CoD2's frame 0 (a hand off-screen
            # against one on the gun) gave it a twist
            pole = b_point(w2["j_elbow_" + s2][1])
            if side == "l":
                # 0.5.677: a true blend, CoD2's hand (keep 0) to CoD3's own at
                # rest (keep 1): position, turn and the elbow's bend
                g2 = frame_from_axes(sub(w2["j_mid_le_0"][1], p2w), sub(w2["j_index_le_0"][1], w2["j_pinky_le_0"][1]))
                shaped = orthonormal(mmul(b_rot(g2), tr(left_shape3)))
                rot[hi] = mat_slerp(shaped, rest3[hi][0], keep)
                pos[hi] = add(scale(b_point(p2w), 1.0 - keep), scale(rest3[hi][1], keep))
                pole = add(scale(pole, 1.0 - keep), scale(rest3[fi][1], keep))
            else:
                pos[hi] = add(b_point(p2w), mvec(rot[hi], hand_off[side]))
            # the elbow: two bones from the shoulder to the hand
            S, H = rest3[ui][1], pos[hi]
            l1 = math.dist(rest3[ui][1], rest3[fi][1])
            l2 = math.dist(rest3[fi][1], rest3[hi][1])
            d = min(math.dist(S, H), l1 + l2 - 1e-4)
            axis = norm(sub(H, S))
            bend = norm(sub(sub(pole, S), scale(axis, dot(sub(pole, S), axis))))
            ca = max(-1.0, min(1.0, (l1 * l1 + d * d - l2 * l2) / (2 * l1 * d)))
            E = add(S, add(scale(axis, l1 * ca), scale(bend, l1 * math.sqrt(max(0.0, 1 - ca * ca)))))
            # upper arm and forearm: their x along the bone, the bend plane kept
            for bi, a_, b_, nxt in ((ui, S, E, H), (fi, E, H, None)):
                ref_dir = sub(rest3[fi if bi == ui else hi][1], rest3[bi][1])
                ref_n = cross(sub(rest3[fi][1], rest3[ui][1]), sub(rest3[hi][1], rest3[fi][1]))
                new_n = cross(sub(E, S), sub(H, E))
                rot[bi] = mmul(frame_from_axes(sub(b_, a_), new_n), mmul(tr(frame_from_axes(ref_dir, ref_n)), rest3[bi][0]))
                pos[bi] = a_
            # the forearm's twist bones follow it
            for t in ("foretwist", "foretwist1", "foretwist2"):
                ti = idx3[f"bip01 {side} {t}"]
                rot[ti] = mmul(rot[fi], mmul(tr(rest3[fi][0]), rest3[ti][0]))
                pos[ti] = add(pos[fi], mvec(rot[fi], mvec(tr(rest3[fi][0]), sub(rest3[ti][1], rest3[fi][1]))))
                override.add(ti)
            override.update((hi, fi, ui))
        # fingers (in parent order: a finger's parent is done before it), and
        # the right hand's ammo gizmo with its hand
        for i in range(n3):
            name = names3[i]
            if "finger" not in name and name != "gizmo_ammo_b":
                continue
            p = parents3[i]
            if name in mapped:
                # 0.5.676: each joint's own bend: CoD2's turn of the joint
                # since frame 0, in its parent's frame, made CoD3's (both
                # bend about their z, CoD2's the other way: FINGER_AXES) and
                # put on CoD3's joint at rest. 0.5.673-675 matched whole
                # world turns, and the left hand's rest (off-screen) against
                # CoD2's (on the gun) bent the fingers about the wrong axes.
                n2 = mapped[name][0]
                p2 = parent2[n2]
                l2 = mmul(tr(w2[p2][0]), w2[n2][0])
                l2_0 = mmul(tr(ref2[p2][0]), ref2[n2][0])
                axes = (THUMB_AXES if "finger0" in name else FINGER_AXES)["l" if " l " in name else "r"]
                delta = mmul(axes, mmul(mmul(tr(l2_0), l2), tr(axes)))
                l3 = mmul(tr(rest3[p][0]), rest3[i][0])
                rot[i] = orthonormal(mmul(rot[p], mmul(l3, delta)))
            else:
                rot[i] = mmul(rot[p], mmul(tr(rest3[p][0]), rest3[i][0]))
            pos[i] = add(pos[p], mvec(rot[p], local_offset(i, p)))
            override.add(i)
        # relative to the pelvis at rest
        rel = []
        for i in range(n3):
            r = mmul(tr(pelvis_r), rot[i])
            p = mvec(tr(pelvis_r), sub(pos[i], pelvis_p))
            rel.append((r, p))
        table.append(rel)

    with open(out_path, "wb") as f:
        f.write(b"WRLD" + struct.pack("<IIIf", 1, frames, n3, float(anim["fps"])))
        for i in range(n3):
            f.write(struct.pack("<B31s", 1 if i in override else 0, names3[i].encode()[:30]))
        f.write(struct.pack("<12f", *[pelvis_r[j][k] for k in range(3) for j in range(3)], *pelvis_p))
        for rel in table:
            for r, p in rel:
                f.write(struct.pack("<12f", *[r[j][k] for k in range(3) for j in range(3)], *p))
    print(f"{out_path}: {frames} frames, {n3} bones, {len(override)} played "
          f"({', '.join(sorted(names3[i] for i in override if 'finger' not in names3[i]))} and the fingers)")

    if check:
        draw_check(table, names3, parents3, rest3, pelvis_r, pelvis_p, check, frames)

    write_gun(arch, anim, bones2, world2, str(Path(out_path).with_name("webley_reload_gun.bin")), mesh_path)


# The gun's own parts (0.5.674): webley_reload_gun.bin beside the arms'
# table, little-endian: 'WGUN', u32 2, u32 frames, u32 bones, f32 fps;
# bones x char[32] name (the gun model's bones, viewmodel_webley's);
# (version 2, 0.5.676) the cylinder: f32 centre[3], f32 axis[3] (in the mesh's
# space, the axis forward: a shot turns it +60 degrees about it, clockwise
# seen from behind as a Webley's), u32 the six rounds' bones (table indices)
# in the order they fire (the top one under the hammer first), u32 the frame
# the extractor throws the cases out, u32 the frame the new rounds are in;
# then frames x bones x (12 f32: the bone's move from the idle pose the mesh
# was made in (custom\webley\viewmodel_webley.mesh, cod2import --pose
# viewmodel_webley_idle), in j_gun's space: x, y, z rows of the turn, then
# the shift; then u8 shown: 0 while CoD2 parks it far from the gun (the
# speed-loader and the rounds out of use) and 3 bytes of padding).
PARKED = 25.0   # CoD2 units from j_gun: a part this far off is put away
SEATED = 0.3    # CoD2 units: a round this close to its chamber's place is in it


def cylinder(mesh_path, gun):
    """The cylinder from the mesh (the rounds' centres): centre, forward axis,
    and the rounds' bones in firing order (the top one first, then each that
    a +60 degree turn about the axis brings up)."""
    import meshview
    verts, _tris, names = meshview.load(mesh_path)
    acc = {}
    for v in verts:
        n = names[v[8]]
        if n.startswith("j_bullet"):
            a = acc.setdefault(n, [0.0, 0.0, 0.0, 0])
            for k in range(3):
                a[k] += v[k]
            a[3] += 1
    rounds = {n: [a[k] / a[3] for k in range(3)] for n, a in acc.items()}
    if len(rounds) != 6:
        raise SystemExit(f"{mesh_path}: {len(rounds)} rounds, 6 expected")
    centre = [sum(p[k] for p in rounds.values()) / 6 for k in range(3)]
    pts = list(rounds.values())
    axis = norm(cross(sub(pts[0], centre), sub(pts[1], centre)))
    for a, b in zip(pts, pts[1:] + pts[:1]):
        n = cross(sub(a, centre), sub(b, centre))
        if dot(n, axis) < 0:
            n = scale(n, -1)
        axis = norm(add(axis, norm(n)))
    if axis[0] < 0:                       # forward: the barrel runs along +x (the hinge forward, the hammer behind)
        axis = scale(axis, -1)
    up = [0.0, 0.0, 1.0]
    e1 = norm(sub(up, scale(axis, dot(up, axis))))   # the top, square to the axis
    e2 = cross(axis, e1)

    def angle(p):
        d = sub(p, centre)
        return math.degrees(math.atan2(dot(d, e2), dot(d, e1)))   # 0: the top; a +turn about the axis raises it
    # a +60 turn brings up the round 60 degrees behind the top
    order = sorted(rounds, key=lambda n: (-angle(rounds[n])) % 360)
    top = min(rounds, key=lambda n: abs(angle(rounds[n])))
    order = order[order.index(top):] + order[:order.index(top)]
    print(f"  cylinder: centre {[round(x, 3) for x in centre]}, axis {[round(x, 3) for x in axis]}, firing order "
          + ", ".join(f"{n} ({angle(rounds[n]):.0f})" for n in order))
    return centre, axis, [gun.index(n) for n in order]


def write_gun(arch, anim, bones2, world2, path, mesh_path):
    gun = [b["name"] for b in bones2 if b["name"].startswith(("j_gun", "gun_pivot", "j_bolt", "j_quickloader", "j_trigger", "tag_flash",
                                                              "joint3", "j_chamber", "j_bullet"))]
    idle = xanim.read_xanim(arch.read("xanim/viewmodel_webley_idle"))

    def relative(w):
        jr, jp = w["j_gun"]
        return {n: (mmul(tr(jr), w[n][0]), mvec(tr(jr), sub(w[n][1], jp))) for n in gun}

    p_idle = xanim.pose(bones2, idle, 0)
    ref = relative({n: (quat_mat(q), list(v)) for n, (v, q) in p_idle.items()})
    frames = anim["frames"]
    centre, axis, order = cylinder(mesh_path, gun)

    # the extractor's frame (the rounds leave their chambers) and the frame
    # the new ones are in (each back in its chamber), by the rounds' places
    # in the cylinder's own frame (j_chamber's)
    def seated(w):
        rel = relative(w)
        cr, cp = rel["j_chamber"]
        c0r, c0p = ref["j_chamber"]
        worst = 0.0
        for k in order:
            n = gun[k]
            now = mvec(tr(cr), sub(rel[n][1], cp))
            was = mvec(tr(c0r), sub(ref[n][1], c0p))
            worst = max(worst, math.dist(now, was))
        return worst
    worst = [seated(world2(fr)) for fr in range(frames)]
    eject = next((fr for fr in range(frames) if worst[fr] > SEATED), frames)
    insert = next((fr for fr in range(eject, frames) if worst[fr] <= SEATED), frames)
    print(f"  the cases out at frame {eject} ({eject / anim['fps']:.2f} s), the new rounds in at frame {insert} ({insert / anim['fps']:.2f} s)")

    with open(path, "wb") as f:
        f.write(b"WGUN" + struct.pack("<IIIf", 2, frames, len(gun), float(anim["fps"])))
        for n in gun:
            f.write(struct.pack("<32s", n.encode()[:31]))
        f.write(struct.pack("<6f", *centre, *axis))
        f.write(struct.pack("<6I", *order))
        f.write(struct.pack("<II", eject, insert))
        hidden = 0
        for fr in range(frames):
            rel = relative(world2(fr))
            for n in gun:
                r, p = rel[n]
                r0, p0 = ref[n]
                d_rot = mmul(r, tr(r0))                      # the move: D = G(t) inv(G(idle))
                d_pos = sub(p, mvec(d_rot, p0))
                shown = math.dist(p, [0, 0, 0]) < PARKED
                hidden += not shown
                f.write(struct.pack("<12f", *[d_rot[j][k] for k in range(3) for j in range(3)], *d_pos))
                f.write(struct.pack("<B3x", 1 if shown else 0))
    print(f"{path}: {frames} frames, {len(gun)} gun bones ({', '.join(gun)}), {hidden} bone-frames put away")


def draw_check(table, names, parents, rest, pr, pp, path, frames):
    from PIL import Image, ImageDraw
    picks = [0, frames // 5, 2 * frames // 5, 3 * frames // 5, 4 * frames // 5, frames - 1]
    w, h = 340, 300
    img = Image.new("RGB", (w * len(picks), h * 2), (24, 24, 30))
    d = ImageDraw.Draw(img)
    pts = [p for f in picks for _, p in table[f]]
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    sc = min((w - 20) / max(1e-3, hi[0] - lo[0], hi[1] - lo[1]), (h - 20) / max(1e-3, hi[2] - lo[2], hi[1] - lo[1]))
    for k, f in enumerate(picks):
        for row, (ax, ay) in enumerate(((0, 2), (0, 1))):
            def at(p):
                return (k * w + 10 + (p[ax] - lo[ax]) * sc, row * h + h - 10 - (p[ay] - lo[ay]) * sc)
            for i, (_, p) in enumerate(table[f]):
                if parents[i] < 0:
                    continue
                col = (230, 150, 90) if " l " in names[i] else (90, 170, 230) if " r " in names[i] else (200, 200, 200)
                d.line([at(table[f][parents[i]][1]), at(p)], fill=col, width=2)
            tw = table[f][names.index("tag_weapon")]
            tip = add(tw[1], mvec(tw[0], [0, -6, 0]))          # the gun's barrel, roughly
            d.line([at(tw[1]), at(tip)], fill=(240, 220, 80), width=3)
            d.text((k * w + 6, row * h + 4), f"frame {f}", fill=(200, 200, 200))
    img.save(path)
    print(f"check drawn: {path}")


if __name__ == "__main__":
    main()
