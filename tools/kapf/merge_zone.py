"""A zone archive for a first in-game look at a converted level, made from a
source map's zone archive (read only): the source's world cells, zone index
and collision renamed out of reach (their name records, same length,
rehashed: lookups by name no longer find them), and new blocks appended:
the converted cells and textures (cod2cells.py) and a block holding the new
collision and the new zone index (the old index's names without the old
cells, plus the new ones). Everything else (scene, light grid, props) is the
source's. A scratch harness: the real level gets its own archives.

usage: python -I merge_zone.py <source zone.cod> <out.cod> <zone name> <cells.pkl>
       <new .cgb> <cell numbers, comma separated> [--textures]
  zone name: e.g. mp_rouen_zone01 (its files' names); --textures: also put the
  converted textures here
"""
import pickle
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import block  # noqa: E402
import iibs  # noqa: E402
import kapf  # noqa: E402


def rename(data, old, new):
    """The block with a name record renamed (same length) and rehashed."""
    if len(new) != len(old):
        raise ValueError("a rename must keep the length")
    p = apkf.read(data)
    out = bytearray(data)
    hits = 0
    for at in range(p.names, p.header + 0x14, 32):
        if out[at + 4:at + 32].split(b"\0")[0] == old.encode("latin-1")[:27]:
            out[at + 4:at + 4 + len(new[:27])] = new.encode("latin-1")[:27]
            struct.pack_into(">I", out, at, apkf.name_hash(new))
            hits += 1
    return bytes(out), hits


def main():
    src, out, zone, cells_pkl, cgb_path, numbers = sys.argv[1:7]
    with_textures = "--textures" in sys.argv
    numbers = [int(n) for n in numbers.split(",") if n]
    conv = pickle.loads(Path(cells_pkl).read_bytes())
    a = kapf.Archive(src)
    parts = kapf.decompose(a)
    main_sec = parts["sections"][0]
    level = zone.rsplit("_zone", 1)[0]
    hidden = 0
    # hide the source's cells, index and collision
    for b, blk in enumerate(main_sec["blocks"]):
        data = blk["data"]
        if data[:4] != b"APKF":
            continue
        p = apkf.read(data)
        for f in p.files:
            n = f["name"]
            if (n.startswith(level + "_") and n.endswith(".xecell")) or n in (f"{zone}.xenon.ib", f"{zone}.cgb"):
                data, hits = rename(data, n, "x" + n[1:])
                hidden += hits
        blk["data"] = data
    entries = []
    for e in main_sec["entries"]:
        name = e[0]
        if (name.startswith(level + "_") and name.endswith(".xecell")) or name in (f"{zone}.xenon.ib", f"{zone}.cgb"):
            name = "x" + name[1:]
        # 2026-11-01: the source's collision out of the directory altogether.
        # Renamed, it was still loaded (the loader walks the directory by type,
        # whatever the name): Carentan's bots stood and stopped on Rouen's
        # floors and walls (z 16.0 at 432 853 where CoD2's ground is -8;
        # tools/carentan/ghost_hits.py). --keep-source-cgb: the old way.
        if name == "x" + f"{zone}.cgb"[1:] and "--keep-source-cgb" not in sys.argv:
            print(f"source collision {name} left out of the directory")
            continue
        # 2026-11-01: the source's static prop batches (.ng.xmbb / .ng.xmpbb: Rouen's wild
        # flowers, hedges, sandbags, burnt cars, Panzer husk, furniture...) out too: the
        # user saw Rouen's dry grass on Carentan's streets. --keep-source-props: kept.
        if name in (f"{zone}.ng.xmbb", f"{zone}.ng.xmpbb") and "--keep-source-props" not in sys.argv:
            print(f"source props {name} left out of the directory")
            continue
        # 2026-11-01 (test): the source's own cells (renamed xp_..., still loaded by the
        # directory walk) out: in Rouen's other outdoor cells (007, 000) the camera saw
        # Carentan (all in cell 001) only through Rouen's portals, and went white
        if "--drop-source-cells" in sys.argv and name.startswith("x" + level[1:] + "_") and name.endswith(".xecell"):
            print(f"source cell {name} left out of the directory")
            continue
        entries.append((name, e[1], e[2], e[3], e[4]))
    print(f"hidden: {hidden} name records")
    if "--drop-assets" in sys.argv:
        # the source's blocks holding only drawn assets (meshes, materials, textures, cell
        # bundles: its props and its own, hidden, cells) out: every block takes one of the
        # game's 96 banks of 3 MB, shared with its own archives; blocks with any plain
        # file (zone scene, light grid, brush models, ...) or FX stay
        drop = set()
        for b, blk in enumerate(main_sec["blocks"]):
            if blk["data"][:4] == b"APKF":
                p = apkf.read(blk["data"])
                if {p.types[f["type"]]["name"] for f in p.files} <= {"MESH", "MAT", "TEX", "MLTI"}:
                    drop.add(b)
        renum = {}
        for b in range(len(main_sec["blocks"])):
            if b not in drop:
                renum[b] = len(renum)
        main_sec["blocks"] = [blk for b, blk in enumerate(main_sec["blocks"]) if b not in drop]
        entries = [(e[0], e[1], e[2], renum[e[3]], e[4]) for e in entries if e[3] not in drop]
        print(f"source asset-only blocks dropped: {sorted(drop)}")
    # the old index's names, without the source's cells
    old_ib = None
    for blk in main_sec["blocks"]:
        if blk["data"][:4] != b"APKF":
            continue
        p = apkf.read(blk["data"])
        for f in p.files:
            if f["name"] == "x" + f"{zone}.xenon.ib"[1:]:
                old_ib = p.cpu(f)
    cats, bits = iibs.read(old_ib)
    cats["MESHFILE"] = [n for n in cats["MESHFILE"] if not n.startswith(level + "_")]
    cats["MESH"] = [n for n in cats["MESH"] if not n.startswith("cell_")]
    # the converted blocks for these cells (and the textures)
    first = len(main_sec["blocks"])
    keep = {}

    def tex_only(data):
        p = apkf.read(data)
        return {p.types[f["type"]]["name"] for f in p.files} == {"TEX"}

    # textures before the cells: a material's texture imports are resolved as its block
    # loads (test 3: cells drawn, all white, the textures' blocks came after them)
    order = sorted(range(len(conv["blocks"])), key=lambda b: not tex_only(conv["blocks"][b]))
    for b in order:
        data = conv["blocks"][b]
        p = apkf.read(data)
        kinds = {p.types[f["type"]]["name"] for f in p.files}
        mine = any(f["name"].startswith(tuple(f"{level}_{n:03d}_split" for n in numbers)) for f in p.files)
        if mine or (with_textures and kinds == {"TEX"}):
            keep[b] = len(keep)
            main_sec["blocks"].append({"data": data, "bank": block_bank(data), "flags": 0x82})
            for f in p.files:
                kind = p.types[f["type"]]["name"]
                if kind == "TEX":
                    cats["TEXTURE"].append(f["name"])
                elif kind == "MESH":
                    cats["MESH"].append(f["name"])
    for e in conv["entries"]:
        if e[3] in keep:
            entries.append((e[0], e[1], e[2], first + keep[e[3]], e[4]))
    for n in numbers:
        cats["MESHFILE"] += conv["cells"].get(n, [])
    # the new collision and index in a block of their own
    cgb = Path(cgb_path).read_bytes()
    ib = iibs.write(cats, bits)
    extra = block.build([("cgb", f"{zone}.cgb", cgb, {}, None), ("xenon.ib", f"{zone}.xenon.ib", ib, {}, None)])
    p = apkf.read(extra)
    # first: the index must be loaded before the resources it names, which are bound to
    # its entries as they load (sub_82441A58 -> sub_82474FD8 -> sub_820CFF58 returns the
    # entry's bound pointer; test 1 and A put it last: nothing bound, nothing drawn)
    main_sec["blocks"].insert(0, {"data": extra, "bank": block_bank(extra), "flags": 0x82})
    entries = [(e[0], e[1], e[2], e[3] + 1, e[4]) for e in entries]
    new = [(f["name"], f"c:\\cod2\\{f['name']}", f["size"], 0, f["type"] << 16 | f["entry"]) for f in p.files]
    # the index first in the directory, as in every game archive (the old, renamed one
    # was first in test 2 and nothing of ours was bound)
    ib = [e for e in new if e[0].endswith(".xenon.ib")]
    # The loader dispatches resources in DIRECTORY order, not block order.
    # 690 trace: MAT imports missed before the c2 TEX handlers ran. All 41
    # shipped MP zones with textures and cells list every .xetex before any
    # .xecell. Merely placing TEX blocks first leaves their handlers too late.
    rest = [e for e in entries if not e[0].endswith(".xenon.ib")] + [e for e in new if e not in ib]
    main_sec["entries"] = ib + [e for e in rest if e[0].endswith(".xetex")] + [e for e in rest if not e[0].endswith(".xetex")]
    data = kapf.compose(parts)
    Path(out).write_bytes(data)
    print(f"{out}: {len(data) / 1e6:.1f} MB, {len(main_sec['blocks'])} blocks ({len(keep)} converted, 1 new "
          f"collision/index), {len(entries)} entries; cells {numbers}: "
          f"{sum(len(conv['cells'].get(n, [])) for n in numbers)} splits; index: "
          + ", ".join(f"{k} {len(v)}" for k, v in cats.items() if v))


def block_bank(data):
    return max(0x300000, (len(data) + 0xFFFF) & ~0xFFFF)


if __name__ == "__main__":
    main()
