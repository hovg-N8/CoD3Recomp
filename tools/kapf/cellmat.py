"""CoD3 materials (APKF MAT resources) as the world cells use them, made
from a shader name and its parameters, for writing a level's materials.

Layout (big-endian; checked on the MP zones' materials by --roundtrip):
  +0x00 ->own name, ->name 'fx', 0, 0, 0, the FX import (the shader, e.g.
  e_blend), 0, 0, 0, u32 parameters, ->list, 0, CD x4
  +0x40 the list, 0x24 a parameter: ->its name, CD x3, u32 type (2 int, 3
  float, 0xB texture), ->value, u32 value size (4; 0x1C a texture), CD x2
  then the values from the next 16-byte line in parameter order, each in
  its size rounded to 16, CD-filled: an int or a float word, or a texture:
  a TEX import (or a pointer to a TEX in the same block) and CD x6.
Shaders the world uses (Rouen's zones): e_blend (position_light0,
cmn_lightmap_texture, lightmap_texture, normalmap_texture, normal_scale,
spec_power, diffusemap_texture, specularmap_texture, technique; or with
detail_repeat, detailmap_texture, detail_scale), e_decal_punchthru (the
same nine: alpha-tested), e_blend_alpha, e_decal_simple and
e_trans_cullnone (position_light0, cmn_lightmap_texture, lightmap_texture,
diffusemap_texture, technique).

usage: python -I cellmat.py --roundtrip <zone archive.cod> [more]
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402

CD4 = b"\xCD" * 4
INT, FLOAT, TEXTURE = 2, 3, 0xB


def build(params, overlay=0):
    """(bytes, refs): params = [(name, type, value)], value an int, a float or
    a texture name; overlay: the word at +0x10 (1 on nearly every decal
    material: e_decal 20 of 21, e_decal_simple 9/9, e_trans_cullnone 8/8;
    e_blend 14 of 423), for CoD2's decal-sorted materials. refs {offset:
    (kind, what)}: ("name", None) own name, ("pool", text) a name record,
    ("fx", None) the shader import, ("tex", name) a texture import,
    ("self", offset)."""
    n = len(params)
    out = bytearray(struct.pack(">10I", 0, 0, 0, 0, overlay, 0, 0, 0, 0, n) + struct.pack(">II", 0, 0) + CD4 * 4)
    refs = {0: ("name", None), 4: ("pool", "fx"), 0x14: ("fx", None), 0x28: ("self", 0x40)}
    values = (0x40 + 0x24 * n + 15) & ~15
    slots, at = [], values
    for _name, kind, _v in params:
        slots.append(at)
        at += 0x20 if kind == TEXTURE else 0x10
    for k, (name, kind, _v) in enumerate(params):
        r = len(out)
        out += bytes(4) + CD4 * 3 + struct.pack(">III", kind, 0, 0x1C if kind == TEXTURE else 4) + CD4 * 2
        refs[r] = ("pool", name)
        refs[r + 0x14] = ("self", slots[k])
    out += b"\xCD" * (values - len(out))
    for k, (name, kind, v) in enumerate(params):
        if kind == TEXTURE:
            refs[len(out)] = ("tex", v)
            out += bytes(4) + CD4 * 7
        else:
            out += (struct.pack(">f", v) if kind == FLOAT else struct.pack(">I", v)) + CD4 * 3
    return bytes(out), refs


def parse(p, f, ptrs, imps):
    """The parameters of a material in the list layout, or None."""
    d, at = p.d, f["at"]
    if at + 0x14 not in imps or at + 0x28 not in ptrs:
        return None
    lst = p.target(apkf.word(d, at + 0x28))
    params = []
    for k in range(apkf.word(d, at + 0x24)):
        r = lst + 0x24 * k
        name = p.name_at(4 * (apkf.word(d, r) & 0x3FFFFFF))
        kind, val = apkf.word(d, r + 0x10), p.target(apkf.word(d, r + 0x14))
        if kind == TEXTURE:
            v = ("import", p.name_at(imps[val][1])) if val in imps else ("ptr", p.target(apkf.word(d, val)))
        elif kind == FLOAT:
            v = struct.unpack_from(">f", d, val)[0]
        else:
            v = apkf.word(d, val)
        params.append((name, kind, v))
    return params


def main():
    if sys.argv[1:2] != ["--roundtrip"]:
        print(__doc__)
        return
    from collections import Counter
    C = Counter()
    for path in sys.argv[2:]:
        a = kapf.Archive(path)
        for b in range(len(a.blocks())):
            d = a.block(b)
            if d[:4] != b"APKF":
                continue
            p = apkf.read(d)
            lists = p.tail_lists()
            if lists is None:
                continue
            ptrs = {p.image + 4 * i for i in lists[0]}
            imps = {p.image + 4 * i: (t, n) for i, t, n in lists[1]}
            for f in p.files:
                if p.types[f["type"]]["name"] != "MAT":
                    continue
                params = parse(p, f, ptrs, imps)
                if params is None:
                    C["other layout"] += 1
                    continue
                ours, refs = build([(n, k, v[1] if isinstance(v, tuple) else v) for n, k, v in params], apkf.word(d, f["at"] + 0x10))
                theirs = p.cpu(f)
                if len(ours) != len(theirs):
                    C[f"size differs"] += 1
                    continue
                ok = True
                for o in range(0, len(ours), 4):
                    here = f["at"] + o
                    if o in refs:
                        kind = refs[o][0]
                        want_imp = kind in ("fx",) or (kind == "tex" and here in imps)
                        if not ((here in imps) if want_imp else (here in ptrs or here in imps)):
                            ok = False
                    elif here in ptrs or here in imps or ours[o:o + 4] != theirs[o:o + 4]:
                        ok = False
                C["same" if ok else "differs"] += 1
    print(C)


if __name__ == "__main__":
    main()
