"""World at War audit, step 0: what is in a zone? Reads the user's own fastfiles,
never writes to the install.

usage: python -I survey.py <file.ff> [more.ff ...]
"""
import collections
import re
import struct
import sys
import zlib
from pathlib import Path


def zone_of(path):
    data = Path(path).read_bytes()
    if data[:8] != b"IWffu100":
        raise ValueError("not a World at War fastfile")
    return zlib.decompress(data[12:])


def script_string_end(zone):
    count = struct.unpack_from("<I", zone, 0x24)[0]
    at = 0x34 + 4 * count
    for i in range(count):
        if struct.unpack_from("<i", zone, 0x34 + 4 * i)[0] == 0:
            continue
        at = zone.index(b"\0", at) + 1
    return count, at


# a raw file as written in the zone: name -1, compressed size, size, data -1, name
RAW = re.compile(rb"\xff\xff\xff\xff(....)(....)\xff\xff\xff\xff([\w./\\-]{3,120}\.[a-zA-Z_]{1,8})\0", re.S)


def main():
    for p in sys.argv[1:]:
        zone = zone_of(p)
        count, end = script_string_end(zone)
        assets = struct.unpack_from("<I", zone, 0x2C)[0]
        print(f"== {Path(p).name}: zone {len(zone):,} bytes, {count} script strings, {assets} assets, list at {end:#x}")
        types = collections.Counter()
        for i in range(assets):
            t, ptr = struct.unpack_from("<Ii", zone, end + 8 * i)
            types[t] += 1
        print("   types:", dict(sorted(types.items())))
        hits = 0
        exts = collections.Counter()
        for m in RAW.finditer(zone):
            clen, size = struct.unpack("<II", m.group(1) + m.group(2))
            name = m.group(3).decode("latin-1")
            head = zone[m.end():m.end() + 2]
            exts[name.rsplit(".", 1)[1]] += 1
            if hits < 4:
                print(f"   raw {name}: compressed {clen} size {size} starts {head.hex()}")
            hits += 1
        print(f"   raw-shaped entries: {hits} {dict(exts)}")


if __name__ == "__main__":
    main()
