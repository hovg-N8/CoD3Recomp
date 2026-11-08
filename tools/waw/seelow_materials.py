"""Seelow's world materials and their colour textures (2026-11-02), read from
the user's own mp_seelow.ff zone by the data's own shapes (seelow_world.py
has the geometry):

  the world's material list (GfxWorld materialMemory): 127 x (Material*, u32
      memory), every pointer -1: the materials are written right after it, in
      the list's order. A surface's material pointer is a zone pointer (top 3
      bits the block, 4; the low 29 its offset) into that list: its entry
      (pointer - the lowest) / 8.
  a material: its struct (not in block 4), its name, then its texture table:
      16 bytes an entry: u32 name hash, the first and last letters of the
      sampler's name ('c','p' colorMap; 'n','p' normalMap; 's','p'
      specularMap; 'c','1' colorMap1, the second layer of a blend ...), u8
      sampler state, u8 semantic (2 colour, 5 normal, 8 specular), 4 bytes,
      the image: -1 when the image is written inline (after the table, in the
      table's order: a 40-byte struct, its name, 16 bytes with its format
      "DXT1" ...; a name starting with ',' has its pixels inline instead), or
      a zone pointer to the table slot of the material that
      wrote it inline first.
  A blend's name lists its layers: "*12n_10n" is layer 12n under layer 10n;
      layer 0 uses cp/np/sp, layer i c<i>/n<i>/s<i>.

The file and block 4 do not keep one distance (material structs and images
are loaded to other blocks), so pointers are resolved by what is certain:
  1. a layer written inline names that layer's image;
  2. another layer pointing at the same address shares that image;
  3. what is left points into the models' materials earlier in the zone: the
     slot with the same semantic whose file distance fits the curve the
     anchors (a model material named like one of the world's wc/ materials)
     draw, the distance only growing through the zone.
  A named world material (wc/x) with no image found uses x_c.

usage: python -I seelow_materials.py <mp_seelow.zone> [out.tsv]
  out.tsv: index, name, its layers' colour images (one per layer)
"""
import re
import struct
import sys

LIST_FILE = 0x24FB041
COUNT = 127
NAME = re.compile(rb"[ -~]{3,63}\x00")
FORMATS = (b"DXT1", b"DXT3", b"DXT5", b"A8R8", b"X8R8", b"R5G6", b"A8\x00", b"L8\x00", b"DXN")
INLINE = 0xFFFFFFFF


def image_name_after(z, at, limit=4000):
    """The first image name (a string with its format just after) from at."""
    for m in NAME.finditer(z, at, at + limit):
        if m.group(0).startswith(b",") or any(f in z[m.end():m.end() + 16] for f in FORMATS):
            return m.group(0)[:-1].decode("latin-1").lstrip(","), m.start()
    return None, None


def entries_at(z, at):
    out = []
    while at + 16 <= len(z):
        first, last, semantic = z[at + 4], z[at + 5], z[at + 7]
        if not (97 <= first <= 122 and (97 <= last <= 122 or 48 <= last <= 57) and semantic < 16):
            break
        ptr = struct.unpack_from("<I", z, at + 12)[0]
        if not (ptr == INLINE or 0x80000001 <= ptr <= 0x9FFFFFFF):
            break
        out.append((chr(first) + chr(last), semantic, ptr))
        at += 16
    return out, at


class Table:
    def __init__(self, z, name, start, entries, end):
        self.name, self.start, self.entries = name, start, entries
        self.images = []          # the inline images' names, None for pointers
        cursor = end
        for _s, _sem, ptr in entries:
            img = None
            if ptr == INLINE:
                img, where = image_name_after(z, cursor)
                if where is not None:
                    cursor = where + len(img) + 1
            self.images.append(img)


def tables_in(z):
    """Every material texture table in the zone, in file order."""
    out = []
    for m in NAME.finditer(z):
        if any(f in z[m.end():m.end() + 16] for f in FORMATS):
            continue
        entries, end = entries_at(z, m.end())
        if entries and not m.group(0).startswith((b"~", b"=")):
            out.append(Table(z, m.group(0)[:-1].decode("latin-1"), m.end(), entries, end))
    return out


def layer_of(sampler):
    return 0 if sampler[1] == "p" else int(sampler[1])


def layers(name):
    return name[1:].split("_") if name.startswith("*") else [name]


def resolve(z):
    tables = tables_in(z)
    first = next(i for i, t in enumerate(tables) if t.start > LIST_FILE + 8 * COUNT)
    world, models = tables[first:first + COUNT], tables[:first]
    layer_image = {}            # (token, semantic) -> image
    pending = []                # ((token, semantic), address, the world material)
    for t in world:
        tokens = layers(t.name)
        for (s, sem, ptr), img in zip(t.entries, t.images):
            if layer_of(s) >= len(tokens):
                continue
            key = (tokens[layer_of(s)], sem)
            if img:
                layer_image.setdefault(key, img)
            else:
                pending.append((key, (ptr - 1) & 0x1FFFFFFF, t.name))
    # 2. layers sharing an address share its image
    addr_image = {}
    for _ in range(4):
        for key, addr, _w in pending:
            if key in layer_image:
                addr_image.setdefault(addr, layer_image[key])
        for key, addr, _w in pending:
            if key not in layer_image and addr in addr_image:
                layer_image[key] = addr_image[addr]
    # 3. by the curve through the anchors
    left = sorted({(key, addr, w) for key, addr, w in pending if key not in layer_image and addr not in addr_image},
                  key=lambda p: p[1])
    slots = [(t.start + 16 * k + 12, sem, t, k) for t in tables for k, (_s, sem, _p) in enumerate(t.entries)]

    def candidates(addr, sem):
        return [(f - addr, t, k) for f, s, t, k in slots if s == sem and 0 <= f - addr < 0xC00000]

    # the anchors: in the models, a wc/x material's pointer landing on a model
    # material named x, or on an image named x_...
    anchors = {(addr, d) for key, addr, w in left if w.startswith("wc/")
               for d, t, k in candidates(addr, key[1]) if d < 0x80000
               and (t.name.split("/")[-1] == w[3:] or (t.images[k] or "").startswith(w[3:] + "_"))}
    # in the world, a layer's pointer to the slot where that layer was first
    # written inline (exact: one distance a table)
    first_slot = {}
    for t in world:
        tokens = layers(t.name)
        for k, ((s, sem, _p), img) in enumerate(zip(t.entries, t.images)):
            if img and layer_of(s) < len(tokens):
                first_slot.setdefault((tokens[layer_of(s)], sem), t.start + 16 * k + 12)
    anchors |= {(addr, first_slot[key] - addr) for key, addr, _w in pending if key in first_slot}
    anchors = sorted(anchors)

    def curve(addr):
        if addr <= anchors[0][0]:
            return anchors[0][1]
        for (a0, d0), (a1, d1) in zip(anchors, anchors[1:]):
            if a0 <= addr <= a1:
                return d0 + (d1 - d0) * (addr - a0) / max(1, a1 - a0)
        return anchors[-1][1]

    picks = []
    for key, addr, _w in left:
        cands = candidates(addr, key[1])
        if not anchors or not cands:
            continue
        d, t, k = min(cands, key=lambda c: abs(c[0] - curve(addr)))
        # (past the last anchor the curve is not known: the distance keeps growing)
        inside = anchors[0][0] <= addr <= anchors[-1][0]
        img = t.images[k] if inside and abs(d - curve(addr)) < 0x300 else None
        picks.append((key, addr, d, t.name, img))
        if img and key not in layer_image:
            layer_image[key] = img
            addr_image[addr] = img
    for key, addr, _w in pending:
        if key not in layer_image and addr in addr_image:
            layer_image[key] = addr_image[addr]
    out = []
    for t in world:
        colours = [layer_image.get((tok, 2)) for tok in layers(t.name)]
        if t.name.startswith("wc/") and not colours[0]:
            colours[0] = t.name[3:] + "_c"
        out.append((t.name, colours))
    return out, picks, anchors


def main():
    z = open(sys.argv[1], "rb").read()
    mats, picks, anchors = resolve(z)
    print("anchors:", [(hex(a), hex(d)) for a, d in anchors])
    for key, addr, d, name, img in picks:
        if key[1] == 2:
            print(f"  layer {key[0]}: {addr:#x} + {d:#x} -> {name}: {img}")
    lines = [f"{i}\t{name}\t{' '.join(c or '?' for c in colours)}" for i, (name, colours) in enumerate(mats)]
    missing = sum(1 for _n, cs in mats for c in cs if not c)
    print(f"{len(mats)} materials, {missing} layers without a colour image")
    print("\n".join(lines))
    if len(sys.argv) > 2:
        open(sys.argv[2], "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
