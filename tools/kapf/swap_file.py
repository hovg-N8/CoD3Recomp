"""A copy of a .cod archive with plain files in its MAIN section replaced
(another size is fine): each file's block rebuilt by apkf.rebuild (pointers
relocated), written back in its own bank (it must still fit the bank), the
block table's used size and the file's directory size set to match. The
source archive is only read.

usage: python -I swap_file.py <archive.cod> <out.cod> <name> <file> [<name> <file> ...]
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402


def swap(src, replace):
    archive = kapf.Archive(src)
    data = bytearray(Path(src).read_bytes())
    _n, nblocks, nfiles, table, directory = archive.sections[0]
    files = archive.files(0)
    by_block = {}
    for name in replace:
        hits = [i for i, f in enumerate(files) if f[0] == name]
        if len(hits) != 1:
            raise ValueError(f"{name}: {len(hits)} directory entries")
        by_block.setdefault(files[hits[0]][3], []).append((hits[0], name))
    for b, entries in by_block.items():
        off, used, bank, _flags, stored = struct.unpack_from("<5I", data, table + 32 * b)
        if stored:
            raise ValueError(f"block {b} is compressed: not handled")
        block = apkf.rebuild(apkf.read(archive.block(b)), {name: replace[name] for _, name in entries})
        if len(block) > bank:
            raise ValueError(f"block {b}: {len(block):X} bytes do not fit its bank of {bank:X}")
        data[off:off + max(used, len(block))] = block + bytes(max(used, len(block)) - len(block))
        struct.pack_into("<I", data, table + 32 * b + 4, len(block))
        for i, name in entries:
            at = directory + 20 * i + 8
            size, = struct.unpack_from("<I", data, at)
            if size & 0xFFFFFF != len(apkf.read(archive.block(b)).cpu(next(
                    f for f in apkf.read(archive.block(b)).files if f["name"] == name))):
                raise ValueError(f"{name}: directory size {size:X} is not the file's size")
            struct.pack_into("<I", data, at, (size & ~0xFFFFFF) | len(replace[name]))
    return bytes(data)


def main():
    src, out = sys.argv[1:3]
    pairs = sys.argv[3:]
    replace = {pairs[i]: Path(pairs[i + 1]).read_bytes() for i in range(0, len(pairs), 2)}
    data = swap(src, replace)
    Path(out).write_bytes(data)
    check = kapf.Archive(out)
    for name, new in replace.items():
        f = next(f for f in check.files(0) if f[0] == name)
        p = apkf.read(check.block(f[3]))
        got = p.cpu(next(x for x in p.files if x["name"] == name))
        print(f"{out}: {name} {'in place' if got == new else 'WRONG'} ({len(new)} bytes, block {f[3]}, directory size {f[2] & 0xFFFFFF:X})")


if __name__ == "__main__":
    main()
