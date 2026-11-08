"""Seelow's placed static models (trees, rubble, crates, carts, fences ...)
from the user's own mp_seelow.ff (2026-11-02), as world triangles for
seelow_cells.py: model, placement and colour image.

  the clip map's static model list (zone 0x3145EB4, 1636 x 80 bytes): u32,
    the XModel (a pointer to its slot in the zone's asset list: the list's
    block offset is 0x277C, 8 bytes an entry; 81 models, every one a type-5
    entry), origin, the inverse-scaled axis (3 x 3), the placed bounds.
  the n-th type-5 entry is the n-th XModel in the file (its 0xE4-byte
    header: -1 name pointer, bone/surface counts, 8 pointers (the first -1),
    4 LODs (distance, u16 surfaces, u16 first surface, ...), its name after).
    Checked: every placed model's own size times its scale against its
    placed bounds (ratio 0.95-1.5 for rotated boxes; books at 0.85 scale 0.99).
  a model's materials: after its surfaces, u32 a surface: -1 a material
    written inline (in order: its name "mc/..." then its texture table), or a
    pointer to an earlier handle (later LODs point back at LOD 0's; the array's
    own block offset is the lowest such pointer).
  LOD: the first, or the second where the first has over 1000 triangles (the
    big trees: 1.05 M triangles all at LOD 0, 0.69 M at LOD 1 otherwise).

usage (a module for seelow_cells.py; as a script it prints the summary):
  python -I seelow_models.py <mp_seelow.zone>
"""
import math
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import wawimport   # noqa: E402
import seelow_materials as sm   # noqa: E402

ASSETS_FILE, ASSETS_BLOCK, ASSET_COUNT = 0x27AD, 0x277C, 711
STATIC_MODELS, STATIC_COUNT = 0x3145EB4, 1636
# LOD 1 for a model over this many triangles at LOD 0: with WaW's whole draw list
# (7582 placements) 1000 made 38 splits and 116 MB (the game draws 32, its banks
# hold ~49 blocks); 300 (and both trees at LOD 2) 612k model triangles
LOD0_LIMIT = 300
FF = b"\xff\xff\xff\xff"


def model_headers(z):
    out = []
    s = z.find(FF)
    while s >= 0 and s + 0xE4 + 4 < len(z):
        nb, nr, ns = z[s + 4], z[s + 5], z[s + 6]
        if 0 < nb <= 128 and 0 < ns <= 32 and nr <= nb:
            ptrs = struct.unpack_from("<8I", z, s + 8)
            if ptrs[0] == 0xFFFFFFFF and all(p in (0, 0xFFFFFFFF) or 0x80000000 <= p < 0xE0000000 for p in ptrs):
                m = re.match(rb"([a-z][A-Za-z0-9_\-/@#]{2,80})\x00", z[s + 0xE4:s + 0xE4 + 90])
                if m:
                    out.append(m.group(1).decode())
        s = z.find(FF, s + 1)
    return out


# model_headers finds 203 of the zone's 207 model assets, so past slot 82 the
# header order runs ahead of the asset slots. Each slot below proven by its
# placements: the colliding ones by the placed boxes against the model's own
# (83-85 one back, near-exact; 86 the muddy sandbag, 2 units, 14 small
# colliders WaW stacks at 2320 -2600; 90 the destroyed Opel Blitz, 14 against
# the Panzer's 26: 251 x 106 x 98 placed), the rest by their placings (87 in
# rows 67 apart: the barbed wire; 88-89 the field flowers; 91 the grass, 5350
# at scale 0.7). Read in header order, 86 drew 386-wide flower patches against
# house walls and 90 Panzer wrecks with Panzer collision where trucks stand
# (the user, 2026-11-02: "grass that isnt from seelow", invisible bumps).
SLOT_FIX = {83: "static_seelow_pictframe_wall_rct", 84: "static_berlin_books_open",
            85: "clutter_berlin_crate_01", 86: "static_seelow_sandbag_muddy",
            87: "static_peleliu_barbedwire", 88: "foliage_pacific_field_flowers03",
            89: "foliage_pacific_field_flowers04", 90: "vehicle_ger_wheeled_opel_blitz_d",
            91: "foliage_pacific_grass01_clump01"}


def slot_name(k, names):
    return SLOT_FIX.get(k, names[k] if k < len(names) else f"?slot{k}")


def placements(z):
    types = [struct.unpack_from("<I", z, ASSETS_FILE + 8 * i)[0] for i in range(ASSET_COUNT)]
    models = [k for k, t in enumerate(types) if t == 5]
    names = model_headers(z)
    out = []
    for i in range(STATIC_COUNT):
        r = struct.unpack_from("<II3f9f3f3f", z, STATIC_MODELS + 80 * i)
        slot = ((r[1] - 1) & 0x1FFFFFFF) - ASSETS_BLOCK - 4
        assert slot % 8 == 0 and types[slot // 8] == 5
        out.append({"model": slot_name(models.index(slot // 8), names), "origin": r[2:5], "inv": r[5:14],
                    "lo": r[14:17], "hi": r[17:20]})
    return out


class Models:
    """Every XModel's surfaces and material names, a pointer to an earlier
    material handle or image slot resolved by the file<->block distance of
    the models' part of the zone. That distance only grows (other blocks'
    data in between); its anchors are exact: the handle arrays that point back
    into themselves (later LODs at LOD 0's) give their own block offset (72
    of 198 arrays: 0x7B3 at file 0x8B59F .. 0x14AC6 at 0xF5FF26). A pointer
    is taken to the slot of its kind nearest where the curve puts it (within
    0x200 bytes)."""

    def __init__(self, z):
        self.z = z
        self.models, self.arrays = {}, []
        for n in model_headers(z):
            try:
                m = wawimport.read_xmodel(z, n)
            except Exception:  # (the destroyed Panzer: a multi-part vehicle)
                continue
            k = len(m["surfaces"])
            handles = struct.unpack_from(f"<{k}I", z, m["end"])
            self.models[n] = m
            self.arrays.append((m["end"], handles, n))
        anchors = []
        for f, hs, n in self.arrays:
            ptr = [(i, (h - 1) & 0x1FFFFFFF) for i, h in enumerate(hs) if h != 0xFFFFFFFF]
            if ptr:
                base = min(p for _i, p in ptr)
                if all((p - base) % 4 == 0 and 0 <= (p - base) // 4 < i for i, p in ptr):
                    anchors.append((base, f - base))
        self.anchors = sorted(anchors)
        self.slot_of = {f + 4 * j: (n, j) for f, hs, n in self.arrays for j in range(len(hs))}
        self.tables = [t for t in sm.tables_in(z) if t.name.startswith("mc/")]
        self.table_slot = {t.start + 16 * k + 12: (t, k) for t in self.tables for k in range(len(t.entries))}
        self.names = {}
        self.images = {}
        self.bias, self.window = 0, 0x200
        self.iwd = set()   # the archives' image names, for the last guess (set by the caller)
        self.guessed = {}

    def guess(self, material):
        """The archives' colour image sharing the most words with the material's
        name, the object's own word among them (black barrel ->
        static_peleliu_blackbarrel01_c): for materials whose image pointer
        could not be placed. A guess: said so where it is used."""
        generic = {"mc", "mtl", "static", "c", "col", "berlin", "seelow", "peleliu", "wet", "plain", "d"}
        every = set(re.split(r"[_/]", material.lower()))
        words = every - generic
        best = None
        # (sorted, ties by the generic words shared too: a set's order changes
        # run to run, and the wet hedgehog went to hedgehog01_c one run and
        # static_berlin_hedgehog_metal_c the next)
        for n in sorted(self.iwd):
            if not (n.endswith("_c") or n.endswith("_col")):
                continue
            # (a word inside the image's: blackbarrel in ..._blackbarrel01_c)
            parts = set(n.split("_"))
            shared = {w for w in words if w in parts or (len(w) >= 6 and w in n)}
            score = (len(shared), len(every & parts))
            if shared and (best is None or score > best[0]):
                best = (score, n)
        return best[1] if best else None

    def delta(self, block):
        a = self.anchors
        if block <= a[0][0]:
            return a[0][1]
        for (b0, d0), (b1, d1) in zip(a, a[1:]):
            if b0 <= block <= b1:
                return d0 + (d1 - d0) * (block - b0) / max(1, b1 - b0)
        return a[-1][1]

    def nearest(self, slots, ptr, keep=lambda hit: True):
        """The slot of this kind nearest the curve's place for ptr (keep: the
        kind; an image pointer only at colour slots: the nearest of any kind
        was a neighbour's specular map, one entry off)."""
        # (+bias: an image slot lies 0x6D past the curve's place, the median of
        # the 14 image pointers: a material's struct before its table is in no
        # handle array's distance; one at 0x1F1 was a wrong table and is refused)
        block = (ptr - 1) & 0x1FFFFFFF
        want = int(block + self.delta(block)) + self.bias
        found = [(abs(f - want), f) for f in range(want - self.window, want + self.window) if f in slots and keep(slots[f])]
        return slots[min(found)[1]] if found else None

    def material_names(self, name):
        if name in self.names:
            return self.names[name]
        m = self.models[name]
        f, handles, _n = next(a for a in self.arrays if a[2] == name)
        inline = m["materials"]
        out, k = [], 0
        self.names[name] = out          # (a pointer back into this array reads what is done)
        for i, h in enumerate(handles):
            if h == 0xFFFFFFFF:
                out.append(inline[k] if k < len(inline) else None)
                k += 1
                continue
            # a handle is a material's record: by the tables' curve first
            record = self.material_record(h)
            if record:
                out.append(record)
                continue
            hit = self.nearest(self.slot_of, h)
            if hit and hit[0] == name and hit[1] < i:
                out.append(out[hit[1]])
            elif hit and hit[0] != name:
                other = self.material_names(hit[0])
                out.append(other[hit[1]] if hit[1] < len(other) else None)
            else:
                # the last resort, past the curves' anchors: the material loaded
                # last before this model (within 256 KB). The Panzer wreck's 8
                # handles, one pointer: mc/mtl_panzer4_body (panzeriv_c) before it.
                before = [t for t in self.tables if f - 0x40000 < t.start < f]
                out.append(before[-1].name if before else None)
        return out

    def material_record(self, ptr):
        """A handle pointing at a material shared with an earlier model points
        at that material's own record, just before its texture table (the
        chairs' 0x3CF858: 0x28 before seelow_trim_wood_plain_planks' table,
        whose colour slot the world's layer 69n points at; the water tower's
        0x592AE8: 0x24 before seelow_wall_wood_old02's, layer 23n): the table's
        first slot ~0x30 on, by the curve with no bias (those two land 4 and 0
        bytes from the slots their layers prove; with the images' +0x6D, 0x69
        off and refused)."""
        # (by the TABLES' own curve, seelow_materials' exact anchors: the handle
        # arrays' curve is ~0x276 short here, the tables lying otherwise between
        # other blocks' data)
        first = getattr(self, "_first_slots", None)
        if first is None:
            first = self._first_slots = {t.start + 12: t for t in self.tables}
            _mats, _picks, anchors = sm.resolve(self.z)
            self._table_anchors = sorted(a for a in anchors if a[0] < 0x1900000)
        block = ((ptr - 1) & 0x1FFFFFFF) + 0x30
        a = self._table_anchors
        d = a[0][1] if block <= a[0][0] else a[-1][1]
        for (b0, d0), (b1, d1) in zip(a, a[1:]):
            if b0 <= block <= b1:
                d = d0 + (d1 - d0) * (block - b0) / max(1, b1 - b0)
        want = int(block + d)
        found = [(abs(f - want), f) for f in range(want - 0x20, want + 0x20) if f in first]
        return first[min(found)[1]].name if found else None

    def colour(self, material):
        """A model material's colour image: its table's colour map, written
        inline or pointed at an earlier table's slot."""
        if material in self.images:
            return self.images[material]
        self.images[material] = None
        t = next((t for t in self.tables if t.name == material), None)
        if t:
            for (s, sem, ptr), img in zip(t.entries, t.images):
                if sem != 2 or s[1] not in "p":
                    continue
                if img:
                    self.images[material] = img
                else:
                    self.bias, self.window = 0x6D, 0x80
                    hit = self.nearest(self.table_slot, ptr, lambda tk: tk[0].entries[tk[1]][1] == 2)
                    self.bias, self.window = 0, 0x200
                    if hit:
                        u, k = hit
                        self.images[material] = u.images[k] or self.colour(u.name)
                break
        if not self.images[material] and material:
            self.images[material] = self.guess(material)
            if self.images[material]:
                self.guessed[material] = self.images[material]
        return self.images[material]


def transform(p, inv, origin):
    """invScaledAxis = the axis divided by the scale, the model's axes in its
    COLUMNS (moved boxes against the placed ones: 922 of 1627 within 8 units
    read so, 830 read by rows; the trees' placed boxes are their trunks')."""
    s2 = inv[0] ** 2 + inv[3] ** 2 + inv[6] ** 2
    scale = 1.0 / math.sqrt(s2)
    ax = [[inv[3 * c + r] * scale * scale for c in range(3)] for r in range(3)]   # axis r, times the scale
    return tuple(origin[c] + p[0] * ax[0][c] + p[1] * ax[1][c] + p[2] * ax[2][c] for c in range(3)), ax


# models drawn at a farther LOD: the game draws a cell's _SPLIT0.._SPLIT31 only;
# with the pines at LOD 1 Seelow's one cell had 34 splits and the last two (the
# alpha-tested trees, written last) were never drawn (test seelow6models). The
# pine's LOD 2 (228 triangles against 1442) is WaW's own from 2000 units.
# the grass clump's LOD 1 (14 triangles; LOD 0 is 1538, 8 million over its 5350 clumps)
FAR_LOD = {"foliage_cod5_tree_pine_01_large": 2, "foliage_cod5_tree_maple_01_large": 2,
           "foliage_pacific_grass01_clump01": 1}
# models with one LOD, too many triangles for their 480 placements: their
# vertices merged on a grid (model units), each cell kept as its first vertex
# (position, normal, uv): the rubble chunk (42 x 45 x 6.6, scale 2) 340 -> 170
SIMPLIFY = {"rubble_chunk_02": 4.0}


def simplified(surface, grid):
    rep, tris = {}, []
    for i, (p, _n, _uv) in enumerate(surface["verts"]):
        rep.setdefault(tuple(round(x / grid) for x in p), i)
    cell = [rep[tuple(round(x / grid) for x in p)] for p, _n, _uv in surface["verts"]]
    for a, b, c in surface["tris"]:
        t = (cell[a], cell[b], cell[c])
        if len(set(t)) == 3:
            tris.append(t)
    return dict(surface, tris=tris)


def lod_surfaces(m, name=None):
    if name in FAR_LOD:
        d, n, s = m["lods"][FAR_LOD[name]][0:3]
        if n:
            return range(s, s + n)
    d0, n0, s0 = m["lods"][0][0:3]
    tris0 = sum(len(s["tris"]) for s in m["surfaces"][s0:s0 + n0])
    d1, n1, s1 = m["lods"][1][0:3]
    if tris0 > LOD0_LIMIT and n1:
        return range(s1, s1 + n1)
    return range(s0, s0 + n0)


def transform_axis(p, axis, scale, origin):
    return tuple(origin[c] + scale * (p[0] * axis[0][c] + p[1] * axis[1][c] + p[2] * axis[2][c]) for c in range(3))


DRAW_LIST, DRAW_COUNT = 0x2FD8684 - 52, 7582
# Slots past 82: SLOT_FIX above (the collision list's names come from the same
# header order, so they proved nothing there; the placed boxes do).


def draw_placements(z):
    """Every model WaW DRAWS (2026-11-02): GfxWorld's static model list, 7582 x
    92 bytes from zone 0x2FD8630 (found as the one long run of model pointers,
    stride 92): f32 origin[3], axis[3][3] (rows: forward, left, up), scale,
    the XModel (its asset list slot, as the clip map's), 32 bytes, cull
    distance. The clip map's list (placements) has only the 1636 that collide:
    from it Seelow had no grass, wheat or hedgerows (the user)."""
    types = [struct.unpack_from("<I", z, ASSETS_FILE + 8 * i)[0] for i in range(ASSET_COUNT)]
    models = [k for k, t in enumerate(types) if t == 5]
    names = model_headers(z)
    out = []
    for i in range(DRAW_COUNT):
        at = DRAW_LIST + 92 * i
        origin = struct.unpack_from("<3f", z, at)
        axis = struct.unpack_from("<9f", z, at + 12)
        scale = struct.unpack_from("<f", z, at + 48)[0]
        slot = ((struct.unpack_from("<I", z, at + 52)[0] - 1) & 0x1FFFFFFF) - ASSETS_BLOCK - 4
        assert slot % 8 == 0 and types[slot // 8] == 5, f"draw list record {i}: not a model"
        k = models.index(slot // 8)
        out.append({"model": slot_name(k, names), "origin": origin,
                    "axis": [axis[0:3], axis[3:6], axis[6:9]], "scale": scale})
    return out + EXTRA_PLACEMENTS


# models of our own beside WaW's (seelow_cells.py my_lights: the user's lamps at night)
EXTRA_PLACEMENTS = []


# Models whose uv read (u, v) draw their image turned a quarter: the wheat's
# stalks and heads lay sideways in the game (the user, 2026-11-02; test wheat3)
# and in a side view rendered from its own data, upright with u and v swapped.
# The grass, hedgerows, flowers, water tower and pines render right as read.
SWAP_UV = {"foliage_cod5_wheat_small", "foliage_cod5_hedgerow04", "foliage_pacific_hedgerow03",
           "foliage_cod5_tree_pine_01_large"}
# (the hedges: as read, hard-edged sheets: the user's "bush lying sideways",
# picture 185114; swapped, leafy bushes; the pine: as read a black broadleaf
# blob, swapped a pine. The maple and the flowers look alike either way.)
# The grass clump is drawn at its LOD 1 everywhere (FAR_LOD: its LOD 0 is 1538
# triangles x 5350 clumps): tall triangle cards with a tuft painted as seen from
# above, blades lying across the card, which WaW shows only past 500 units. Close
# up here they leaned (the user's pictures, tests bush1/bush2; a quarter turn
# still leaned ~25 degrees). The image's blades rise 27 degrees to the right
# (grass05a_card_c, its alpha's structure tensor): each card maps that direction
# straight up the card, the image's middle at the card's middle. (WaW's close-up
# blade image, grass05a_c, has no alpha, so it cannot go on cards.)
UPRIGHT_CARDS = {"foliage_pacific_grass01_clump01"}
BLADE = (math.cos(math.radians(27.0)), -math.sin(math.radians(27.0)))   # image (u, v), v down
ACROSS = (-BLADE[1], BLADE[0])
CARD_SPAN = 0.8


def upright_card(local):
    """uv for a card triangle from its model-space points: up the card (its
    height over its top) along the image's blades, along its base across them."""
    top = max(p[2] for p in local) or 1.0
    base = sorted(local, key=lambda p: p[2])[:2]
    axis = (base[1][0] - base[0][0], base[1][1] - base[0][1])
    span = axis[0] * axis[0] + axis[1] * axis[1] or 1.0
    out = []
    for p in local:
        s = ((p[0] - base[0][0]) * axis[0] + (p[1] - base[0][1]) * axis[1]) / span - 0.5
        t = p[2] / top - 0.5
        out.append((0.5 + CARD_SPAN * (s * ACROSS[0] + t * BLADE[0]),
                    0.5 + CARD_SPAN * (s * ACROSS[1] + t * BLADE[1])))
    return out


def world_triangles(z, iwd_names, report):
    """[(image or None, material name, [(pos, normal, uv) x 3])] for every model
    WaW draws (its draw list; the collision list's placements are for collision)."""
    places = draw_placements(z)
    models = Models(z)
    models.iwd = set(iwd_names)
    out = []
    for pl in places:
        name = pl["model"]
        if name not in models.models:
            report[f"placements left out: {name} (not read)"] += 1
            continue
        m, mats = models.models[name], models.material_names(name)
        ax, origin, scale = pl["axis"], pl["origin"], pl["scale"]
        surfaces = lod_surfaces(m, name)
        for si in surfaces:
            s = m["surfaces"][si]
            if name in SIMPLIFY:
                s = simplified(s, SIMPLIFY[name])
            mat = mats[si] if si < len(mats) else None
            if mat is None and m["lods"][0][1] == len(surfaces):
                # a lower LOD's handle that did not resolve: its LODs list the
                # same materials in the same order (the boxcar's LOD 1: flatbed,
                # None, None for LOD 0's flatbed, boxcar, boxcar_metal)
                k = m["lods"][0][2] + si - surfaces[0]
                mat = mats[k] if k < len(mats) else None
                if mat:
                    report["materials from LOD 0's same surface"] += len(s["tris"])
            img = models.colour(mat) if mat else None
            pts = []
            for p, n, uv in s["verts"]:
                wp = tuple(origin[c] + scale * (p[0] * ax[0][c] + p[1] * ax[1][c] + p[2] * ax[2][c]) for c in range(3))
                wn = [n[0] * ax[0][c] + n[1] * ax[1][c] + n[2] * ax[2][c] for c in range(3)]
                ln = math.sqrt(sum(x * x for x in wn)) or 1.0
                pts.append((wp, [x / ln for x in wn], uv))
            if name in SWAP_UV:
                pts = [(p, n, (uv[1], uv[0])) for p, n, uv in pts]
            for a, b, c in s["tris"]:
                if name in UPRIGHT_CARDS:
                    uvs = upright_card([s["verts"][i][0] for i in (a, b, c)])
                    out.append((img, mat, tuple((pts[i][0], pts[i][1], uv) for i, uv in zip((a, b, c), uvs))))
                    continue
                out.append((img, mat, (pts[a], pts[b], pts[c])))
            report["triangles"] += len(s["tris"])
            report["with a colour image" if img else "no colour image"] += len(s["tris"])
    world_triangles.guessed = models.guessed
    return out, places


def main():
    z = open(sys.argv[1], "rb").read()
    import seelow_textures   # noqa: F401  (the archives' index)
    main_dir = r"D:\Steam\steamapps\common\Call of Duty World at War\main"
    iwd = set(seelow_textures.iwd_index(main_dir))
    report = Counter()
    tris, drawn = world_triangles(z, iwd, report)
    for k, v in sorted(report.items()):
        print(f"  {k}: {v}")
    missing = Counter(mat for img, mat, _t in tris if not img)
    print("guessed by name:", world_triangles.guessed)
    print("materials without a colour image:", missing.most_common(12))
    print(f"drawn: {len(drawn)} placements of {len({d['model'] for d in drawn})} models")
    # the two lists agree: a colliding model's placement in both, a point of it
    # moved by each (the draw list's axis rows x scale; the clip map's columns)
    places = placements(z)
    by_origin = {}
    for d in drawn:
        by_origin.setdefault((d["model"], tuple(round(c) for c in d["origin"])), d)
    agree = Counter()
    for pl in places:
        d = by_origin.get((pl["model"], tuple(round(c) for c in pl["origin"])))
        if not d:
            agree["not in the draw list"] += 1
            continue
        a = transform((10.0, 20.0, 30.0), pl["inv"], pl["origin"])[0]
        b = transform_axis((10.0, 20.0, 30.0), d["axis"], d["scale"], d["origin"])
        agree["same place" if math.dist(a, b) < 0.5 else "differ"] += 1
    print("collision list against draw list:", dict(agree))
    # the transform checked: each placement's LOD 0 box, moved, against its placed box
    fits = Counter()
    cache = {}
    for pl in places:
        if pl["model"] not in cache:
            try:
                cache[pl["model"]] = wawimport.read_xmodel(z, pl["model"])
            except Exception:
                cache[pl["model"]] = None
        m = cache[pl["model"]]
        if m is None:
            continue
        d0, n0, s0 = m["lods"][0][0:3]
        pts = [transform(v[0], pl["inv"], pl["origin"])[0] for s in m["surfaces"][s0:s0 + n0] for v in s["verts"]]
        lo = [min(p[k] for p in pts) for k in range(3)]
        hi = [max(p[k] for p in pts) for k in range(3)]
        err = max(max(abs(lo[k] - pl["lo"][k]), abs(hi[k] - pl["hi"][k])) for k in range(3))
        fits["within 8 units" if err < 8 else "within 64" if err < 64 else "off"] += 1
    print(f"{len(places)} placements; moved boxes against the placed ones: {dict(fits)}")


if __name__ == "__main__":
    main()
