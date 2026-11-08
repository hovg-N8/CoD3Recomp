"""Read a Call of Duty 2 model and texture from the user's own CoD2 install.

0.5.652 (the Webley in CoD3's multiplayer). CoD2's assets are zip archives
(main/*.iwd). This reads, never runs, them:
  xmodel/<name>          the model: its detail levels' part names, their materials
  xmodelparts/<part>     the bones: parent, position, rotation (version 20)
  xmodelsurfs/<part>     the surfaces: vertices (normal, colour, uv, bone,
                         position relative to the bone) and triangles
  images/<name>.iwi      textures (version 5: DXT1/3/5, ARGB32, RGB24)
The formats follow mauserzjeh/cod-asset-importer (GPL-3.0; rust/.../assets/
xmodel.rs, xmodelpart.rs, xmodelsurf.rs, iwi.rs), written again here in Python.

usage:
  python -I cod2import.py <cod2 main folder> model <xmodel name> <out.mesh> [--pose <xanim name>]
    (--pose: the bones as that animation's first frame has them; CoD2's
    viewmodels need it, e.g. --pose viewmodel_webley_idle)
  python -I cod2import.py <cod2 main folder> image <image name> <out.png>
  python -I cod2import.py <cod2 main folder> file <path in the archives> <out file>

The .mesh written is little-endian:
  "C2MS" u32 version 1, u32 vertex count, u32 triangle count, u32 surface count,
  then per vertex: f32 position[3] (bones applied), f32 normal[3], f32 uv[2],
  u32 bone index, then u16 indices (3 a triangle), then per surface:
  u32 first triangle, u32 triangle count, u32 name length, the material name;
  then u32 bone count and per bone: u32 name length, the name, f32 world position[3].
"""
import math
import struct
import sys
import zipfile
import zlib
from pathlib import Path

ROTATION_DIVISOR = 32768.0
RIGGED = 65535


class Archives:
    """CoD2's .iwd files; a later archive's file wins, as the game loads them."""

    def __init__(self, main):
        self.files = {}
        for iwd in sorted(Path(main).glob("*.iwd")):
            z = zipfile.ZipFile(iwd)
            for name in z.namelist():
                self.files[name.lower()] = (z, name)

    def read(self, path):
        hit = self.files.get(path.lower())
        if not hit:
            raise FileNotFoundError(path)
        z, name = hit
        return z.read(name)


class Reader:
    def __init__(self, data):
        self.d, self.p = data, 0

    def take(self, fmt):
        v = struct.unpack_from("<" + fmt, self.d, self.p)
        self.p += struct.calcsize("<" + fmt)
        return v if len(v) > 1 else v[0]

    def skip(self, n):
        self.p += n

    def string(self):
        end = self.d.index(b"\0", self.p)
        s = self.d[self.p:end].decode("latin-1")
        self.p = end + 1
        return s


def quat_rotate(v, q):
    w, x, y, z = q
    a = (y * v[2] - z * v[1] + v[0] * w, z * v[0] - x * v[2] + v[1] * w, x * v[1] - y * v[0] + v[2] * w)
    b = (y * a[2] - z * a[1], z * a[0] - x * a[2], x * a[1] - y * a[0])
    return (v[0] + 2 * b[0], v[1] + 2 * b[1], v[2] + 2 * b[2])


def quat_multiply(q1, q2):
    w1, x1, y1, z1 = q1
    w2, x2, y2, z2 = q2
    return (w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
            w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
            w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
            w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2)


def read_xmodel(data):
    r = Reader(data)
    version = r.take("H")
    if version != 20:
        raise ValueError(f"xmodel version {version}, not CoD2's 20")
    r.skip(25)
    lods = []
    for _ in range(4):
        distance = r.take("f")
        name = r.string()
        if name:
            lods.append({"name": name, "distance": distance, "materials": []})
    r.skip(4)
    for _ in range(r.take("I")):
        sub = r.take("I")
        r.skip(sub * 48 + 36)
    for lod in lods:
        for _ in range(r.take("H")):
            lod["materials"].append(r.string())
    return lods


def read_parts(data):
    r = Reader(data)
    version = r.take("H")
    if version != 20:
        raise ValueError(f"xmodelparts version {version}, not CoD2's 20")
    count, roots = r.take("HH")
    bones = [{"parent": -1, "pos": (0.0, 0.0, 0.0), "rot": (1.0, 0.0, 0.0, 0.0)} for _ in range(roots)]
    for _ in range(count):
        parent = r.take("b")
        pos = r.take("fff")
        qx, qy, qz = (c / ROTATION_DIVISOR for c in r.take("hhh"))
        qw = math.sqrt(max(0.0, 1.0 - qx * qx - qy * qy - qz * qz))
        bones.append({"parent": parent, "pos": pos, "rot": (qw, qx, qy, qz)})
    for bone in bones:
        bone["name"] = r.string()
        bone["wpos"], bone["wrot"] = bone["pos"], bone["rot"]
    for bone in bones:
        if bone["parent"] > -1:
            parent = bones[bone["parent"]]
            p = quat_rotate(bone["pos"], parent["wrot"])
            bone["wpos"] = (parent["wpos"][0] + p[0], parent["wpos"][1] + p[1], parent["wpos"][2] + p[2])
            bone["wrot"] = quat_multiply(parent["wrot"], bone["rot"])
    return bones


def read_xanim_pose(data, frame=0):
    """An xanim's (version 14) bones at one frame: {name: (rotation or None,
    translation or None)}, local to each bone's parent. CoD2's viewmodels have
    every bone at the origin in their parts; their animations place the parts
    (0.5.661: the Webley's barrel, cylinder and rounds floated apart without
    this). The layout follows riicchhaarrd/xmodelconverter (GPL-3.0,
    animation.cpp), written again here."""
    r = Reader(data)
    version = r.take("H")
    if version != 14:
        raise ValueError(f"xanim version {version}, not CoD2's 14")
    frames, parts = r.take("HH")
    flags = r.take("B")
    r.take("H")  # frame rate
    looping, delta = flags & 1, flags & 2

    def frame_list(count):
        if count == 1 or count == frames:
            return list(range(count))
        return [r.take("H" if frames > 0xFF else "B") for _ in range(count)]

    def rotations(simple):
        count = r.take("H")
        if not count:
            return {}
        at = frame_list(count)
        out = {}
        for f in at:
            x = y = 0.0
            if simple:
                z = r.take("h") / 32767.0
            else:
                x, y, z = (c / 32767.0 for c in r.take("hhh"))
            w = 1.0 - x * x - y * y - z * z
            out[f] = (math.sqrt(w) if w > 0 else 0.0, x, y, z)
        return out

    def translations():
        count = r.take("H")
        if not count:
            return {}
        if count == 1:
            return {0: r.take("fff")}
        at = frame_list(count)
        return {f: r.take("fff") for f in at}

    if delta:
        rotations(True)
        translations()
    if looping:
        frames += 1
    flag_bytes = ((parts - 1) >> 3) + 1
    flip = r.d[r.p:r.p + flag_bytes]; r.skip(flag_bytes)
    simple = r.d[r.p:r.p + flag_bytes]; r.skip(flag_bytes)
    names = [r.string() for _ in range(parts)]

    def at_frame(keys):
        best = None
        for f in sorted(keys):
            if f <= frame:
                best = keys[f]
        return best if best is not None else (keys[min(keys)] if keys else None)

    pose = {}
    for i, name in enumerate(names):
        rot = rotations(bool(simple[i >> 3] & (1 << (i & 7))))
        trans = translations()
        pose[name] = (at_frame(rot), at_frame(trans), bool(flip[i >> 3] & (1 << (i & 7))))
    return pose


def apply_pose(bones, pose):
    """The bones' local transforms from an animation's pose, their world ones
    made again; the model's root bones stay at the origin (the gun's place in
    the hand is fitted in CoD3)."""
    for bone in bones:
        hit = pose.get(bone["name"])
        if hit and bone["parent"] > -1:
            rot, trans, _ = hit
            if rot is not None:
                bone["rot"] = rot
            if trans is not None:
                bone["pos"] = trans
        bone["wpos"], bone["wrot"] = bone["pos"], bone["rot"]
    for bone in bones:
        if bone["parent"] > -1:
            parent = bones[bone["parent"]]
            p = quat_rotate(bone["pos"], parent["wrot"])
            bone["wpos"] = (parent["wpos"][0] + p[0], parent["wpos"][1] + p[1], parent["wpos"][2] + p[2])
            bone["wrot"] = quat_multiply(parent["wrot"], bone["rot"])


def read_surfaces(data, bones):
    r = Reader(data)
    version = r.take("H")
    if version != 20:
        raise ValueError(f"xmodelsurfs version {version}, not CoD2's 20")
    surfaces = []
    for _ in range(r.take("H")):
        r.skip(1)
        vcount, tcount, default_bone = r.take("HHH")
        rigged = default_bone == RIGGED
        if rigged:
            r.skip(2)
            default_bone = 0
        verts = []
        for _ in range(vcount):
            normal = r.take("fff")
            r.skip(4)  # colour
            uv = r.take("ff")
            r.skip(24)
            weights, bone = 0, default_bone
            if rigged:
                weights = r.take("B")
                bone = r.take("H")
            pos = r.take("fff")
            if weights:
                r.skip(1)
                r.skip(weights * 16)
            b = bones[bone] if bone < len(bones) else bones[0]
            p = quat_rotate(pos, b["wrot"])
            p = (p[0] + b["wpos"][0], p[1] + b["wpos"][1], p[2] + b["wpos"][2])
            n = quat_rotate(normal, b["wrot"])
            verts.append((p, n, uv, bone))
        tris = []
        for _ in range(tcount):
            a, b, c = r.take("HHH")
            tris.append((a, c, b))
        surfaces.append((verts, tris))
    return surfaces


def write_mesh(path, surfaces, materials, bones):
    verts, tris, spans = [], [], []
    for i, (sv, st) in enumerate(surfaces):
        base = len(verts)
        spans.append((len(tris), len(st), materials[i] if i < len(materials) else ""))
        verts.extend(sv)
        tris.extend((a + base, b + base, c + base) for a, b, c in st)
    out = bytearray(b"C2MS" + struct.pack("<IIII", 1, len(verts), len(tris), len(spans)))
    for p, n, uv, bone in verts:
        out += struct.pack("<3f3f2fI", *p, *n, *uv, bone)
    for t in tris:
        out += struct.pack("<3H", *t)
    for first, count, name in spans:
        nb = name.encode()
        out += struct.pack("<III", first, count, len(nb)) + nb
    out += struct.pack("<I", len(bones))
    for b in bones:
        nb = b["name"].encode()
        out += struct.pack("<I", len(nb)) + nb + struct.pack("<3f", *b["wpos"])
    Path(path).write_bytes(out)
    lo = [min(v[0][k] for v in verts) for k in range(3)]
    hi = [max(v[0][k] for v in verts) for k in range(3)]
    print(f"{path}: {len(verts)} vertices, {len(tris)} triangles, {len(spans)} surfaces "
          f"({', '.join(s[2] for s in spans)}); box {[round(x, 2) for x in lo]} .. {[round(x, 2) for x in hi]}")
    used = sorted({v[3] for v in verts})
    print("bones used: " + ", ".join(f"{i} {bones[i]['name']}" for i in used if i < len(bones)))


def decode_dxt_block_colors(c0, c1, dxt1):
    def rgb(c):
        return ((c >> 11 & 31) * 255 // 31, (c >> 5 & 63) * 255 // 63, (c & 31) * 255 // 31)
    a, b = rgb(c0), rgb(c1)
    if c0 > c1 or not dxt1:
        return [a + (255,), b + (255,),
                tuple((2 * a[k] + b[k]) // 3 for k in range(3)) + (255,),
                tuple((a[k] + 2 * b[k]) // 3 for k in range(3)) + (255,)]
    return [a + (255,), b + (255,), tuple((a[k] + b[k]) // 2 for k in range(3)) + (255,), (0, 0, 0, 0)]


IWI_DXT = {0x0B: (8, 0x12), 0x0C: (16, 0x13), 0x0D: (16, 0x14)}  # iwi format: (block bytes, CoD3 TEX format)


def iwi_levels(data):
    """A DXT .iwi's every level as PC block bytes: (iwi format, width, height,
    [level 0 blocks, level 1, ...]). The file holds the levels smallest first;
    the header's four offsets are levels 0..3, the smaller ones packed before
    level 3 from byte 28. Raises when the sizes do not add up."""
    # (version 6: World at War's, the same header and levels: Seelow, 2026-11-02)
    if data[:3] != b"IWi" or data[3] not in (5, 6):
        raise ValueError("not a CoD2 (version 5) or World at War (6) .iwi")
    fmt, _usage, w, h, _depth = struct.unpack_from("<BBHHH", data, 4)
    if fmt not in IWI_DXT:
        raise ValueError(f"iwi format {fmt:#x} is not DXT")
    block = IWI_DXT[fmt][0]
    sizes = []
    lw, lh = w, h
    while True:
        sizes.append(max(1, (lw + 3) // 4) * max(1, (lh + 3) // 4) * block)
        if lw == 1 and lh == 1:
            break
        lw, lh = max(lw // 2, 1), max(lh // 2, 1)
    if 28 + sum(sizes) == len(data):
        count = len(sizes)
    elif 28 + sizes[0] == len(data):
        count = 1
    else:
        raise ValueError(f"iwi of {len(data)} bytes holds neither a full chain nor one level")
    out, end = [], len(data)
    for k in range(count):  # level 0 last in the file
        out.append(data[end - sizes[k]:end])
        end -= sizes[k]
    return fmt, w, h, out


def decode_image(data):
    if data[:3] != b"IWi" or data[3] != 5:
        raise ValueError("not a CoD2 (version 5) .iwi")
    fmt, usage, w, h, depth = struct.unpack_from("<BBHHH", data, 4)
    offsets = struct.unpack_from("<4I", data, 12)
    first = 28
    # the largest mip: offsets[0] to the end of the file
    sizes = [(offsets[0], len(data) - offsets[0])]
    for i in range(1, 4):
        sizes.append((first, offsets[i] - first) if i == 3 else (offsets[i], offsets[i - 1] - offsets[i]))
    start, size = max(sizes, key=lambda s: s[1])
    raw = data[start:start + size]
    px = bytearray(w * h * 4)
    if fmt in (0x0B, 0x0C, 0x0D):
        block = 8 if fmt == 0x0B else 16
        bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
        for by in range(bh):
            for bx in range(bw):
                o = (by * bw + bx) * block
                alpha = None
                if fmt == 0x0C:
                    bits = int.from_bytes(raw[o:o + 8], "little")
                    alpha = [((bits >> (4 * i)) & 15) * 17 for i in range(16)]
                    o += 8
                elif fmt == 0x0D:
                    a0, a1 = raw[o], raw[o + 1]
                    bits = int.from_bytes(raw[o + 2:o + 8], "little")
                    table = [a0, a1] + ([((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)] if a0 > a1 else
                                        [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255])
                    alpha = [table[(bits >> (3 * i)) & 7] for i in range(16)]
                    o += 8
                c0, c1, idx = struct.unpack_from("<HHI", raw, o)
                colors = decode_dxt_block_colors(c0, c1, fmt == 0x0B)
                for i in range(16):
                    x, y = bx * 4 + i % 4, by * 4 + i // 4
                    if x >= w or y >= h:
                        continue
                    c = colors[(idx >> (2 * i)) & 3]
                    a = c[3] if alpha is None else alpha[i]
                    px[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes((c[0], c[1], c[2], a))
    elif fmt == 0x01:
        for i in range(w * h):
            b_, g, r_, a = raw[i * 4:i * 4 + 4]
            px[i * 4:i * 4 + 4] = bytes((r_, g, b_, a))
    elif fmt == 0x02:
        for i in range(w * h):
            b_, g, r_ = raw[i * 3:i * 3 + 3]
            px[i * 4:i * 4 + 4] = bytes((r_, g, b_, 255))
    else:
        raise ValueError(f"image format {fmt:#x} not handled")
    return w, h, bytes(px), fmt


def write_png(path, w, h, rgba):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def main():
    main_dir, kind, name, out = sys.argv[1:5]
    arch = Archives(main_dir)
    if kind == "model":
        lods = read_xmodel(arch.read(f"xmodel/{name}"))
        part = lods[0]["name"]
        print(f"{name}: detail levels {[l['name'] for l in lods]}; level 0 materials {lods[0]['materials']}")
        bones = read_parts(arch.read(f"xmodelparts/{part}"))
        if len(sys.argv) > 6 and sys.argv[5] == "--pose":
            pose = read_xanim_pose(arch.read(f"xanim/{sys.argv[6]}"))
            apply_pose(bones, pose)
            for b in bones:
                hit = pose.get(b["name"])
                print(f"  pose {b['name']}: {'flip ' if hit and hit[2] else ''}at {[round(v, 3) for v in b['wpos']]}"
                      f" turned {[round(v, 3) for v in b['wrot']]}" if hit else f"  pose {b['name']}: not in the animation")
        surfaces = read_surfaces(arch.read(f"xmodelsurfs/{part}"), bones)
        write_mesh(out, surfaces, lods[0]["materials"], bones)
    elif kind == "image":
        w, h, rgba, fmt = decode_image(arch.read(f"images/{name}.iwi"))
        write_png(out, w, h, rgba)
        print(f"{out}: {w}x{h} (format {fmt:#x})")
    elif kind == "file":
        Path(out).write_bytes(arch.read(name))
        print(f"{out}: {name}")


if __name__ == "__main__":
    main()
