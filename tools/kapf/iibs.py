"""CoD3's zone resource index (<zone>.xenon.ib, 'IIBS'), read and written,
from the user's own game files. The game finds a zone's world cells by name
through it (<level>_NNN, then _SPLIT0, _SPLIT1, ... until one is missing).

Layout (big-endian; worked out 2026-11-01 on every MP zone's index):
  0x00 'IIBS', u32 0x4000A3D7, u32 categories (13), 0x0C the first pointer
  (to the category list), 0x10 u32 the fix-up table's offset, u32 0
  0x14 categories, 32 bytes: u32 id (0, 1, ...), name[12], u32 slots,
  ->table, u32 names, ->name list (0 and 0 when empty)
  then per category with names: the table, the name list, the strings:
    table: (u32 name_hash, u32 name index) as an implicit search tree:
      the least 2^k - 1 slots holding the names, sorted by hash, middle
      (lo + hi - 1) // 2, breadth-first (children 2s+1, 2s+2), empty (0, 0)
    name list: (->string, u32 0) in name order (names sorted by bytes)
    strings: each NUL-ended, padded to 4
  the fix-up table: u32 count, ->list (the next word); list: u32 bits, then
  each chain's first word. Pointers are FLI-style chains (tools/kapf/fli.py):
  a word's low (31 - bits) bits a signed offset from itself, the next bits
  bits the step in bytes to the chain's next word; bit 31 ends the chain
  (its low 31 bits the offset).
name_hash: tools/kapf/apkf.py (h * 33 + c from 0, lowercased).

usage: python -I iibs.py <file.xenon.ib> [more]           list the categories
       python -I iibs.py --roundtrip <file.xenon.ib> [more]  parse, write, compare
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402

CATEGORIES = ("TEXTURE", "FONT", "MESHFILE", "MESH", "ANIMFILE", "ANIM", "SCNANIM", "ANIMOFFSET", "SKELETON",
              "EFFECT", "FX", "DISCTEX", "DISCTEXSIZE")
MAGIC = b"IIBS" + struct.pack(">I", 0x4000A3D7)


def word(d, a):
    return struct.unpack_from(">I", d, a)[0]


def chains(d):
    """(bits, chain starts, {pointer word: target})."""
    table = word(d, 0x10)
    count, listed = struct.unpack_from(">II", d, table)
    bits = word(d, listed)
    starts = [word(d, listed + 4 * i) for i in range(1, count)]
    width = 31 - bits
    found = {}
    for place in starts:
        while True:
            w = word(d, place)
            if w & 0x80000000:
                v = w & 0x7FFFFFFF
                if v & 0x40000000:
                    v -= 0x80000000
                found[place] = place + v
                break
            v = w & ((1 << width) - 1)
            if v & (1 << (width - 1)):
                v -= 1 << width
            found[place] = place + v
            step = (w & 0x7FFFFFFF) >> width
            if not step:
                break
            place += step
    return bits, starts, found


def read(d):
    """{category: [names]} and the chain bits."""
    if d[:8] != MAGIC:
        raise ValueError("not an IIBS index")
    bits, _starts, ptr = chains(d)
    cats = {}
    for k in range(word(d, 8)):
        at = 0x14 + 32 * k
        name = d[at + 4:at + 16].split(b"\0")[0].decode()
        n = word(d, at + 24)
        names = []
        for i in range(n):
            s = ptr[ptr[at + 28] + 8 * i]
            names.append(d[s:d.index(b"\0", s)].decode("latin-1"))
        cats[name] = names
    return cats, bits


def tree(names):
    keys = sorted((apkf.name_hash(n), i) for i, n in enumerate(names))
    size = (1 << len(keys).bit_length()) - 1
    out = [(0, 0)] * size

    def put(lo, hi, s):
        if lo >= hi or s >= size:
            return
        m = (lo + hi - 1) // 2
        out[s] = keys[m]
        put(lo, m, 2 * s + 1)
        put(m + 1, hi, 2 * s + 2)

    put(0, len(keys), 0)
    return out


def write(cats, bits=11):
    """The index for {category: [names]} (the 13 categories in order; names
    are sorted here by bytes, as the game's are)."""
    out = bytearray(MAGIC + struct.pack(">I", len(CATEGORIES)) + bytes(8))  # 0x0C, 0x10 filled below
    pointers = {0x0C: 0x14}  # pointer word -> target
    out += bytes(32 * len(CATEGORIES))
    for k, cat in enumerate(CATEGORIES):
        at = 0x14 + 32 * k
        struct.pack_into(">I", out, at, k)
        out[at + 4:at + 16] = cat.encode().ljust(12, b"\0")
        names = sorted(cats.get(cat, []), key=lambda n: n.encode("latin-1"))
        if not names:
            continue
        slots = tree(names)
        struct.pack_into(">I", out, at + 16, len(slots))
        struct.pack_into(">I", out, at + 24, len(names))
        pointers[at + 20] = len(out)
        for h, i in slots:
            out += struct.pack(">II", h, i)
        pointers[at + 28] = len(out)
        listed = len(out)
        out += bytes(8 * len(names))
        for i, n in enumerate(names):
            pointers[listed + 8 * i] = len(out)
            raw = n.encode("latin-1") + b"\0"
            out += raw + bytes(-len(raw) % 4)
    # the chains: in address order, a new chain where the step does not fit
    width = 31 - bits
    places = sorted(pointers)
    starts = [places[0]]
    for i, place in enumerate(places):
        off = pointers[place] - place
        nxt = places[i + 1] if i + 1 < len(places) else None
        if nxt is not None and nxt - place < (1 << bits):
            if not -(1 << (width - 1)) <= off < 1 << (width - 1):
                raise ValueError(f"pointer at {place:X} out of reach")
            struct.pack_into(">I", out, place, (nxt - place) << width | (off & ((1 << width) - 1)))
        else:
            struct.pack_into(">I", out, place, 0x80000000 | (off & 0x7FFFFFFF))
            if nxt is not None:
                starts.append(nxt)
    table = len(out)
    struct.pack_into(">I", out, 0x10, table)
    out += struct.pack(">II", 1 + len(starts), table + 8)
    out += struct.pack(">I", bits) + b"".join(struct.pack(">I", s) for s in starts)
    return bytes(out)


def main():
    if sys.argv[1:2] == ["--roundtrip"]:
        same = 0
        for path in sys.argv[2:]:
            d = Path(path).read_bytes()
            cats, bits = read(d)
            new = write(cats, bits)
            if new == d:
                same += 1
            else:
                at = next((i for i in range(min(len(d), len(new))) if d[i] != new[i]), min(len(d), len(new)))
                print(f"{Path(path).name}: DIFFERS at {at:X} (sizes {len(d):X} / {len(new):X})")
        print(f"{same} of {len(sys.argv) - 2} written byte for byte")
        return
    for path in sys.argv[1:]:
        cats, bits = read(Path(path).read_bytes())
        print(f"{Path(path).name}: chain bits {bits}; " + ", ".join(f"{k} {len(v)}" for k, v in cats.items() if v))


if __name__ == "__main__":
    main()
