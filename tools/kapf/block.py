"""An APKF block made from resources with symbolic references, laid out by
apkf.compose's rules (header, image, physical, tail) with every pointer
resolved and listed as a fix-up, and every import listed.

A resource: (kind, name, cpu bytes, refs, gpu bytes or None). kind: 'MAT',
'TEX', 'MESH', 'MLTI' or a plain file's extension. refs {cpu offset: ref}:
  ("name",)                 this resource's own name record
  ("pool", text)            a name record (added to the pool)
  ("self", offset)          a place in this resource
  ("res", name, offset)     a place in another resource of this block
  ("import", tag, text)     an import (the word stays 0, the game resolves it)
Type records as Rouen's zones have them (rouen/zn01.cod block 0).
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402

TYPES = {"MAT": (0x0054414D, 2, 1, [0x10, 0xFF000000]),
         "TEX": (0x00584554, 3, 2, [0x4, 0x01001000]),
         "MESH": (0x4853454D, 2, 2, [0x10, 0x01000004]),
         "MLTI": (0x49544C4D, 100, 1, [0x4, 0xFF000000])}
ORDER = ["plain", "MAT", "TEX", "MESH", "MLTI"]
IMAGE_ALIGN, PHYSICAL_ALIGN = 16, 0x10000


def type_record(kind):
    if kind in TYPES:
        tag, version, parts, aligns = TYPES[kind]
    else:  # a plain file: the tag is its extension's hash
        tag, version, parts, aligns = apkf.name_hash(kind), 0, 1, [0x10, 0xFF000000]
    return {"tag": tag, "version": version, "parts": parts, "aligns": list(aligns)}


def build(resources):
    kinds = sorted({r[0] for r in resources}, key=lambda k: (ORDER.index(k) if k in ORDER else 0, k))
    types = [type_record(k) for k in kinds]
    ordered = [r for k in kinds for r in resources if r[0] == k]
    # the name pool: every resource's name, the region names, every referenced text
    pool, where = [], {}

    def pool_index(text):
        if text not in where:
            where[text] = len(pool)
            raw = text.encode("latin-1")[:27]
            pool.append((apkf.name_hash(text), raw + bytes(28 - len(raw))))
        return where[text]

    for r in ordered:
        pool_index(r[1])
    for r in ordered:
        for ref in r[3].values():
            if ref[0] == "pool":
                pool_index(ref[1])
            elif ref[0] == "import":
                pool_index(ref[2])
    regions = [{"name": pool_index("image"), "w1": 0, "align": IMAGE_ALIGN, "w3": 0},
               {"name": pool_index("physical"), "w1": 0, "align": PHYSICAL_ALIGN, "w3": 0}]
    # the layout compose() will make: header size from the counts, then the image
    nreg = 2
    entries_at = 0x1C + (20 + 4 * nreg) * len(types) + 4
    table = entries_at + sum(4 * (t["parts"] + 1) * sum(1 for r in ordered if r[0] == kinds[i]) for i, t in enumerate(types))
    names = table
    image = apkf.align_up(names + 32 * len(pool) + 24 * nreg, IMAGE_ALIGN)
    at, place = image, {}
    for r in ordered:
        at = apkf.align_up(at, types[kinds.index(r[0])]["aligns"][0])
        place[r[1]] = at
        at += len(r[2])
    fixups, imports, files = [], [], []
    for r in ordered:
        kind, name, cpu, refs, gpu = r
        data = bytearray(cpu)
        base = place[name]
        for off, ref in refs.items():
            word = (base + off - image) // 4
            if ref[0] == "import":
                struct.pack_into(">I", data, off, 0)
                imports.append((word, apkf.name_hash(ref[1]) if ref[1] not in ("FX", "TEX", "MAT", "MESH") else
                                {"FX": 0x00005846, "TEX": 0x00584554, "MAT": 0x0054414D, "MESH": 0x4853454D}[ref[1]],
                                32 * where[ref[2]]))
                continue
            if ref[0] == "name":
                value = 0xFC000000 | (8 * where[name])
            elif ref[0] == "pool":
                value = 0xFC000000 | (8 * where[ref[1]])
            elif ref[0] == "self":
                value = (base + ref[1] - image) // 4
            elif ref[0] == "res":
                value = (place[ref[1]] + ref[2] - image) // 4
            else:
                raise ValueError(f"{name}: unknown reference {ref}")
            struct.pack_into(">I", data, off, value)
            fixups.append(word)
        t = kinds.index(kind)
        sizes = [len(data)] + ([len(gpu or b"")] if types[t]["parts"] > 1 else [])
        files.append({"type": t, "name": where[name], "cpu": bytes(data), "gpu": gpu or b"", "sizes": sizes})
    parts = {"words": (0x04010000, 0x04000000, 0xFFFFFFFF), "types": types, "files": files, "pool": pool,
             "regions": regions, "fixups": sorted(fixups), "imports": sorted(imports), "gaps": b""}
    out = apkf.compose(parts)
    check = apkf.read(out)
    if check.image != image or any(f["at"] != place[r[1]] for f, r in zip(check.files, ordered)):
        raise AssertionError("block layout differs from the planned one")
    return out
