"""CoD3's APKF packs (a .cod archive's blocks), read from the user's own game
files: which files a block holds and where each one's bytes are.

Layout (big-endian; worked out on mp/rouen/rouen.cod block 0, 2026-11-01,
checked on every block of Rouen, Verdun and Poissons):
  0x00 'APKF', u32 0x04010000, u32 0x04000000, u32 -1, u32 record kind,
  0x14 u32 header size, u32 0
  0x1C type records up to the first with tag 0; 28 bytes each when the record
       kind is 2, 24 (no flags) when it is 1 (verdun block 15, effects only):
       u32 tag (a 4CC as text read backwards: 'MAT', 'TEX', 'MESH', 'MLTI'
       (a .xemesh: a bundle of the others), 'SKEL', 'FX', 'AEPS'; or a name's
       hash for a plain file), u32 version (0: a plain file), u32 parts,
       u32 entries (self-relative, from this field), u32 count, u32
       alignment, u32 flags (low 16 bits: the GPU part's alignment)
  an entry: u32 name record (self-relative), then one u32 size a part. Part
       1 is the CPU's (a plain file, or a resource's description); part 2 the
       GPU's (texture pixels, vertices and indices), 0 when none.
  the name pool: 32-byte records (u32 hash, char[28] name): the files', and
       the names their data use (bones, textures, shaders).
  two region records (24 bytes each) end the header, at the image's start -
       0x30: s32 name (self-relative: "image", "physical"), 0, u32 alignment,
       u32, u32 size, u32.
  "image" (the CPU's): from the header size + 0x44 rounded up to 16, every
       type's first parts in type order, each at its type's alignment.
  "physical" (the GPU's): from the next 64K after the image, the second
       parts in type order, each at its type's flags' alignment (TEX 0x1000,
       MESH 4). A texture's fetch constant holds its offset in this region.
  the tail, at the end of physical rounded up to 4, to the block's end:
       the fix-ups: word indices into the image (each such word is a pointer
       to relocate), to -1; then the imports: (u32 image word index, u32 type
       tag, u32 byte offset in the name pool), to -1.
  a pointer (a fix-up's word): the top 6 bits pick the base, the low 26 are
       a word offset from it: 0 = the image, 0x3F = the name pool (a
       resource's first word: its own name record).
A directory entry's index (kapf.py) is (the type's place << 16) | entry.

usage:
  python -I apkf.py <archive.cod> [block]          the block's files and places
  python -I apkf.py <archive.cod> <block> <name> <out>   one file's CPU part
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kapf  # noqa: E402


def word(d, at):
    return struct.unpack_from(">I", d, at)[0]


def tag_text(t):
    raw = struct.pack(">I", t)
    text = raw[::-1].rstrip(b"\0")
    return text.decode() if text and all(65 <= c <= 90 for c in text) else f"{t:08X}"


def align_up(x, a):
    return (x + a - 1) & ~(a - 1)


def read(d):
    """Returns a Pack: header, types, files, image, physical, names, tail."""
    if d[:4] != b"APKF":
        raise ValueError("not an APKF pack")
    header = word(d, 0x14)
    # word 0x10: the memory regions ("image": the CPU's; "physical": the
    # GPU's), their 24-byte records at the header's end; a type record has an
    # alignment word for each (0xFF000000: nothing in that region)
    count = word(d, 0x10)
    regions = []
    for r in range(count):
        at = header + 0x14 + 24 * r
        name_at = at + struct.unpack_from(">i", d, at)[0]
        regions.append({"name": d[name_at + 4:name_at + 32].split(bytes(1))[0].decode("latin-1"),
                        "align": word(d, at + 8), "size": word(d, at + 16), "words": struct.unpack_from(">6I", d, at)})
    record = 20 + 4 * count
    types = []
    at = 0x1C
    while at + record <= min(header, len(d)) and word(d, at) != 0:
        t, version, parts, rel, count_ = struct.unpack_from(">5I", d, at)
        aligns = list(struct.unpack_from(f">{count}I", d, at + 20))
        align = aligns[0] if aligns else 0
        if version > 0x200 or not 1 <= parts <= 8 or count_ > 0x10000 or at + 12 + rel + 4 * (parts + 1) * count_ > min(header, len(d)) \
                or align == 0 or align & (align - 1):
            break   # not a type record
        types.append({"at": at, "tag": t, "name": tag_text(t), "version": version, "parts": parts,
                      "entries": at + 12 + rel, "count": count_, "align": align, "aligns": aligns,
                      "flags": aligns[1] if len(aligns) > 1 else 0})
        at += record
    files = []
    for i, ty in enumerate(types):
        stride = 4 * (ty["parts"] + 1)
        for k in range(ty["count"]):
            e = ty["entries"] + stride * k
            rec = e + word(d, e)
            name = d[rec + 4:rec + 32].split(b"\0")[0].decode("latin-1")
            sizes = [word(d, e + 4 + 4 * p) for p in range(ty["parts"])]
            files.append({"type": i, "entry": k, "name": name, "record": rec, "sizes": sizes, "size": sizes[0],
                          "at": None, "gpu": None, "plain": ty["version"] == 0})
    names = min((f["record"] for f in files), default=header)
    # the image: after the region records, rounded up to the image region's
    # alignment (mostly 16; two regions: the header size + 0x44, rouen block
    # 0: 0x153C -> 0x1580, its csv text there; one region (mp_merv zn01
    # block 0, loadscr block 0): + 0x2C; loadscr block 1: 0x800, its IIBS);
    # every type's first parts in type order
    image = align_up(header + 0x14 + 24 * len(regions), regions[0]["align"] if regions else 16)
    place = image
    for f in files:
        place = align_up(place, types[f["type"]]["align"])
        f["at"] = place
        place += f["size"]
    image_end = place
    physical = None
    gpu = [f for f in files if len(f["sizes"]) > 1 and f["sizes"][1]]
    if gpu:
        place = physical = align_up(place, regions[1]["align"] if len(regions) > 1 else 0x10000)
        for f in gpu:
            place = align_up(place, types[f["type"]]["flags"] & 0xFFFF or 4)
            f["gpu"] = place
            place += f["sizes"][1]
    pack = Pack(d, header, types, files, image, image_end, physical, names, align_up(place, 4))
    pack.regions = regions
    pack.physical_end = place if physical is not None else None  # its region size ends here, unaligned
    return pack


class Pack:
    def __init__(self, d, header, types, files, image, image_end, physical, names, tail):
        self.d, self.header, self.types, self.files = d, header, types, files
        self.image, self.image_end, self.physical, self.names, self.tail = image, image_end, physical, names, tail

    def tail_lists(self):
        """(fix-ups, imports), or None when the tail does not end the block exactly."""
        d = self.d
        fixups, imports, at = [], [], self.tail
        while at + 4 <= len(d) and word(d, at) != 0xFFFFFFFF:
            fixups.append(word(d, at))
            at += 4
        at += 4
        while at + 4 <= len(d) and word(d, at) != 0xFFFFFFFF:
            imports.append(struct.unpack_from(">3I", d, at))
            at += 12
        return (fixups, imports) if at + 4 == len(d) else None

    def target(self, value):
        """Where a pointer word points in the block, or None (another base)."""
        base = {0: self.image, 0x3F: self.names}.get(value >> 26)
        return None if base is None else base + 4 * (value & 0x3FFFFFF)

    def name_at(self, offset):
        """A name pool record's name (offset: bytes from the pool's start)."""
        at = self.names + offset + 4
        return self.d[at:at + 28].split(b"\0")[0].decode("latin-1")

    def cpu(self, f):
        return self.d[f["at"]:f["at"] + f["size"]]

    def gpu(self, f):
        return self.d[f["gpu"]:f["gpu"] + f["sizes"][1]] if f["gpu"] is not None else b""


def name_hash(name):
    """A name record's hash (and a plain file's type tag: its extension's):
    h * 33 + c from 0 over the lowercased name (checked on 'image',
    'physical', 'cgb' -> 0x1B2CC and every record of every MP block)."""
    h = 0
    for c in name.lower().encode("latin-1"):
        h = (h * 33 + c) & 0xFFFFFFFF
    return h


def decompose(p):
    """A block's parts, for compose(): what a writer would make from scratch."""
    d = p.d
    fixups, imports = p.tail_lists()
    pool_end = p.header + 0x14
    pool = [(word(d, at), d[at + 4:at + 32]) for at in range(p.names, pool_end, 32)]
    regions = []
    for r in range(len(p.regions)):
        at = p.header + 0x14 + 24 * r
        regions.append({"name": (at + struct.unpack_from(">i", d, at)[0] - p.names) // 32,
                        "w1": word(d, at + 4), "align": word(d, at + 8), "w3": word(d, at + 12)})
    types = [{"tag": t["tag"], "version": t["version"], "parts": t["parts"], "aligns": t["aligns"]} for t in p.types]
    files = [{"type": f["type"], "name": (f["record"] - p.names) // 32, "cpu": p.cpu(f), "gpu": p.gpu(f),
              "sizes": f["sizes"]} for f in p.files]
    return {"words": struct.unpack_from(">3I", d, 4), "types": types, "files": files, "pool": pool,
            "regions": regions, "fixups": fixups, "imports": imports,
            "gaps": d[p.image_end:p.physical] if p.physical is not None else b""}


def compose(parts):
    """A block from its parts (see decompose): header (types, a zero word,
    entry tables, the name pool, region records), image, physical, tail.
    files: in type order, each {type, name (pool index), cpu, gpu, sizes};
    fixups: image word indices; imports: (image word index, tag, pool byte
    offset); a resource's pointer words are already in its cpu bytes."""
    types, files, pool, regions = parts["types"], parts["files"], parts["pool"], parts["regions"]
    nreg = len(regions)
    record = 20 + 4 * nreg
    counts = [sum(1 for f in files if f["type"] == i) for i in range(len(types))]
    entries_at = 0x1C + record * len(types) + 4
    tables, at = [], entries_at
    for i, t in enumerate(types):
        tables.append(at)
        at += 4 * (t["parts"] + 1) * counts[i]
    names = at
    pool_end = names + 32 * len(pool)
    header = pool_end - 0x14
    out = bytearray(b"APKF" + struct.pack(">3I", *parts["words"]) + struct.pack(">3I", nreg, header, 0))
    for i, t in enumerate(types):
        rel = tables[i] - (len(out) + 12)
        out += struct.pack(">5I", t["tag"], t["version"], t["parts"], rel, counts[i])
        out += struct.pack(f">{nreg}I", *t["aligns"])
    out += bytes(4)
    for i, t in enumerate(types):
        for f in (f for f in files if f["type"] == i):
            e = len(out)
            out += struct.pack(">I", names + 32 * f["name"] - e)
            out += struct.pack(f">{t['parts']}I", *f["sizes"])
    for h, raw in pool:
        out += struct.pack(">I", h) + raw
    reg_at = len(out)
    out += bytes(24 * nreg)
    image = align_up(len(out), regions[0]["align"] if regions else 16)
    out += bytes(image - len(out))
    for f in files:
        place = align_up(len(out), types[f["type"]]["aligns"][0])
        out += bytes(place - len(out)) + f["cpu"]
    image_end = len(out)
    gpu = [f for f in files if len(f["sizes"]) > 1 and f["sizes"][1]]
    phys = phys_end = None
    if gpu:
        phys = align_up(image_end, regions[1]["align"] if nreg > 1 else 0x10000)
        gaps = parts.get("gaps", b"")
        out += gaps if len(gaps) == phys - image_end else bytes(phys - image_end)
        for f in gpu:
            place = align_up(len(out), types[f["type"]]["aligns"][1] & 0xFFFF or 4)
            out += bytes(place - len(out)) + f["gpu"]
        phys_end = len(out)
    out += bytes(align_up(len(out), 4) - len(out))
    tail_at = len(out)
    for i in parts["fixups"]:
        out += struct.pack(">I", i)
    out += struct.pack(">I", 0xFFFFFFFF)
    for i, t, n in parts["imports"]:
        out += struct.pack(">3I", i, t, n)
    out += struct.pack(">I", 0xFFFFFFFF)
    sizes = [image_end - image, (phys_end - phys) if phys is not None else 0]
    starts = [image, phys if phys is not None else tail_at]
    for r, reg in enumerate(regions):
        at = reg_at + 24 * r
        struct.pack_into(">iIIIIi", out, at, names + 32 * reg["name"] - at, reg["w1"], reg["align"], reg["w3"],
                         sizes[r], starts[r] - (at + 20))
    return bytes(out)


def rebuild(p, replace):
    """The block again with some plain files' bytes replaced (replace: {name:
    bytes}): the image laid out anew, every pointer (the fix-ups' words with
    the image's base), fix-up and import index moved with its file, the
    physical region moved whole to the next 64K (its own offsets are
    relative), the region records' sizes and self-relative starts, the
    type entries' sizes. A region record: s32 name (self-relative), 0, u32
    alignment, 0, u32 size, s32 the region's start (self-relative, from this
    field)."""
    d = p.d
    for f in p.files:
        if f["name"] in replace and not f["plain"]:
            raise ValueError(f"{f['name']} is a resource, not a plain file")
    out = bytearray(d[:p.image])
    moves = []  # (old start, old end, new start), image-relative
    place = p.image
    for f in p.files:
        place = align_up(place, p.types[f["type"]]["align"])
        data = replace.get(f["name"], p.cpu(f))
        out += bytes(place - len(out))
        if f["name"] not in replace:
            moves.append((f["at"] - p.image, f["at"] - p.image + f["size"], place - p.image))
        else:
            moves.append((f["at"] - p.image, f["at"] - p.image + f["size"], None))
            e = p.types[f["type"]]["entries"] + 4 * (p.types[f["type"]]["parts"] + 1) * f["entry"]
            struct.pack_into(">I", out, e + 4, len(data))
        out += data
        place += len(data)
    image_end = len(out)

    def moved(rel):
        for start, end, new in moves:
            if start <= rel < end:
                if new is None:
                    raise ValueError(f"a pointer into a replaced file at image +{rel:X}")
                return new + rel - start
        raise ValueError(f"image +{rel:X} is in no file")

    fixups, imports = p.tail_lists()
    for i in fixups:
        at = p.image + moved(4 * i)
        value = word(d, p.image + 4 * i)
        if value >> 26 == 0:
            value = moved(4 * value) // 4
        struct.pack_into(">I", out, at, value)
    phys_len = 0
    if p.physical is not None:
        new_phys = align_up(image_end, p.regions[1]["align"] if len(p.regions) > 1 else 0x10000)
        out += d[image_end:new_phys] if new_phys - image_end == p.physical - p.image_end else bytes(new_phys - image_end)
        out += d[p.physical:p.tail]
        phys_len = p.physical_end - p.physical
    else:
        out += bytes(align_up(image_end, 4) - image_end)
    tail = bytearray()
    for i in fixups:
        tail += struct.pack(">I", moved(4 * i) // 4)
    tail += struct.pack(">I", 0xFFFFFFFF)
    for i, t, name in imports:
        tail += struct.pack(">3I", moved(4 * i) // 4, t, name)
    tail += struct.pack(">I", 0xFFFFFFFF)
    out += tail
    sizes = [image_end - p.image, phys_len]
    starts = [p.image, len(out) - len(tail) - (p.tail - p.physical if p.physical is not None else 0)]
    for r in range(len(p.regions)):
        at = p.header + 0x14 + 24 * r
        struct.pack_into(">I", out, at + 16, sizes[r])
        struct.pack_into(">i", out, at + 20, starts[r] - (at + 20))
    return bytes(out)


def main():
    if sys.argv[1:2] == ["--roundtrip"]:
        for path in sys.argv[2:]:
            archive = kapf.Archive(path)
            same = 0
            blocks = range(len(archive.blocks()))
            for b in blocks:
                d = archive.block(b)
                if d[:4] != b"APKF":
                    continue
                p = read(d)
                if p.tail_lists() is None:
                    print(f"{path} block {b}: tail does not end the block, skipped")
                    continue
                parts = decompose(p)
                hashes = all(h == name_hash(raw.split(b"\0")[0].decode("latin-1")) for h, raw in parts["pool"])
                if rebuild(p, {}) == d and compose(parts) == d:
                    same += 1
                else:
                    print(f"{path} block {b}: rebuilt bytes DIFFER (rebuild {rebuild(p, {}) == d}, compose {compose(parts) == d})")
                if not hashes:
                    print(f"{path} block {b}: a name record's hash is not name_hash(name)")
            print(f"{Path(path).name}: {same} blocks rebuilt byte for byte")
        return
    archive = kapf.Archive(sys.argv[1])
    blocks = [int(sys.argv[2])] if len(sys.argv) > 2 else range(len(archive.blocks()))
    for b in blocks:
        d = archive.block(b)
        p = read(d)
        if len(sys.argv) > 4:
            hit = next(f for f in p.files if f["name"] == sys.argv[3])
            Path(sys.argv[4]).write_bytes(p.cpu(hit))
            print(f"{sys.argv[4]}: {hit['size']} bytes from block {b} at {hit['at']:X}")
            return
        print(f"block {b}: {len(d):X} bytes, header {p.header:X}, {len(p.types)} types, names {p.names:X}, "
              f"image {p.image:X}..{p.image_end:X}, physical {p.physical or 0:X}")
        for i, ty in enumerate(p.types):
            print(f"  type {i:2} {ty['name']:9} v{ty['version']} parts {ty['parts']} count {ty['count']} align {ty['align']:X} flags {ty['flags']:08X}")
        for f in p.files:
            gpu = f"{f['gpu']:08X}" if f["gpu"] is not None else "        "
            print(f"    {f['type']:2}.{f['entry']:<3} {f['at']:08X} {gpu} {' '.join(f'{x:08X}' for x in f['sizes'])}  {f['name']}")
        lists = p.tail_lists()
        print(f"  tail {p.tail:X}: " + (f"{len(lists[0])} fix-ups, {len(lists[1])} imports" if lists else "does NOT end the block"))


if __name__ == "__main__":
    main()
