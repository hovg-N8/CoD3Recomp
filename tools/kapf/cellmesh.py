"""CoD3 world-cell meshes (APKF MESH resources named cell_NNN_mesh_MMM): a
mesh's description and GPU data, made from submeshes, for writing a level's
geometry from the user's own CoD2 maps.

The description (big-endian; offsets from the mesh's start; checked on
every cell mesh of the MP zones by --roundtrip):
  +0x00 ->own name record, 0, u32 submeshes, ->list, 0, 0, 0, u32 triangles
  +0x20 f32 sphere centre xyz, radius (the submeshes')
  +0x30 0xCD x4 (the exporter's unset bytes), 0, 0xCD x8
  +0x40 the list: (0, ->submesh) each
  a submesh (0x130 bytes, after the list):
    +0x00 f32 sphere centre xyz, radius; +0x10 min xyz, 0; +0x20 max xyz, 0
    +0x30 f32 (kept as given), ->MAT, 0, 0
    +0x40 0, ->vertex buffer, 0, ->index buffer... (see SUB below)
  the vertex format, D3D buffer objects and fetch words follow each
  submesh: see build().
GPU part: each submesh's vertices (52 bytes: f32 position, DEC3N normal,
three f32 uv pairs, colour, DEC3N tangent, DEC3N binormal), then its u16
indices, each start 4-aligned.

usage: python -I cellmesh.py --roundtrip <zone archive.cod> [more]
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402

CD4 = b"\xCD" * 4


def w(d, a):
    return struct.unpack_from(">I", d, a)[0]


def parse(p, f):
    """A cell mesh as {name, sphere, submeshes: [{sphere, lo, hi, f30, mat, decl,
    vb_words, ib_words, vcount, icount, prim, vertices, indices}], ...}
    plus the raw description and its pointer words, for comparing."""
    d, at = p.d, f["at"]
    gpu = p.gpu(f)
    fixups, imports = p.tail_lists()
    ptrs = {p.image + 4 * i - at for i in fixups if at <= p.image + 4 * i < at + f["size"]}
    imps = {p.image + 4 * i - at: (t, n) for i, t, n in imports if at <= p.image + 4 * i < at + f["size"]}
    m = {"name": f["name"], "count": w(d, at + 8), "tris": w(d, at + 0x1C),
         "sphere": struct.unpack_from(">4f", d, at + 0x20), "raw": p.cpu(f), "ptrs": ptrs, "imps": imps}
    lst = p.target(w(d, at + 0x0C))
    subs = []
    for i in range(m["count"]):
        s = p.target(w(d, lst + 8 * i + 4))
        sub = {"at": s - at, "sphere": struct.unpack_from(">4f", d, s), "lo": struct.unpack_from(">4f", d, s + 0x10),
               "hi": struct.unpack_from(">4f", d, s + 0x20), "f30": struct.unpack_from(">f", d, s + 0x30)[0]}
        sub["mat"] = (s + 0x34 - at) in imps and ("import", imps[s + 0x34 - at]) or ("ptr", p.target(w(d, s + 0x34)))
        vb, ib, decl = p.target(w(d, s + 0x40)), p.target(w(d, s + 0x50)), p.target(w(d, s + 0x60))
        sub["vcount"], sub["icount"] = w(d, s + 0x48), w(d, s + 0x58)
        stride = w(d, decl)
        e = el = p.target(w(d, decl + 4))
        while d[e:e + 2] != b"\x00\xff":
            e += 12
        sub["decl"] = d[el:e + 12]
        voff = w(d, vb + 24) & ~3
        ioff = w(d, ib + 24)
        sub["stride"] = stride
        sub["vertices"] = gpu[voff:voff + stride * sub["vcount"]]
        sub["indices"] = gpu[ioff:ioff + 2 * sub["icount"]]
        subs.append(sub)
    m["subs"] = subs
    return m


def build(subs, tris, sphere):
    """(description bytes, pointers {offset: ("self", offset) | ("mat", k)},
    gpu bytes). subs: [{sphere, lo, hi, f30, vcount, icount, stride, decl,
    vertices, indices}]; decl: the vertex format's 12-byte element records
    (end marker included). The own-name pointer at +0 is the caller's
    (("name", None))."""
    n = len(subs)
    out = bytearray(struct.pack(">IIII", 0, 0, n, 0x40) + struct.pack(">III", 0, 0, 0) + struct.pack(">I", tris))
    out += struct.pack(">4f", *sphere) + CD4 + bytes(4) + CD4 * 2
    ptrs = {0: ("name", None), 0x0C: ("self", 0x40)}
    # a submesh: 0x7C + its format's elements, then the two buffer objects (0x40); the
    # first starts on 16 bytes after the list, each but the last padded to 16
    starts = [(0x40 + 8 * n + 15) & ~15]
    for s in subs[:-1]:
        starts.append(starts[-1] + ((0x7C + len(s["decl"]) + 0x40 + 15) & ~15))
    first = starts[0]
    for k in range(n):
        out += struct.pack(">II", 0, starts[k])
        ptrs[0x40 + 8 * k + 4] = ("self", starts[k])
    out += b"\xCD" * (first - len(out))
    gpu = bytearray()
    for k, s in enumerate(subs):
        S = len(out)
        assert S == starts[k]
        vbo = 0x7C + len(s["decl"])
        gpu += b"\xCD" * (-len(gpu) % 4)  # the exporter's unset bytes before a vertex buffer
        voff = len(gpu)
        gpu += s["vertices"]
        gpu += b"\xCD" * (-len(gpu) % 4)
        ioff = len(gpu)
        gpu += s["indices"]
        out += struct.pack(">4f", *s["sphere"]) + struct.pack(">4f", *s["lo"]) + struct.pack(">4f", *s["hi"])
        out += struct.pack(">f", s["f30"]) + bytes(12)
        ptrs[S + 0x34] = ("mat", k)
        out += struct.pack(">IIII", 0, 0, s["vcount"], 0xCDCDCDCD)  # +0x40
        ptrs[S + 0x40] = ("self", S + vbo)
        out += struct.pack(">IIII", 0, 0, s["icount"], 2)  # +0x50
        ptrs[S + 0x50] = ("self", S + vbo + 0x20)
        out += struct.pack(">IIII", 0, 4, 1, 0xCDCDCDCD)  # +0x60
        ptrs[S + 0x60] = ("self", S + 0x70)
        out += struct.pack(">IIII", s["stride"], 0, 0, 0)  # +0x70: the vertex format
        ptrs[S + 0x74] = ("self", S + 0x7C)
        out = out[:S + 0x7C]
        out += s["decl"]
        out += struct.pack(">6I", 0x00200001, 1, 0, 0, 0, 0xFFFF0000)
        out += struct.pack(">II", voff | 3, 0x10000000 | (len(s["vertices"]) // 4) << 2 | 2)
        out += struct.pack(">6I", 0x20200002, 1, 0, 0, 0, 0xFFFF0000)
        out += struct.pack(">II", ioff, len(s["indices"]))
        if k + 1 < n:
            out += b"\xCD" * (-len(out) % 16)
    return bytes(out), ptrs, bytes(gpu)


def compare(m, built):
    """True when build() gives the parsed mesh's description word for word
    (pointers by target) and its GPU bytes."""
    raw, ptrs, gpu = built
    if len(raw) != len(m["raw"]):
        return f"size {len(raw):X} vs {len(m['raw']):X}"
    for o in range(0, len(raw), 4):
        if o in ptrs:
            if (o in m["ptrs"]) == (o in m["imps"]):
                if o not in m["ptrs"] and o not in m["imps"]:
                    return f"+{o:X}: a pointer here, none in the game's"
        elif o in m["ptrs"] or o in m["imps"]:
            return f"+{o:X}: the game's has a pointer here"
        elif raw[o:o + 4] != m["raw"][o:o + 4]:
            return f"+{o:X}: {raw[o:o + 4].hex()} vs {m['raw'][o:o + 4].hex()}"
    return True


def dump_layout(m):
    """The description's words as (offset, kind, value) for study."""
    raw = m["raw"]
    out = []
    for o in range(0, len(raw) - 3, 4):
        kind = "ptr" if o in m["ptrs"] else "imp" if o in m["imps"] else "word"
        out.append((o, kind, struct.unpack_from(">I", raw, o)[0]))
    return out


def main():
    if sys.argv[1:2] != ["--roundtrip"]:
        print(__doc__)
        return
    from collections import Counter
    shapes = Counter()
    for path in sys.argv[2:]:
        a = kapf.Archive(path)
        for b in range(len(a.blocks())):
            d = a.block(b)
            if d[:4] != b"APKF":
                continue
            p = apkf.read(d)
            if p.tail_lists() is None:
                continue
            for f in p.files:
                if p.types[f["type"]]["name"] != "MESH" or not f["name"].startswith("cell_"):
                    continue
                m = parse(p, f)
                s0 = m["subs"][0]["at"]
                e = s0 + 0x7C
                while m["raw"][e:e + 2] != b"\x00\xff":
                    e += 12
                decl = m["raw"][s0 + 0x7C:e + 12]
                built = build(m["subs"], m["tris"], m["sphere"])
                res = compare(m, built)
                gpu_ok = built[2] == p.gpu(f)
                shapes["description same" if res is True else "description: " + res.split(":")[0]] += 1
                shapes["gpu same" if gpu_ok else "gpu differs"] += 1
                if res is not True and shapes["shown"] < 3:
                    shapes["shown"] += 1
                    print(f["name"], res)
    for k, v in shapes.most_common(12):
        print(v, k)


if __name__ == "__main__":
    main()
