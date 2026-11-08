"""CoD3 .cod archives (KAPF), read from the user's own game files.

Layout (little-endian; from rouen.cod and the 2026-10-20 study):
  0x00 'KAPF', f32 version (2.06), u32 48, u32 section count, u32 directory
       offset, u32, u32, char[] name at 0x1C ("CODLOCS")
  0x3C main section: u16 blocks, u16 entries, u32 block table, u32 directory
  0x48 more sections, 24 bytes: char[12] name, u16 blocks, u16 entries,
       u32 block table, u32 directory (AUDIO, FLI, ANIM, STRING)
  block table, 32 bytes a block: u32 file offset, u32 used, u32 bank size,
       u32 flags (0x82 ...), u32 stored size (0: raw; else NCH/LZO1X chunks)
  directory, 20 bytes an entry: u32 name offset, u32 path offset, u32 size
       (low 24 bits?) and flags, u32 block, u32 index
  a block: an 'APKF' pack (big-endian inside).

usage:
  python -I kapf.py <archive.cod>               header, sections, blocks, files by type
  python -I kapf.py <archive.cod> --files       every file
  python -I kapf.py <archive.cod> --block N out write block N (decompressed) to out
"""
import collections
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lzo  # noqa: E402


def cstr(d, at):
    end = d.index(b"\0", at)
    return d[at:end].decode("latin-1")


class Archive:
    def __init__(self, path):
        self.path = Path(path)
        self.f = open(path, "rb")
        head = self.f.read(0x200000)
        self.head = head
        magic, version, size, sections, diroff = struct.unpack_from("<4sfIII", head, 0)
        if magic != b"KAPF":
            raise ValueError(f"{path}: not a KAPF archive")
        self.version, self.name = version, cstr(head, 0x1C)
        self.sections = []
        blocks, entries, table, directory = struct.unpack_from("<HHII", head, 0x3C)
        self.sections.append(("MAIN", blocks, entries, table, directory))
        for i in range(sections - 1):
            at = 0x48 + 24 * i
            name = head[at:at + 12].split(b"\0")[0].decode("latin-1")
            b, e, t, dr = struct.unpack_from("<HHII", head, at + 12)
            self.sections.append((name, b, e, t, dr))

    def blocks(self, section=0):
        _n, count, _e, table, _d = self.sections[section]
        return [struct.unpack_from("<8I", self.head, table + 32 * i) for i in range(count)]

    def files(self, section=0):
        name, _b, count, _t, directory = self.sections[section]
        out = []
        for i in range(count):
            n, p, size, block, index = struct.unpack_from("<5I", self.head, directory + 20 * i)
            try:
                out.append((cstr(self.head, n), cstr(self.head, p), size, block, index))
            except (ValueError, IndexError):
                out.append((f"?{n:x}", f"?{p:x}", size, block, index))
        return out

    def block(self, i, section=0):
        off, used, bank, flags, stored = self.blocks(section)[i][:5]
        self.f.seek(off)
        if stored == 0:
            return self.f.read(used)
        raw = self.f.read(stored)
        out, at = bytearray(), 0
        while at + 32 <= len(raw) and raw[at:at + 3] == b"NCH":
            csize, _crc, usize = struct.unpack_from(">III", raw, at + 4)
            out += lzo.decompress(raw[at + 32:at + 32 + csize], usize)
            at = (at + 32 + csize + 0xFFFF) & ~0xFFFF
        return bytes(out)


PAGE = 0x10000


def decompose(a):
    """An archive's parts for compose(): its name, version and sections
    [{name, blocks: [{data, bank, flags, used}], entries: [(name, path,
    size field, block, index)]}]. A compressed block (the campaign's, e.g.
    Saint-Lo's vehicles.cod) comes out decompressed, so compose() writes it
    plain (stored 0), as Merville's are (2026-11-05, the Panzer IV's cut)."""
    sections = []
    for s, (name, _nb, _ne, _t, _d) in enumerate(a.sections):
        blocks = []
        for i, b in enumerate(a.blocks(s)):
            off, used, bank, flags, stored = b[:5]
            if stored:
                data = a.block(i, s)
                if len(data) != used:
                    raise ValueError(f"block {i}: {len(data)} bytes decompressed, {used} expected")
            else:
                a.f.seek(off)
                data = a.f.read(used)
            blocks.append({"data": data, "bank": bank, "flags": flags})
        sections.append({"name": name, "blocks": blocks, "entries": a.files(s)})
    return {"version": a.version, "title": a.name, "sections": sections}


def compose(parts):
    """A .cod from its parts (little-endian): 'KAPF', f32 version, u32 0x30,
    u32 sections, u32 the first directory, u32 the paths' start, u32 the
    strings' end; the title (32 bytes) at 0x1C; MAIN's record at 0x3C (u16
    blocks, u16 entries, ->block table, ->directory), the others from 0x48
    (name[12] and the same); every section's block table (32 bytes a block:
    offset, used, bank, flags, 0 x4); per section its directory (20 bytes:
    ->name, ->path, size field, block, index) then its names; then every
    path; strings NUL-ended and padded to 4. Blocks from the next 64K page,
    each at the previous one's end rounded up to 64K; the file ends on 64K."""
    secs = parts["sections"]
    head = bytearray(0x3C + 12 + 24 * (len(secs) - 1))
    tables = []
    for s in secs:
        tables.append(len(head))
        head += bytes(32 * len(s["blocks"]))
    dirs, name_at = [], []
    for s in secs:
        dirs.append(len(head))
        head += bytes(20 * len(s["entries"]))
        names = []
        for e in s["entries"]:
            names.append(len(head))
            raw = e[0].encode("latin-1") + b"\0"
            head += raw + bytes(-len(raw) % 4)
        name_at.append(names)
    paths_start = len(head)
    path_at = []
    for s in secs:
        ps = []
        for e in s["entries"]:
            ps.append(len(head))
            raw = e[1].encode("latin-1") + b"\0"
            head += raw + bytes(-len(raw) % 4)
        path_at.append(ps)
    end = len(head)
    struct.pack_into("<4sfIIIII", head, 0, b"KAPF", parts["version"], 0x30, len(secs), dirs[0], paths_start, end)
    head[0x1C:0x3C] = parts["title"].encode("latin-1").ljust(32, b"\0")
    for k, s in enumerate(secs):
        rec = struct.pack("<HHII", len(s["blocks"]), len(s["entries"]), tables[k], dirs[k])
        if k == 0:
            head[0x3C:0x48] = rec
        else:
            at = 0x48 + 24 * (k - 1)
            head[at:at + 24] = s["name"].encode("latin-1").ljust(12, b"\0") + rec
        for i, e in enumerate(s["entries"]):
            struct.pack_into("<5I", head, dirs[k] + 20 * i, name_at[k][i], path_at[k][i], e[2], e[3], e[4])
    out = bytearray(head)
    place = (len(out) + PAGE - 1) // PAGE * PAGE
    for k, s in enumerate(secs):
        for i, b in enumerate(s["blocks"]):
            out += bytes(place - len(out))
            struct.pack_into("<5I", out, tables[k] + 32 * i, place, len(b["data"]), b["bank"], b["flags"], 0)
            out += b["data"]
            place = (len(out) + PAGE - 1) // PAGE * PAGE
    out += bytes(place - len(out))
    return bytes(out)


def main():
    if sys.argv[1:2] == ["--roundtrip"]:
        for path in sys.argv[2:]:
            a = Archive(path)
            try:
                parts = decompose(a)
            except ValueError as e:
                print(f"{Path(path).name}: {e}")
                continue
            same = compose(parts) == Path(path).read_bytes()
            print(f"{Path(path).parent.name}/{Path(path).name}: {'byte for byte' if same else 'DIFFERS'}")
        return
    a = Archive(sys.argv[1])
    if "--block" in sys.argv:
        i = sys.argv.index("--block")
        data = a.block(int(sys.argv[i + 1]))
        Path(sys.argv[i + 2]).write_bytes(data)
        print(f"block {sys.argv[i + 1]}: {len(data)} bytes")
        return
    print(f"{a.path.name}: KAPF {a.version:.2f} '{a.name}', sections " + ", ".join(f"{s[0]} ({s[1]} blocks, {s[2]} files)" for s in a.sections))
    for si, s in enumerate(a.sections):
        files = a.files(si)
        if "--files" in sys.argv:
            for f in files:
                print(f"  [{s[0]}] {f[1]}{'' if f[1].endswith(('/', chr(92))) or not f[1] else '/'}{f[0]}  size {f[2]:08X} block {f[3]} index {f[4]:08X}")
        else:
            kinds = collections.Counter(Path(f[0]).suffix.lower() for f in files)
            print(f"  {s[0]}: " + ", ".join(f"{k or '(none)'} {v}" for k, v in kinds.most_common()))
    if "--files" not in sys.argv:
        for i, b in enumerate(a.blocks()[:4]):
            print("  block", i, " ".join(f"{x:08X}" for x in b))


if __name__ == "__main__":
    main()
