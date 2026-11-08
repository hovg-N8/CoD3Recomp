"""A level's FLI (its manifest, the FLI section of <map>.cod), read from the
user's own game files, and moved to another folder.

Layout (big-endian; from all 21 MP levels, base and packs, 2026-10-31):
  0x00 'FLIN', f32 1.07, u32 7 or 15
  0x18 u32: where the fix-up table is (the block's last 16 bytes in every
       level): u32 count (2), u32 offset of a list: u32 bits (9), then
       count-1 chain starts (0x0C).
  a chain (sub_820F9968 / sub_820F9910 / sub_820F99D8 fix it up on load):
       each word is a pointer; its low 22 bits (31 less the 9) are a signed
       offset from the word itself, the 9 bits above them the distance in bytes to
       the chain's next word. A word with bit 31 set ends the chain (its
       low 31 bits are the offset; bit 30 set keeps bit 31 too).
  strings are NUL-ended and packed on 4-byte boundaries; every path
  ("mp\\rouen\\zn01.cod", "mp\\weapusge.cod") has exactly one pointer.
In its .cod the FLI is the last block, alone in its 64 KB page (0xF000 or
more to spare in every level), so it can grow without moving anything.
New strings go after the table: they are not pointers, so the chains and
the table stay where they are (0.5.668 test: moving the table word sent the
fix-up walk into the new strings and hung the load).

usage:
  python -I fli.py <archive.cod>        its paths and the pointers to them
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

PAGE = 0x10000


def word(data, at):
    return struct.unpack_from(">I", data, at)[0]


def chain_bits(data):
    table = word(data, 0x18)
    count, listed = struct.unpack_from(">II", data, table)
    bits = word(data, listed)
    starts = [word(data, listed + 4 * i) for i in range(1, count)]
    return bits, starts


def pointers(data):
    """{place: target} for every pointer word, by walking the chains."""
    bits, starts = chain_bits(data)
    width = 31 - bits
    found = {}
    for place in starts:
        while True:
            if place in found or not 0 <= place < len(data) - 3:
                raise ValueError(f"FLI: a fix-up chain runs to {place:X}")
            w = word(data, place)
            if w & 0x80000000:
                v = w if w & 0x40000000 else w & 0x7FFFFFFF
                found[place] = (place + (v - (1 << 32) if v & 0x80000000 else v)) & 0xFFFFFFFF
                break
            v = w & ((1 << width) - 1)
            if v & (1 << (width - 1)):
                v -= 1 << width
            found[place] = place + v
            step = (w & 0x7FFFFFFF) >> width
            if not step:
                break
            place += step
    return found


def paths(data):
    """[(at, text, [pointer words' places])] for every mp\\ path."""
    found = []
    at = data.find(b"mp\\")
    while at >= 0:
        end = data.index(b"\0", at)
        found.append((at, data[at:end].decode("latin-1"), []))
        at = data.find(b"mp\\", end)
    where = {f[0]: f[2] for f in found}
    for place, target in sorted(pointers(data).items()):
        if target in where:
            where[target].append(place)
    return found


def repoint(data, place, target):
    bits, _ = chain_bits(data)
    width = 31 - bits
    w = word(data, place)
    if w & 0x80000000:
        raise ValueError(f"FLI: the pointer at {place:X} ends its chain: not moved")
    off = target - place
    if not -(1 << (width - 1)) <= off < 1 << (width - 1):
        raise ValueError(f"FLI: {off:X} is out of a pointer's reach")
    mask = (1 << width) - 1
    struct.pack_into(">I", data, place, (w & ~mask) | (off & mask))


def move(data, old, new):
    """The FLI block (bytes) with its mp\\<old>\\ paths in mp\\<new>\\ and
    <old>.x renamed <new>.x: in place when the new path fits the old one's
    room, else appended after the block's end and its pointer moved there."""
    out = bytearray(data)
    changed = []
    for at, text, refs in paths(data):
        head = "mp\\" + old + "\\"
        if not text.lower().startswith(head.lower()):
            continue
        if len(refs) != 1:
            raise ValueError(f"FLI: '{text}' has {len(refs)} pointers (1 expected): not moved")
        rest = text[len(head):]
        if rest.lower().startswith(old.lower() + "."):
            rest = new + rest[len(old):]
        moved = ("mp\\" + new + "\\" + rest).encode("latin-1")
        room = (len(text) + 1 + 3) & ~3
        if len(moved) + 1 <= room:
            out[at:at + room] = moved + bytes(room - len(moved))
        else:
            out[at:at + room] = bytes(room)
            place = len(out)
            out += moved + bytes(((len(moved) + 1 + 3) & ~3) - len(moved))
            repoint(out, refs[0], place)
        changed.append((text, moved.decode("latin-1")))
    if not changed:
        raise ValueError(f"FLI: no 'mp\\{old}\\' path")
    before = pointers(data)
    after = pointers(out)
    if set(before) != set(after):
        raise ValueError("FLI: the fix-up chains changed: not written")
    return bytes(out), changed


def replace_block(archive, data, fli):
    """The .cod bytes (bytearray, changed in place) with its FLI block
    replaced by fli: the block table's used and bank sizes and the file's
    directory size follow it."""
    names = [s[0] for s in archive.sections]
    section = names.index("FLI")
    _n, blocks, entries, table, directory = archive.sections[section]
    if blocks != 1 or entries != 1:
        raise ValueError(f"FLI section: {blocks} blocks, {entries} files (1 and 1 expected)")
    off, used, bank, _flags, stored = struct.unpack_from("<5I", data, table)
    if stored:
        raise ValueError("a compressed FLI block (campaign archive?): not handled")
    if off + used > len(data) or (off + max(used, bank) + PAGE - 1) // PAGE * PAGE != len(data):
        raise ValueError("the FLI block is not the last page of the file: not handled")
    if off % PAGE + len(fli) > PAGE:
        raise ValueError(f"the new FLI ({len(fli)} bytes) does not fit its page")
    data[off:off + PAGE - off % PAGE] = fli + bytes(PAGE - off % PAGE - len(fli))
    struct.pack_into("<II", data, table + 4, len(fli), len(fli))
    size, = struct.unpack_from("<I", data, directory + 8)
    if size & 0xFFFFFF != used:
        raise ValueError(f"FLI directory size {size:X}, block {used:X}: not handled")
    struct.pack_into("<I", data, directory + 8, (size & ~0xFFFFFF) | len(fli))


def main():
    import kapf
    archive = kapf.Archive(sys.argv[1])
    data = archive.block(0, [s[0] for s in archive.sections].index("FLI"))
    bits, starts = chain_bits(data)
    print(f"FLI {len(data)} bytes, fix-up table at {word(data, 0x18):X} ({bits} bits, chains from "
          + ", ".join(f"{s:X}" for s in starts) + f"), {len(pointers(data))} pointers")
    for at, text, refs in paths(data):
        print(f"  {at:04X} {text:34} from " + ", ".join(f"{r:04X}" for r in refs))


if __name__ == "__main__":
    main()
