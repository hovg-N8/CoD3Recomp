"""CoD2's animations (xanim version 14) read whole, every frame, and played
on a skeleton built from CoD2's models (the hands' and the gun's, joined at
tag_weapon as the game joins them), read from the user's own CoD2 install.

The key layout is read_xanim_pose's (cod2import.py, after
riicchhaarrd/xmodelconverter); this keeps every key and samples between them.

usage:
  python -I xanim.py <cod2 main> <anim> <hands model> <gun model> <out.png> [frames]
      draws the skeleton (bones as lines, from the side and from above) at
      those frames (default 0, a quarter, a half, three quarters, the last)
e.g. xanim.py ".../Call of Duty 2/main" viewmodel_webley_reload
     viewmodel_hands_british viewmodel_webley reload.png
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cod2import as c  # noqa: E402


def read_xanim(data):
    """{frames, fps, looping, bones: {name: {rot: {frame: q}, trans: {frame: v}, flip}}}"""
    r = c.Reader(data)
    version = r.take("H")
    if version != 14:
        raise ValueError(f"xanim version {version}, not CoD2's 14")
    frames, parts = r.take("HH")
    flags = r.take("B")
    fps = r.take("H")
    looping, delta = flags & 1, flags & 2

    def frame_list(count):
        if count == 1 or count == frames:
            return list(range(count))
        return [r.take("H" if frames > 0xFF else "B") for _ in range(count)]

    def rotations(simple):
        count = r.take("H")
        if not count:
            return {}
        out = {}
        for f in frame_list(count):
            x = y = 0.0
            if simple:
                z = r.take("h") / 32767.0
            else:
                x, y, z = (v / 32767.0 for v in r.take("hhh"))
            w = 1.0 - x * x - y * y - z * z
            out[f] = (math.sqrt(w) if w > 0 else 0.0, x, y, z)
        return out

    def translations():
        count = r.take("H")
        if not count:
            return {}
        if count == 1:
            return {0: r.take("fff")}
        return {f: r.take("fff") for f in frame_list(count)}

    if delta:
        rotations(True)
        translations()
    if looping:
        frames += 1
    flag_bytes = ((parts - 1) >> 3) + 1
    flip = r.d[r.p:r.p + flag_bytes]; r.skip(flag_bytes)
    simple = r.d[r.p:r.p + flag_bytes]; r.skip(flag_bytes)
    names = [r.string() for _ in range(parts)]
    bones = {}
    for i, name in enumerate(names):
        rot = rotations(bool(simple[i >> 3] & (1 << (i & 7))))
        bones[name] = {"rot": rot, "trans": translations(), "flip": bool(flip[i >> 3] & (1 << (i & 7)))}
    return {"frames": frames, "fps": fps, "looping": bool(looping), "bones": bones}


def slerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b, d = tuple(-x for x in b), -d
    if d > 0.9995:
        q = tuple(x + (y - x) * t for x, y in zip(a, b))
    else:
        th = math.acos(min(1.0, d))
        s = math.sin(th)
        wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
        q = tuple(wa * x + wb * y for x, y in zip(a, b))
    n = math.sqrt(sum(x * x for x in q)) or 1.0
    return tuple(x / n for x in q)


def sample(keys, frame, blend):
    """The key at a frame, between the two around it (blend: slerp or lerp)."""
    if not keys:
        return None
    at = sorted(keys)
    if frame <= at[0]:
        return keys[at[0]]
    if frame >= at[-1]:
        return keys[at[-1]]
    for lo, hi in zip(at, at[1:]):
        if lo <= frame <= hi:
            t = (frame - lo) / (hi - lo) if hi > lo else 0.0
            return blend(keys[lo], keys[hi], t)
    return keys[at[-1]]


def lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def skeleton(arch, hands, gun):
    """CoD2's viewmodel skeleton: the hands model's bones, then the gun's, its
    roots under tag_weapon (as the game attaches a weapon's model)."""
    def parts_of(model):
        lods = c.read_xmodel(arch.read("xmodel/" + model))
        return c.read_parts(arch.read("xmodelparts/" + lods[0]["name"]))
    bones = [dict(b) for b in parts_of(hands)]
    base = len(bones)
    weapon = next(i for i, b in enumerate(bones) if b["name"] == "tag_weapon")
    for b in parts_of(gun):
        b = dict(b)
        b["parent"] = weapon if b["parent"] < 0 else b["parent"] + base
        bones.append(b)
    return bones


def pose(bones, anim, frame):
    """World (tag_view space) position and rotation of each bone at a frame:
    {name: (wpos, wrot)}; a bone the animation does not move keeps its own."""
    out = []
    for b in bones:
        track = anim["bones"].get(b["name"])
        rot, pos = b["rot"], b["pos"]
        if track and b["parent"] > -1:
            q = sample(track["rot"], frame, slerp)
            v = sample(track["trans"], frame, lerp)
            if q is not None:
                rot = q
            if v is not None:
                pos = v
        if b["parent"] > -1:
            ppos, prot = out[b["parent"]]
            p = c.quat_rotate(pos, prot)
            out.append(((ppos[0] + p[0], ppos[1] + p[1], ppos[2] + p[2]), c.quat_multiply(prot, rot)))
        else:
            out.append((pos, rot))
    return {b["name"]: o for b, o in zip(bones, out)}


def draw(bones, poses, path):
    from PIL import Image, ImageDraw
    w, h = 360, 300
    img = Image.new("RGB", (w * len(poses), h * 2), (24, 24, 30))
    d = ImageDraw.Draw(img)
    # framed on the arms and the gun's body: the speed-loader and its rounds
    # are parked far off while not in use
    pts = [p[0] for ps in poses for n, p in ps.items() if not n.startswith(("j_quick", "j_bullet"))]
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    s = min((w - 20) / max(1e-3, max(hi[0] - lo[0], hi[1] - lo[1])), (h - 20) / max(1e-3, hi[2] - lo[2], hi[1] - lo[1]))
    for k, ps in enumerate(poses):
        for row, (ax, ay) in enumerate(((0, 2), (0, 1))):
            def at(p):
                return (k * w + 10 + (p[ax] - lo[ax]) * s, row * h + h - 10 - (p[ay] - lo[ay]) * s)
            for b in bones:
                if b["parent"] < 0:
                    continue
                name, parent = b["name"], bones[b["parent"]]["name"]
                col = (230, 150, 90) if "_le" in name else (90, 170, 230) if "_ri" in name else (200, 200, 200)
                if name.startswith(("j_gun", "gun_", "j_chamber", "j_bullet", "j_quick", "j_bolt", "joint3")):
                    col = (240, 220, 80)
                d.line([at(ps[parent][0]), at(ps[name][0])], fill=col, width=2)
    img.save(path)


def main():
    main_dir, anim_name, hands, gun, out = sys.argv[1:6]
    arch = c.Archives(main_dir)
    anim = read_xanim(arch.read("xanim/" + anim_name))
    bones = skeleton(arch, hands, gun)
    last = anim["frames"] - 1
    frames = [int(x) for x in sys.argv[6].split(",")] if len(sys.argv) > 6 else [0, last // 4, last // 2, 3 * last // 4, last]
    missing = sorted(set(anim["bones"]) - {b["name"] for b in bones})
    print(f"{anim_name}: {anim['frames']} frames at {anim['fps']} fps, {len(anim['bones'])} tracks; "
          f"skeleton {len(bones)} bones; tracks with no bone: {missing}")
    draw(bones, [pose(bones, anim, f) for f in frames], out)
    print(f"frames {frames} drawn to {out}")


if __name__ == "__main__":
    main()
