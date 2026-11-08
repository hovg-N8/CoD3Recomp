"""A converted map's archive repacked into fewer blocks (2026-11-02, Seelow):
every block takes one of the game's 96 banks of 3 MB however little it
holds, and Seelow's level archive (Rouen's, the harness) was 18 blocks, a
third of them Rouen's props' textures. Two steps:

  stand-ins: the textures nothing kept reaches (from the archive's plain
    files, effects, skeletons and the models named by --keep, through every
    pointer and import, and every texture another loaded archive imports)
    become 8x8 textures of the same name and format: every import by name
    still finds one, nothing is left dangling.
  repack: the files laid into as few blocks as fit in a bank. A merged
    block's name pool is its sources' pools one after another (a pool is a
    string table: unsorted, with duplicates, in the game's own); every
    pointer word (base 0: the image, moved with its file; 0x3F: the pool,
    moved with its source's pool), fix-up and import moved; files that point
    at each other's images stay in one block. A texture's or mesh's GPU
    offsets are relative to its own GPU part (the loader adds its place), so
    they move as they are. The directory keeps its order (the loader
    dispatches by it), each entry given its file's new block and index.
    Imports may name files in later blocks (Rouen 239, Merville 5437 times):
    the order of blocks is free.

Checked on the result: every file once, every fix-up pointing at the same
file and offset (or pool name) as before, every import the same name.

usage: python -I repack.py <in.cod> <out.cod> --keep <regex> [--outside-archives <a.cod|b.cod|...>]
  --keep: models (MESH/MLTI) to keep by name, e.g. "^(c_|w_|p_mp_|sky)"
  --outside-archives: the other archives loaded with the map (global, weapons,
    campaign ones): every texture they import by name stays real
  (--outside <names.txt>: the same as a list, one a line, first column)
"""
import collections
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402
import xetex  # noqa: E402

BANK = 0x300000


def align(x, a):
    return (x + a - 1) & ~(a - 1)


class Source:
    """One source block: its pack, parts and each file's fix-ups and imports."""

    def __init__(self, index, blk):
        self.index = index
        self.flags = blk["flags"]
        self.p = apkf.read(blk["data"])
        self.parts = apkf.decompose(self.p)
        lists = self.p.tail_lists()
        if lists is None:
            raise ValueError(f"block {index}: its tail does not end the block")
        fixups, imports = lists
        p = self.p
        self.spans = [(f["at"] - p.image, f["at"] - p.image + f["size"], k) for k, f in enumerate(p.files)]
        self.fixups = collections.defaultdict(list)    # file -> [(word in file, value)]
        self.imports = collections.defaultdict(list)   # file -> [(word in file, tag, pool byte offset)]
        self.links = collections.defaultdict(set)      # file -> files its base-0 pointers reach
        for i in fixups:
            k, rel = self.owner(4 * i)
            value = apkf.word(p.d, p.image + 4 * i)
            self.fixups[k].append((rel // 4, value))
            if value >> 26 == 0:
                t, _ = self.owner(4 * (value & 0x3FFFFFF))
                if t != k:
                    self.links[k].add(t)
        for i, tag, n in imports:
            k, rel = self.owner(4 * i)
            self.imports[k].append((rel // 4, tag, n))

    def owner(self, rel):
        for s, e, k in self.spans:
            if s <= rel < e:
                return k, rel - s
        raise ValueError(f"block {self.index}: image +{rel:X} is in no file")


def stand_in(src, k):
    """File k of a source (a TEX) as an 8x8 texture of its format: the
    description's own pointer words (+0x10 its name, +0x14 its D3D object)
    kept, the fetch constant and sizes new; the GPU part one 4 KB surface."""
    f = src.parts["files"][k]
    old = f["cpu"]
    fmt = xetex.describe(old)["format"]
    desc, _ = xetex.tex_desc(8, 8, fmt, 0)
    desc = bytearray(desc)
    desc[0x10:0x18] = old[0x10:0x18]
    side, nbytes = xetex.BLOCK[fmt]
    n = (8 // side) ** 2
    if fmt == 0x12:
        level = struct.pack(">HHI", 0x8410, 0x8410, 0)   # grey (never seen: nothing kept draws it)
    elif fmt in (0x13, 0x14):
        level = bytes(8) + struct.pack(">HHI", 0x8410, 0x8410, 0)
    else:
        level = bytes([0x80]) * nbytes
    gpu = xetex.tex_gpu(bytes(desc), {0: level * n})
    return {"type": f["type"], "name": f["name"], "cpu": bytes(desc) + old[len(desc):] if len(old) > len(desc) else bytes(desc),
            "gpu": bytes(gpu), "sizes": [len(old), len(gpu)] + f["sizes"][2:]}


def type_key(t):
    return (t["tag"], t["version"], t["parts"], tuple(t["aligns"]))


def main():
    src_path, out_path = sys.argv[1], sys.argv[2]
    keep_re = re.compile(sys.argv[sys.argv.index("--keep") + 1], re.I)
    outside = set()
    if "--outside-archives" in sys.argv:
        for path in sys.argv[sys.argv.index("--outside-archives") + 1].split("|"):
            try:
                other = kapf.decompose(kapf.Archive(path))
            except ValueError as e:   # (compressed campaign archives: not read)
                print(f"outside: {path} not read ({e})")
                continue
            for s in other["sections"]:
                for blk in s["blocks"]:
                    if blk["data"][:4] != b"APKF":
                        continue
                    p = apkf.read(blk["data"])
                    lists = p.tail_lists()
                    for _i, tag, n in (lists[1] if lists else ()):
                        if apkf.tag_text(tag) == "TEX":
                            outside.add(p.name_at(n).lower())
        print(f"outside: {len(outside)} texture names other archives import")
    if "--outside" in sys.argv:
        for line in Path(sys.argv[sys.argv.index("--outside") + 1]).read_text(encoding="latin-1").splitlines():
            if line.strip():
                outside.add(line.split("\t")[0].strip().lower())
    archive = kapf.Archive(src_path)
    parts = kapf.decompose(archive)
    sec = parts["sections"][0]
    sources = [Source(b, blk) for b, blk in enumerate(sec["blocks"])]

    # 0.5.731: --graft <other.cod> --graft-names <regex>: files of another
    # archive brought in (Longyard's night sky for Seelow Night): its listed
    # files whose names match, and every file they point at or import (found
    # in that archive), in place of this archive's files of the same names.
    grafted, displaced, graft_entries = set(), set(), []
    if "--graft" in sys.argv:
        gparts = kapf.decompose(kapf.Archive(sys.argv[sys.argv.index("--graft") + 1]))
        gre = re.compile(sys.argv[sys.argv.index("--graft-names") + 1], re.I)
        gsec = gparts["sections"][0]
        first = len(sources)
        gsources = [Source(first + b, blk) for b, blk in enumerate(gsec["blocks"])]
        sources += gsources
        gnames = collections.defaultdict(list)
        for s in gsources:
            for k, f in enumerate(s.p.files):
                gnames[f["name"].lower()].append((s.index, k))
        roots, gentries = [], []
        for e in gsec["entries"]:
            s = sources[first + e[3]]
            k = next(n for n, f in enumerate(s.p.files) if f["type"] == e[4] >> 16 and f["entry"] == e[4] & 0xFFFF)
            gentries.append(((s.index, k), (e[0], e[1], e[2], s.index, e[4])))
            if gre.search(e[0]):
                roots.append((s.index, k))
        stack = list(roots)
        while stack:
            key = stack.pop()
            if key in grafted:
                continue
            grafted.add(key)
            s = sources[key[0]]
            stack += [(s.index, t) for t in s.links.get(key[1], ())]
            stack += [x for _w, _t, n in s.imports.get(key[1], ()) for x in gnames.get(s.p.name_at(n).lower(), [])]
        graft_entries = [e for key, e in gentries if key in grafted]   # (every grafted file it lists)
        names = {sources[b].p.files[k]["name"].lower() for b, k in grafted}
        for s in sources[:first]:
            for k, f in enumerate(s.p.files):
                if f["name"].lower() in names:
                    displaced.add((s.index, k))
        print(f"graft: {len(grafted)} files ({len(graft_entries)} listed: {', '.join(e[0] for e in graft_entries)}), "
              f"{len(displaced)} of this archive's of the same names out")

    # 0.5.729 (zones): files out altogether. The directory lists only what the
    # loader takes (bundles, plain files, textures); a bundle's meshes and
    # materials hang off it by pointers. A file stays if it is listed (and not
    # named by --drop: Rouen's renamed, still listed old cells) or a kept
    # file's pointers reach it, or anything here or outside imports its name;
    # the rest (Rouen's renamed index and collision, prop batches, its cells'
    # meshes and materials) leave the blocks and the directory.
    listed = set()
    for e in sec["entries"]:
        s = sources[e[3]]
        k = next(n for n, f in enumerate(s.p.files) if f["type"] == e[4] >> 16 and f["entry"] == e[4] & 0xFFFF)
        listed.add((e[3], k))
    for e in graft_entries:
        s = sources[e[3]]
        listed.add((e[3], next(n for n, f in enumerate(s.p.files) if f["type"] == e[4] >> 16 and f["entry"] == e[4] & 0xFFFF)))
    drop_re = re.compile(sys.argv[sys.argv.index("--drop") + 1], re.I) if "--drop" in sys.argv else None
    # --follow-imports (a pack cut out of a level, the vehicles'): what kept
    # files import by name is kept too, and only those imports protect a file
    # (without it, any import from any file in the archive protects its target)
    follow = "--follow-imports" in sys.argv
    imported = set(outside)
    if not follow:
        for s in sources:
            for k, lst in s.imports.items():
                for _w, _tag, n in lst:
                    imported.add(s.p.name_at(n).lower())
    names_at = collections.defaultdict(list)
    for s in sources:
        for k, f in enumerate(s.p.files):
            names_at[f["name"].lower()].append((s.index, k))
    removed = set()
    if drop_re is not None:
        exist = {key for key in listed if not drop_re.search(sources[key[0]].p.files[key[1]]["name"])}
        stack = list(exist)
        while stack:
            key = stack.pop()
            s = sources[key[0]]
            nxt = [(s.index, t) for t in s.links.get(key[1], ())]
            if follow:
                nxt += [x for _w, _tag, n in s.imports.get(key[1], ()) for x in names_at.get(s.p.name_at(n).lower(), [])]
            for x in nxt:
                if x not in exist:
                    exist.add(x)
                    stack.append(x)
        for s in sources:
            for k, f in enumerate(s.p.files):
                if (s.index, k) not in exist and f["name"].lower() not in imported:
                    removed.add((s.index, k))
        # --external <a.cod|b.cod>: textures those archives hold (loaded with
        # this one) out of this one: imports by name find theirs (the vehicles'
        # pack: the soldiers' textures, which every level archive has)
        if "--external" in sys.argv:
            ext = set()
            for path in sys.argv[sys.argv.index("--external") + 1].split("|"):
                for blk in kapf.decompose(kapf.Archive(path))["sections"][0]["blocks"]:
                    if blk["data"][:4] == b"APKF":
                        q = apkf.read(blk["data"])
                        ext |= {f["name"].lower() for f in q.files if q.types[f["type"]]["name"] == "TEX"}
            out = {(s.index, k) for s in sources for k, f in enumerate(s.p.files)
                   if s.p.types[f["type"]]["name"] == "TEX" and f["name"].lower() in ext}
            # (not those a kept file points at: effects point at their textures)
            out -= {(s.index, t) for s in sources for k in range(len(s.p.files))
                    if (s.index, k) not in removed for t in s.links.get(k, ())}
            removed |= out
            print(f"external: {len(out)} textures another archive has left out")
        # grafting: this archive's files of the grafted names out, the other
        # archive's files not grafted out, the grafted ones in
        removed |= displaced
        removed |= {(s.index, k) for s in sources[len(sec["blocks"]):] for k in range(len(s.p.files))}
        removed -= grafted
        # a removed file reached by a kept one's pointer would dangle: none may be
        for s in sources:
            for k in range(len(s.p.files)):
                if (s.index, k) not in removed:
                    for t in s.links.get(k, ()):
                        if (s.index, t) in removed:
                            raise SystemExit(f"{s.p.files[k]['name']} points at removed {s.p.files[t]['name']}")
        print(f"removed: {len(removed)} files ({sum(sum(sources[b].p.files[k]['sizes']) for b, k in removed) / 1048576:.1f} MB): "
              + ", ".join(f"{n} {t}" for t, n in collections.Counter(re.sub(r'\d+', '#', sources[b].p.files[k]['name'])[:20] for b, k in removed).most_common(8)))

    # what is reached: the roots through pointers and imports (by name)
    byname = collections.defaultdict(list)
    for s in sources:
        for k, f in enumerate(s.p.files):
            byname[f["name"].lower()].append((s.index, k))
    kind = {(s.index, k): s.p.types[f["type"]]["name"] for s in sources for k, f in enumerate(s.p.files)}
    plain = {(s.index, k) for s in sources for k, f in enumerate(s.p.files) if f["plain"]}
    roots = [key for key, t in kind.items() if key in plain or t in ("FX", "AEPS", "SKEL")
             or (t in ("MESH", "MLTI") and keep_re.search(sources[key[0]].p.files[key[1]]["name"]))]
    roots += [key for n in outside for key in byname.get(n, [])]
    # --keep-tex <regex>: textures the engine asks for by name, not through
    # any file (fogramp, the fog's colour ramp: stood in, Seelow's sky went
    # white at night and its day fog lost its ramp; tests night1/night2)
    if "--keep-tex" in sys.argv:
        keep_tex = re.compile(sys.argv[sys.argv.index("--keep-tex") + 1], re.I)
        roots += [key for key, t in kind.items() if t == "TEX" and keep_tex.search(sources[key[0]].p.files[key[1]]["name"])]
    roots = [key for key in roots if key not in removed]
    seen, stack = set(), list(roots)
    while stack:
        key = stack.pop()
        if key in seen:
            continue
        seen.add(key)
        s = sources[key[0]]
        stack.extend((s.index, t) for t in s.links.get(key[1], ()))
        for _w, _tag, n in s.imports.get(key[1], ()):
            stack.extend(byname.get(s.p.name_at(n).lower(), []))
    swap = set()
    for key, t in kind.items():
        if t != "TEX" or key in seen or key in removed or "--no-stand-ins" in sys.argv:
            continue
        s = sources[key[0]]
        words = sorted(w for w, _v in s.fixups.get(key[1], ()))
        if words == [4, 5] and not s.imports.get(key[1]) and s.p.files[key[1]]["size"] == 0x60:
            swap.add(key)
    saved = sum(sum(sources[b].p.files[k]["sizes"]) for b, k in swap)
    print(f"stand-ins: {len(swap)} textures ({saved / 1048576:.1f} MB) nothing kept reaches")

    # groups: files whose base-0 pointers reach each other stay together
    parent = {}

    def find(x):
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for s in sources:
        for k in range(len(s.p.files)):
            find((s.index, k))
            for t in s.links.get(k, ()):
                parent[find((s.index, k))] = find((s.index, t))
    groups = collections.OrderedDict()
    for s in sources:
        for k in range(len(s.p.files)):
            if (s.index, k) not in removed:
                groups.setdefault(find((s.index, k)), []).append((s.index, k))

    # --replace "name=path|...": plain files given new bytes, laid out with the rest
    # (2026-11-02: Seelow's 4th collision file as the level's unzoned.cgb, 0.7 MB,
    # past its old block's bank; swap_file.py replaces only within the bank)
    replace = {}
    if "--replace" in sys.argv:
        for pair in sys.argv[sys.argv.index("--replace") + 1].split("|"):
            name, path = pair.split("=", 1)
            replace[name.lower()] = Path(path).read_bytes()
        for s in sources:
            for k, f in enumerate(s.p.files):
                if f["name"].lower() in replace and (not f["plain"] or s.fixups.get(k) or s.imports.get(k)):
                    raise SystemExit(f"--replace {f['name']}: not a plain file without pointers")
        found = {f["name"].lower() for s in sources for f in s.p.files} & set(replace)
        if found != set(replace):
            raise SystemExit(f"--replace: not in the archive: {sorted(set(replace) - found)}")

    def file_of(key):
        s = sources[key[0]]
        f = stand_in(s, key[1]) if key in swap else s.parts["files"][key[1]]
        new = replace.get(s.p.files[key[1]]["name"].lower())
        if new is not None:
            f = dict(f, cpu=new, sizes=[len(new)] + list(f["sizes"][1:]))
        return f

    # sizes, for packing (generous: the compose below checks)
    def cost(key):
        s = sources[key[0]]
        f = file_of(key)
        t = s.parts["types"][f["type"]]
        c = 4 * (t["parts"] + 1) + align(len(f["cpu"]), max(16, t["aligns"][0])) + 16
        if len(f["sizes"]) > 1 and f["sizes"][1]:
            c += align(len(f["gpu"]), (t["aligns"][1] & 0xFFFF) or 4) + ((t["aligns"][1] & 0xFFFF) or 4)
        c += 4 * len(s.fixups.get(key[1], ())) + 12 * len(s.imports.get(key[1], ()))
        return c
    pool_cost = {s.index: 32 * len(s.parts["pool"]) for s in sources}
    TYPES = 0x400 + 0x10000 + 0x1000   # type records, the physical region's 64K alignment, slack

    # in order (the last block or a new one): a file never lands in a block
    # before one holding a file that came before it (converted cells want
    # their textures' blocks loaded first: merge_zone.py's test 3, all white)
    # 0.5.731: every texture first, then a new block for the rest: a material's
    # texture imports resolve as its block loads, from textures already in;
    # Longyard's sky grafted into one block with its textures drew white
    # (test night1; Longyard keeps them blocks apart)
    def texture_group(g):
        return all(kind[k] == "TEX" for k in g if k in kind) and all(k in kind for k in g)
    for s in sources[len(sec["blocks"]):]:
        for k, f in enumerate(s.p.files):
            kind[(s.index, k)] = s.p.types[f["type"]]["name"]
    # (a zone's index before all: merge_zone.py's tests 1 and A, nothing bound)
    def index_group(g):
        return any(sources[k[0]].p.files[k[1]]["name"].lower().endswith(".xenon.ib") for k in g)
    ordered = [g for g in groups.values() if index_group(g)] + \
              [g for g in groups.values() if texture_group(g) and not index_group(g)] + \
              [g for g in groups.values() if not texture_group(g) and not index_group(g)]
    bins = []   # [{"keys": [...], "sources": set, "size": n}]
    textures_done = False
    for g in ordered:
        need = sum(cost(k) for k in g)
        srcs = {k[0] for k in g}
        placed = False
        if not texture_group(g) and not index_group(g) and not textures_done:
            textures_done = True
            if bins and any(kind[k] == "TEX" for k in bins[-1]["keys"]):
                bins.append({"keys": [], "sources": set(), "size": 0})
        for b in bins[-1:]:
            extra = need + sum(pool_cost[i] for i in srcs - b["sources"])
            if b["size"] + extra + TYPES <= BANK:
                b["keys"] += g
                b["sources"] |= srcs
                b["size"] += extra
                placed = True
                break
        if not placed:
            bins.append({"keys": list(g), "sources": set(srcs), "size": need + sum(pool_cost[i] for i in srcs)})

    # each bin as a block
    new_blocks, where = [], {}   # where: (src block, file) -> (new block, type place, entry)
    for nb, b in enumerate(bins):
        srcs = sorted(b["sources"])
        # the pool starts with a copy of the first file's own name record: a
        # pool's start is its lowest file name record (apkf.read, the game's
        # blocks), and a source's first record may name a file now elsewhere
        first = min(b["keys"])
        pool = [sources[first[0]].parts["pool"][sources[first[0]].parts["files"][first[1]]["name"]]]
        pool_base = {}
        for i in srcs:
            pool_base[i] = len(pool)
            pool += sources[i].parts["pool"]
        # the types: each source's order kept (merged in order of first appearance after its predecessors)
        order = []
        for i in srcs:
            prev = -1
            for t in sources[i].parts["types"]:
                tk = type_key(t)
                if tk in order:
                    prev = order.index(tk)
                else:
                    order.insert(prev + 1, tk)
                    prev += 1
        type_of = {tk: n for n, tk in enumerate(order)}
        types = []
        for tk in order:
            types.append({"tag": tk[0], "version": tk[1], "parts": tk[2], "aligns": list(tk[3])})
        nreg = max(len(sources[i].parts["regions"]) for i in srcs)
        ref = next(sources[i] for i in srcs if len(sources[i].parts["regions"]) == nreg)
        regions = [dict(r, name=r["name"] + pool_base[ref.index]) for r in ref.parts["regions"]]
        # files in type order, each type's in source order
        keys = sorted(b["keys"], key=lambda k: (type_of[type_key(sources[k[0]].parts["types"][sources[k[0]].p.files[k[1]]["type"]])], srcs.index(k[0]), k[1]))
        files, starts = [], {}
        entry_count = collections.Counter()
        for k in keys:
            s = sources[k[0]]
            f = file_of(k)
            tk = type_key(s.parts["types"][f["type"]])
            t = type_of[tk]
            files.append({"type": t, "name": f["name"] + pool_base[k[0]], "cpu": bytearray(f["cpu"]),
                          "gpu": f["gpu"], "sizes": list(f["sizes"])})
            where[k] = (nb, t, entry_count[t])
            entry_count[t] += 1
        k0 = keys[0]
        pool[0] = sources[k0[0]].parts["pool"][sources[k0[0]].parts["files"][k0[1]]["name"]]
        files[0]["name"] = 0
        # where compose puts each file (it aligns them from the image's real
        # start, which the header's size decides): laid out once, read back
        # (the tail's lists change only its length, after the image)
        trial = apkf.compose({"words": ref.parts["words"], "types": types,
                              "files": [dict(f, cpu=bytes(f["cpu"])) for f in files], "pool": pool,
                              "regions": regions, "fixups": [], "imports": [], "gaps": b""})
        tp = apkf.read(trial)
        for k, f in zip(keys, tp.files):
            starts[k] = f["at"] - tp.image

        def moved(src, rel):
            fk, off = sources[src].owner(rel)
            key = (src, fk)
            if key not in starts:
                raise ValueError(f"block {src} file {fk}: pointed at from another new block")
            return starts[key] + off
        fixups, imports = [], []
        for n, k in enumerate(keys):
            s = sources[k[0]]
            for w, value in s.fixups.get(k[1], ()):
                if value >> 26 == 0:
                    value = moved(k[0], 4 * (value & 0x3FFFFFF)) // 4
                elif value >> 26 == 0x3F:
                    value = (0x3F << 26) | ((value & 0x3FFFFFF) + 8 * pool_base[k[0]])
                else:
                    raise ValueError(f"block {k[0]}: a pointer of base {value >> 26:#x}")
                struct.pack_into(">I", files[n]["cpu"], 4 * w, value)
                fixups.append(starts[k] // 4 + w)
            for w, tag, nm in s.imports.get(k[1], ()):
                imports.append((starts[k] // 4 + w, tag, nm + 32 * pool_base[k[0]]))
        for f in files:
            f["cpu"] = bytes(f["cpu"])
        order_pos = sorted(range(len(fixups)), key=lambda i: fixups[i])
        block = apkf.compose({"words": ref.parts["words"], "types": types, "files": files, "pool": pool,
                              "regions": regions, "fixups": [fixups[i] for i in order_pos],
                              "imports": sorted(imports), "gaps": b""})
        if len(block) > BANK:
            raise SystemExit(f"new block {nb}: {len(block):X} bytes, over a bank")
        new_blocks.append({"data": block, "bank": BANK, "flags": ref.flags})

    # the directory, in its order
    entries = []
    dropped = 0
    for e in sec["entries"]:
        b, idx = e[3], e[4]
        s = sources[b]
        tplace, ent = idx >> 16, idx & 0xFFFF
        k = next(n for n, f in enumerate(s.p.files) if f["type"] == tplace and f["entry"] == ent)
        if (b, k) in removed:
            dropped += 1
            continue
        nb, t, ne = where[(b, k)]
        size = e[2]
        if (b, k) in swap:
            size = len(file_of((b, k))["cpu"]) + len(file_of((b, k))["gpu"])
        if sources[b].p.files[k]["name"].lower() in replace:
            size = (size & ~0xFFFFFF) | len(replace[sources[b].p.files[k]["name"].lower()])
        entries.append((e[0], e[1], size, nb, (t << 16) | ne))
    # grafted entries: textures before this archive's first texture (the
    # loader dispatches by the directory: textures before what imports them),
    # the rest at the end
    for e in graft_entries:
        s = sources[e[3]]
        k = next(n for n, f in enumerate(s.p.files) if f["type"] == e[4] >> 16 and f["entry"] == e[4] & 0xFFFF)
        nb, t, ne = where[(e[3], k)]
        new = (e[0], e[1], e[2], nb, (t << 16) | ne)
        if e[0].lower().endswith(".xetex"):
            at = next((i for i, x in enumerate(entries) if x[0].lower().endswith(".xetex")), len(entries))
            entries.insert(at, new)
        else:
            entries.append(new)
    sec["blocks"] = new_blocks
    sec["entries"] = entries
    # --fli <path in the manifest>: the level's manifest (this archive's FLI
    # section) holds, for each archive, 8 floats after its path's pointer; the
    # fifth was 18 for Rouen's 18-block level archive, and the game took 18
    # banks for it after it was repacked into 12 (test seelow17repack): it
    # is set to the new block count
    # --fli-also "path=archive|...": the same for other archives (the zones,
    # repacked first), each set to its archive's block count
    if "--fli" in sys.argv:
        import fli
        counts = {sys.argv[sys.argv.index("--fli") + 1].lower(): len(new_blocks)}
        if "--fli-also" in sys.argv:
            for pair in sys.argv[sys.argv.index("--fli-also") + 1].split("|"):
                path, file = pair.split("=", 1)
                counts[path.lower()] = len(kapf.decompose(kapf.Archive(file))["sections"][0]["blocks"])
        for s in parts["sections"]:
            for blk in s["blocks"]:
                d = bytearray(blk["data"])
                if d[:4] != b"FLIN":
                    continue
                for place, target in fli.pointers(bytes(d)).items():
                    for want, count in counts.items():
                        w = want.encode()
                        if bytes(d[target:target + len(w) + 1]).lower() == w + b"\0":
                            old = struct.unpack_from(">f", d, place + 0x14)[0]
                            struct.pack_into(">f", d, place + 0x14, float(count))
                            print(f"manifest: {want}'s fifth number {old:g} -> {count}")
                blk["data"] = bytes(d)
    data = kapf.compose(parts)
    Path(out_path).write_bytes(data)
    print(f"{out_path}: {len(sources)} blocks -> {len(new_blocks)}, {len(entries)} entries ({dropped} removed), {len(data) / 1048576:.1f} MB")
    check(sources, swap, out_path, removed)


def check(sources, swap, out_path, removed=frozenset()):
    """Every file once; every fix-up at the same file and offset (or pool
    name) as before; every import the same name."""
    parts = kapf.decompose(kapf.Archive(out_path))
    new = {}
    for nb, blk in enumerate(parts["sections"][0]["blocks"]):
        p = apkf.read(blk["data"])
        fx, im = p.tail_lists()
        spans = [(f["at"], f["at"] + f["size"], f["name"]) for f in p.files]

        def owner(a):
            for s, e, n in spans:
                if s <= a < e:
                    return n, a - s
        for f in p.files:
            new.setdefault(f["name"], []).append(nb)
        for i in fx:
            n, off = owner(p.image + 4 * i)
            v = apkf.word(p.d, p.image + 4 * i)
            if v >> 26 == 0:
                t = owner(p.image + 4 * (v & 0x3FFFFFF))
                new.setdefault(("fix", n, off), []).append(("file", t[0], t[1]))
            else:
                at = p.names + 4 * (v & 0x3FFFFFF)
                new.setdefault(("fix", n, off), []).append(("pool", p.d[at - (at - p.names) % 32 + 4:][:28].split(b"\0")[0], (at - p.names) % 32))
        for i, t, nm in im:
            n, off = owner(p.image + 4 * i)
            new.setdefault(("imp", n, off), []).append(p.name_at(nm))
    bad = 0
    for s in sources:
        p = s.p
        for k, f in enumerate(p.files):
            if (s.index, k) in removed:
                continue
            if f["name"] not in new:
                bad += 1
                print("missing", f["name"])
            for w, v in s.fixups.get(k, ()):
                got = new.get(("fix", f["name"], 4 * w))
                if v >> 26 == 0:
                    tk, off = s.owner(4 * (v & 0x3FFFFFF))
                    want = ("file", p.files[tk]["name"], off)
                else:
                    at = p.names + 4 * (v & 0x3FFFFFF)
                    want = ("pool", p.d[at - (at - p.names) % 32 + 4:][:28].split(b"\0")[0], (at - p.names) % 32)
                if not got or want not in got:
                    bad += 1
                    if bad < 10:
                        print("fix-up moved wrong:", f["name"], 4 * w, want, got)
            for w, t, nm in s.imports.get(k, ()):
                got = new.get(("imp", f["name"], 4 * w))
                if not got or p.name_at(nm) not in got:
                    bad += 1
                    if bad < 10:
                        print("import moved wrong:", f["name"], 4 * w, p.name_at(nm), got)
    print("check:", "OK" if not bad else f"{bad} problems")
    if bad:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
