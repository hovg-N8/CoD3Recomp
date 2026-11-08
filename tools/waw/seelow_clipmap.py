"""World at War's Seelow collision (its clip map, col_map_mp) as CoD3 zone
collision (.cgb), from the user's own mp_seelow.ff (2026-11-02). Found by
the data's shapes and checked by its own counts (every check below is an
assert); the layout is T4's (WaW), close to CoD4's:

  header (zone 0x3145D68): name (a pointer: GfxWorld's string), isInUse,
    planeCount 15817 + a POINTER (GfxWorld's planes, file 0x17C9C7A, block
    offset 0x1528DA4; 20 bytes: normal, dist, type, sign bits, pad), then
    count + -1 (written inline, in this order): staticModels 1636 (80 bytes),
    materials 260 (72: name[64], surface flags, contents: CoD2's d3dbsp
    layout), brushSides 20987 (12: plane pointer, u32 material, 4),
    brushEdges 124203 (u8), nodes 3257, leafs 3342, leafbrushNodes 12320,
    leafbrushes 25391, leafsurfaces 0, verts 19897 (f32[3]), 66817 vec3s,
    39390 u16 (the partitions' vertex lists), triCount 29157 (u16 x3 +
    walkable bits), borders 3113, partitions 6437 (20: u8 triangles, u8
    borders, 2, u32 first triangle, u32 vertices, u32 first vertex, borders
    pointer), aabbTrees 18953 (32: origin[3], u16 material, u16 children,
    half size[3], u32 first child or partition), cmodels 84 (72: bounds,
    radius, a 44-byte leaf; 0 the world, the rest entities' brush models in
    their own space), brushes 8298 (80: mins[3], contents, maxs[3], u32
    sides beyond the box, sides pointer (block 4, 12 bytes a side), u16
    axial materials[2][3], ...).
  Collision triangles wind clockwise seen from their front (6581 of 7727
    flat ones face down by (b-a)x(c-a)): the front is -(b-a)x(c-a).

To CoD3 (tools/cod2import/cod2collision.py convert_brushes / convert_terrain,
CoD2's contents and surface bits being WaW's): a triangle takes its AABB
leaf's material; a brush its own contents, its surface type from its sides'
materials. Left out: brushes of contents 0x10000000 only (portals, hints)
or with nothing but detail and 0x01000000 (mantle helpers), and the
entities' brushes (inside a sub-model's own box near the origin), and
(drop_small_clip, 2026-11-02) WaW's small player clip at walking height.
The volumes go into as many .cgb as needed (a .cgb holds 65535 mesh index
bytes and points), split by x, one a zone.

usage: python -I seelow_clipmap.py <mp_seelow.zone> <template.cgb> <out prefix> [files (4: the 3 zones and the level's unzoned.cgb)]
  writes <out prefix>1.cgb, 2.cgb, ...
"""
import math
import os
import struct
import sys
from array import array
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "cod2import"))
sys.path.insert(0, str(HERE.parent / "kapf"))
import cod2collision   # noqa: E402
import cgb   # noqa: E402

PLANES_FILE, PLANES_BLOCK, PLANE_COUNT = 0x17C9C7A, 0x1528DA4, 15817
STATIC_MODELS = 0x3145EB4
VERTS, VERT_COUNT = 0x32391ED, 19897
TRIS, TRI_COUNT = 0x334A5D1, 29157
PARTITION_COUNT, AABB_COUNT, CMODEL_COUNT, BRUSH_COUNT = 6437, 18953, 84, 8298
MATERIAL_COUNT, SIDE_COUNT = 260, 20987


def cod3_contents(c):
    """WaW's contents in CoD2/CoD3's bits: the same bits, 0x10000 player clip
    and 0x20000 monster (AI) clip, as Rouen's (0x10000 alone 96 times). Swapped
    (the morning of 2026-11-02) WaW's 440 AI clip brushes walled the player in:
    the space a player climbs through at 206 of WaW's 292 mantle helpers lies
    inside one of them (none would be player clip), the user could not get
    through windows; WaW's 0x10000 brushes are the stair ramps (27 of 137
    sloped, against 7 of 440)."""
    return c


MANTLE = []   # the mantle helpers' boxes (mins, maxs), filled by read()
PIPE_CAPS = ((1261.0, 1263.0), (717.0, 737.0))   # x spans of the drain pipe's end rings (read())
# The drain pipe's roof between WaW's two solid slabs over its ends (x 735-855 and 1142-1262, z -56..-48,
# detail 0x8000001): its own shell (ceiling -60, solid triangles) does not stop standing up (the stand
# check meets brushes, not a thin mesh from inside): the user stood up in the middle and the game lifted
# them till their head met the road (-25), the view through the ground (945 6166, 2026-11-03). A slab
# as WaW's: (x0, x1, y0, y1, z0, z1), solid.
PIPE_ROOF = ((855.0, 1142.0, 6146.0, 6210.0, -56.0, -48.0),)
# Buried walls whose upright faces poke a few units out of the ground with no top to step onto:
# feet catch on the edge. (x0, x1, y0, y1, z bottom, z top): upright triangles (|n.z| < 0.3) wholly inside
# between the z, left out. 3320-3370 2770-2870: the tunnel entrance's parapet (roof -200,
# top -172/-168 in ground at -178..-185), the user stuck "on two textures" at 3349 2825 -172 and
# a bot unstuck at 3344 2834 (2026-11-03).
# (z bottom too: only faces above the tunnel's roof, its own walls below stay)
KERBS = ((3320.0, 3370.0, 2770.0, 2870.0, -205.0, -165.0),)
# Blasted holes in walls whose jagged edge and sill pieces (detail brushes, 0x8000001) leave a
# crouching player a band too narrow to find: those pieces out, the wall round the hole kept
# (its top strips reach above z1). (x0, x1, y0, y1, z0, z1): a brush wholly inside is left out.
# 2616-2624 3742-3846: the house east of 2461 3809, 63 high but open only y 3774-3793 for a
# 30-wide player (55 either side); the user got through once in many tries (2026-11-02 23:15).
# (z1 -31: the jagged lintel and corner pieces under the wall's own solid top at -32 too; the
# user, still stuck at the corners: "it needs to be wider")
WALL_HOLES = ((2615.0, 2625.0, 3741.0, 3847.0, -121.0, -31.0),)


def read(z):
    materials_at = STATIC_MODELS + 80 * 1636
    materials = []
    for i in range(MATERIAL_COUNT):
        name = z[materials_at + 72 * i:materials_at + 72 * i + 64].split(b"\0")[0].decode("latin-1")
        surf, cont = struct.unpack_from("<II", z, materials_at + 72 * i + 64)
        materials.append((name, surf, cod3_contents(cont)))
    assert materials[0][0] == "global_black" and materials[1][0] == "berlin_trim_concrete_beveld"
    sides_at = materials_at + 72 * MATERIAL_COUNT
    sides = [struct.unpack_from("<II", z, sides_at + 12 * i) for i in range(SIDE_COUNT)]
    planes = [struct.unpack_from("<4f", z, PLANES_FILE + 20 * i) for i in range(PLANE_COUNT)]

    def plane_of(ptr):
        off = ((ptr - 1) & 0x1FFFFFFF) - PLANES_BLOCK
        assert off % 20 == 0 and 0 <= off // 20 < PLANE_COUNT
        return planes[off // 20]

    verts = [struct.unpack_from("<3f", z, VERTS + 12 * i) for i in range(VERT_COUNT)]
    idx = array("H")
    idx.frombytes(z[TRIS:TRIS + 6 * TRI_COUNT])
    assert max(idx) == VERT_COUNT - 1
    aabb_at = None
    # the partitions end where the AABB trees begin: found by the trees' shape
    # (origin, u16 material < 260, u16 children < 64, half sizes >= 0)
    def node(at):
        o = struct.unpack_from("<3f", z, at)
        m, c = struct.unpack_from("<HH", z, at + 12)
        h = struct.unpack_from("<3f", z, at + 16)
        return (all(math.isfinite(x) and abs(x) < 40000 for x in o) and m < MATERIAL_COUNT and c < 64
                and all(math.isfinite(x) and 0 <= x < 40000 for x in h))
    # and the partitions before them cover the triangles exactly (a stray
    # record just before the trees has the trees' shape too)
    at = TRIS + 6 * TRI_COUNT
    while aabb_at is None:
        if all(node(at + 32 * k) for k in range(AABB_COUNT)):
            parts = [struct.unpack_from("<BBxxIII", z, at - 20 * PARTITION_COUNT + 20 * i)
                     for i in range(PARTITION_COUNT)]
            if sum(p[0] for p in parts) == TRI_COUNT and max(p[0] + p[2] for p in parts) == TRI_COUNT:
                aabb_at = at
        at += 1
        assert at < TRIS + 0x100000, "the AABB trees not found"
    tri_mat = {}
    for i in range(AABB_COUNT):
        _x, _y, _z, m, c, _a, _b, _c, u = struct.unpack_from("<3fHH3fI", z, aabb_at + 32 * i)
        if c == 0:
            n, _borders, first, _nv, _fv = parts[u]
            for t in range(first, first + n):
                tri_mat.setdefault(t, m)
    terrain = []
    for t in range(TRI_COUNT):
        a, b, c = (verts[idx[3 * t + k]] for k in range(3))
        u = [b[i] - a[i] for i in range(3)]
        w = [c[i] - a[i] for i in range(3)]
        front = (-(u[1] * w[2] - u[2] * w[1]), -(u[2] * w[0] - u[0] * w[2]), -(u[0] * w[1] - u[1] * w[0]))
        terrain.append((tri_mat.get(t, 0), front, (a, b, c)))
    # Floors (facing up) whose material only stops shots (0x2080, nothing a
    # player stands on, 108 triangles): solid too. The user fell through one at
    # 3559 4912 onto the fill block under the map (-128), walking from WaW's
    # ground at -42 (2026-11-02, 0.5.736's "where:" lines). The 44 facing down
    # (ceilings) and the walls stay shot-only, as no invisible walls are wanted.
    solid_of, floors_solid = {}, 0
    for t, (m, front, tri) in enumerate(terrain):
        cont = materials[m][2]
        ln = math.sqrt(sum(x * x for x in front)) or 1.0
        if cont & 0x2080 and not cont & 0x10011 and front[2] / ln > 0.7:
            if m not in solid_of:
                solid_of[m] = len(materials)
                materials.append((f"{materials[m][0]} (floor made solid)", materials[m][1], cont | 1))
            terrain[t] = (solid_of[m], front, tri)
            floors_solid += 1

    # The drain pipe under the north road (x 719-1281, y 6146-6211, floor -116): its
    # two end rings (the rim faces at x 1262 and 719-735) leave a hole only 22 wide at
    # the floor, narrower than the pipe and a player (30): the user got in at 1257 6170
    # and no further (2026-11-02, prone, "terrain wasn't good"). Out of the collision;
    # the pipe's walls and the bank stay.
    caps = 0
    for t, (m, front, tri) in enumerate(terrain):
        ln = math.sqrt(sum(x * x for x in front)) or 1.0
        if abs(front[0]) / ln < 0.7:    # (faces along the pipe only: not its floor or walls)
            continue
        for x0, x1 in PIPE_CAPS:
            if all(x0 <= q[0] <= x1 and 6140 <= q[1] <= 6215 and -125 <= q[2] <= -50 for q in tri):
                terrain[t] = None
                caps += 1
                break
    terrain = [t for t in terrain if t is not None]
    kerbs = 0
    for t, (m, front, tri) in enumerate(terrain):
        ln = math.sqrt(sum(x * x for x in front)) or 1.0
        if abs(front[2]) / ln >= 0.3:
            continue
        if any(all(k[0] <= q[0] <= k[1] and k[2] <= q[1] <= k[3] and k[4] <= q[2] <= k[5] for q in tri) for k in KERBS):
            terrain[t] = None
            kerbs += 1
    terrain = [t for t in terrain if t is not None]

    cmodels_at = aabb_at + 32 * AABB_COUNT
    cmodels = [struct.unpack_from("<6f", z, cmodels_at + 72 * i) for i in range(CMODEL_COUNT)]
    brushes_at = cmodels_at + 72 * CMODEL_COUNT
    raw = [struct.unpack_from("<3fI3fII6H", z, brushes_at + 80 * i) for i in range(BRUSH_COUNT)]
    assert sum(r[7] for r in raw) == SIDE_COUNT
    first_side = min((r[8] - 1) & 0x1FFFFFFF for r in raw if r[7])
    brushes, report = [], Counter()
    report["terrain floors made solid (shot-only material)"] = floors_solid
    report["terrain triangles left out: the drain pipe's end rings"] = caps
    report["terrain triangles left out: buried walls' edges (KERBS)"] = kerbs
    contents_mat = {}
    for r in raw:
        lo, cont, hi, n, ptr, axial = r[0:3], cod3_contents(r[3]), r[4:7], r[7], r[8], r[9:15]
        if cont & ~0x18000000 & ~0x20000000 & ~0x01000000 == 0:
            report[f"brushes left out: contents {cont:#x}"] += 1
            if cont & 0x01000000:
                MANTLE.append((lo, hi))   # (seelow_mantle.py: where WaW lets a player mantle)
            continue
        # WaW's breakable glass (0x10: the 7 berlin_window_white_grimy window
        # fillers): shot out in WaW, never broken here, so solid and unseen
        # in the windows (the user, 2026-11-02: windows not passable)
        if cont & 0x10:
            report["brushes left out: glass"] += 1
            continue
        if cont & 0x08000000 and any(h[0] <= lo[0] and hi[0] <= h[1] and h[2] <= lo[1] and hi[1] <= h[3]
                                     and h[4] <= lo[2] and hi[2] <= h[5] for h in WALL_HOLES):
            report["brushes left out: a wall hole's jagged edge"] += 1
            continue
        if any(all(m[a] - 1 <= lo[a] and hi[a] <= m[3 + a] + 1 for a in range(3)) for m in cmodels[1:]):
            report["brushes left out: an entity's (sub-model space)"] += 1
            continue
        side_list = []
        if n:
            k0 = (((ptr - 1) & 0x1FFFFFFF) - first_side) // 12
            side_list = sides[k0:k0 + n]
        side_mats = [m for m in axial if m < MATERIAL_COUNT] + [s[1] for s in side_list if s[1] < MATERIAL_COUNT]
        # WaW's ladders (material "ladder", surface 0x440b8: 0x8 ladder) keep
        # their surface flags, as Rouen's two ladders (0x40b8 and a type: metal
        # 0xd00000 or wood 0x1500000); by contents alone (surface 0) they were
        # walls (the user, 2026-11-02: ladders not climbable)
        surf = 0
        if any(materials[m][1] & 0x8 for m in side_mats):
            typed = Counter(materials[m][1] & 0x1F00000 for m in side_mats if materials[m][1] & 0x1F00000)
            surf = 0x40b8 | (typed.most_common(1)[0][0] if typed else 0xd00000)
            report["brushes: ladders (surface 0x40b8 kept)"] += 1
        if (cont, surf) not in contents_mat:
            contents_mat[(cont, surf)] = len(materials) + len(contents_mat)
        box = [lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]]
        brushes.append((contents_mat[(cont, surf)], side_mats, box, [plane_of(s[0]) for s in side_list]))
    for box in PIPE_ROOF:
        if (0x8000001, 0) not in contents_mat:
            contents_mat[(0x8000001, 0)] = len(materials) + len(contents_mat)
        brushes.append((contents_mat[(0x8000001, 0)], [], list(box), []))
        report["brushes added: the drain pipe's roof slab (PIPE_ROOF)"] += 1
    all_materials =materials + [(f"contents {c:#x}" + (" (ladder)" if s else ""), s, c) for c, s in contents_mat]
    return all_materials, terrain, brushes, cmodels[0], report


PLACES = []   # the clip map's model placements, "solid" set on those given a hull (model_collision)


def drop_small_clip(brushes, materials, terrain, report):
    """WaW's small player clip (0x10000, not solid, under 120 tall: 1320 boxes
    over rubble, debris and props) out where a walking player meets it: its
    bottom less than a player's height (72) above the floor under its middle
    (floor_under), or under that floor (sunk into it). The user, 2026-11-02: the "invisible things" one gets stuck on.
    Kept: the 516 tall ones (walls, map edges, roofs), small ones higher up
    (slabs over roofs and ledges), and any that are a prop's only collision
    (a clip map placement taller than the step (28, 0.5.737) and 16 wide
    each way, given no hull of its own, its bounds overlapping the box: the
    rocks, 97 of 104 in such boxes, walked through without them, the user's
    next play; not the tools, picture frames, flat sandbags or low rubble).
    SEELOW_SMALL_CLIP=1 keeps all of WaW's."""
    bare = [p for p in PLACES if not p.get("solid") and p["hi"][2] - p["lo"][2] > 28
            and min(p["hi"][0] - p["lo"][0], p["hi"][1] - p["lo"][1]) >= 16]
    G = 64
    grid = defaultdict(list)
    for m, _front, tri in terrain:
        if materials[m][2] & 1:
            xs, ys = [p[0] for p in tri], [p[1] for p in tri]
            for gx in range(int(min(xs) // G), int(max(xs) // G) + 1):
                for gy in range(int(min(ys) // G), int(max(ys) // G) + 1):
                    grid[gx, gy].append(tri)
    tops = [b[2] for b in brushes if materials[b[0]][2] & 1]

    def stands_for(box):
        for p in bare:
            lo, hi = p["lo"], p["hi"]
            ox = min(hi[0], box[1]) - max(lo[0], box[0])
            oy = min(hi[1], box[3]) - max(lo[1], box[2])
            if ox > 0 and oy > 0 and min(hi[2], box[5]) > max(lo[2], box[4]):
                small = min((hi[0] - lo[0]) * (hi[1] - lo[1]), (box[1] - box[0]) * (box[3] - box[2])) or 1.0
                if ox * oy >= 0.3 * small:
                    return p["model"]
        return None

    def floor_under(box):
        """The floor a walking player meets the box from: WaW's ground up to
        the box's top (boxes sunk 17-33 into it: the hedgehogs' 90 x 90 x 74,
        once taken for slabs over the tunnel below), a solid brush's top up to
        16 into its bottom (not one inside it: the strip on a wall's top)."""
        x, y = (box[0] + box[1]) / 2, (box[2] + box[3]) / 2
        out = [b[5] for b in tops if b[0] <= x <= b[1] and b[2] <= y <= b[3] and b[5] <= box[4] + 16]
        out += [v for v in ground(x, y) if v <= box[5]]
        return max(out) if out else None

    def ground(x, y):
        out = []
        for a, b, c in grid[int(x // G), int(y // G)]:
            d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
            if abs(d) < 1e-9:
                continue
            l1 = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / d
            l2 = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / d
            if min(l1, l2, 1 - l1 - l2) >= -1e-6:
                out.append(l1 * a[2] + l2 * b[2] + (1 - l1 - l2) * c[2])
        return out

    kept = []
    for brush in brushes:
        box, cont = brush[2], materials[brush[0]][2]
        if cont & 0x10000 and not cont & 1 and box[5] - box[4] < 120 and not materials[brush[0]][1] & 0x8:
            floor = floor_under(box)
            if floor is None or box[4] - floor < 72:
                prop = stands_for(box)
                if prop is None:
                    report["brushes left out: small player clip (walking height)"] += 1
                    continue
                report[f"small player clip kept (a prop's only collision): {prop}"] += 1
            else:
                report["small player clip kept (up high)"] += 1
        kept.append(brush)
    return kept


def solve3(a, b):
    def det(m):
        return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
    d = det(a)
    if abs(d) < 1e-9:
        return None
    out = []
    for c in range(3):
        m = [row[:] for row in a]
        for r in range(3):
            m[r][c] = b[r]
        out.append(det(m) / d)
    return out


def model_collision(z, materials, report):
    """The placed static models' own collision (2026-11-02) as world brushes.
    An XModel's header holds at 0x98 its collision surfaces' pointer (-1:
    inline), count and contents; the surfaces (44 bytes: triangles pointer,
    count, mins[3], maxs[3], bone, contents, surface flags) follow its
    materials (found by their shape: -1, a count, a box inside the model's),
    each one's triangles after the array, 48 bytes: plane (n, d), s and t
    (xyz, w): a corner where s.p - sw and t.p - tw are 0 or 1 on the plane
    (2026-11-02 evening: every model's triangles inside its box read so, the
    hedgehog's 148/148, the box car's 546/546, the ox cart's 190/190; read
    with + until then, 13/148, 13/546, 16/190: the meshed props stood in
    stray walls up to 363 out, the user could not walk round the hedgehogs;
    the crate's 12 close into its stored box exactly). A surface with contents
    without 1 (the trees' canopies, 2) is walked through. Grass, flowers:
    no surfaces. Lower than a step (12: the 480 rubble chunks, 340 triangles
    each) left out. A hull becomes a brush: its own planes when they close
    exactly into its stored box (convex), else that box turned with the model
    (as triangles the rest needed 59700 mesh triangles; the three .cgb hold
    about 30000 more)."""
    import seelow_models as smd
    import wawimport
    places = smd.placements(z)
    PLACES[:] = places
    hulls = {}
    for name in {p["model"] for p in places}:
        try:
            m = wawimport.read_xmodel(z, name)
        except Exception:
            report[f"model collision not read: {name}"] += 1
            continue
        s = m["start"]
        ptr, count = struct.unpack_from("<iI", z, s + 0x98)
        if ptr != -1 or count == 0:
            hulls[name] = []
            continue
        mlo, mhi = struct.unpack_from("<3f", z, s + 0xAC), struct.unpack_from("<3f", z, s + 0xB8)
        at, found = m["end"], False
        while at < m["end"] + 400000:
            p, nt = struct.unpack_from("<iI", z, at)
            b = struct.unpack_from("<6f", z, at + 8)
            if p == -1 and 1 <= nt <= 2000 and all(math.isfinite(x) for x in b) \
                    and all(mlo[k] - 2 <= b[k] <= b[k + 3] <= mhi[k] + 2 for k in range(3)):
                found = True
                break
            at += 1
        if not found:
            report[f"model collision not found (left out): {name}"] += 1
            hulls[name] = []
            continue
        recs = [struct.unpack_from("<iI6f3I", z, at + 44 * i) for i in range(count)]
        t = at + 44 * count
        out = []
        for r in recs:
            planes = [struct.unpack_from("<4f", z, t + 48 * k) for k in range(r[1])]
            t += 48 * r[1]
            lo, hi = list(r[2:5]), list(r[5:8])
            # its own planes where they close exactly into its stored box (a
            # convex hull: the crate's 12); else the box itself (meshes: carts,
            # trunks, train cars: the triangles would need 59700 mesh triangles,
            # the three .cgb have room for about 30000)
            exact = False
            if len(planes) <= 40:
                vs = cgb.brush_vertices([x - 50 for x in lo], [x + 50 for x in hi], planes)
                exact = bool(vs) and all(abs(min(v[k] for v in vs) - lo[k]) < 1 and abs(max(v[k] for v in vs) - hi[k]) < 1
                                         for k in range(3))
            # (not convex: its triangles too, for the meshes chosen below)
            tris, area = [], 0.0
            if not exact:
                for k in range(len(planes)):
                    q = struct.unpack_from("<12f", z, t - 48 * len(planes) + 48 * k)
                    n, d, sv, sw, tv, tw = q[0:3], q[3], q[4:7], q[7], q[8:11], q[11]
                    c = [solve3([list(n), list(sv), list(tv)], [d, a + sw, b + tw]) for a, b in ((0, 0), (1, 0), (0, 1))]
                    if None not in c:
                        tris.append((n, c))
                        u = [c[1][i] - c[0][i] for i in range(3)]
                        w = [c[2][i] - c[0][i] for i in range(3)]
                        area += 0.5 * math.sqrt((u[1] * w[2] - u[2] * w[1]) ** 2 + (u[2] * w[0] - u[0] * w[2]) ** 2
                                                + (u[0] * w[1] - u[1] * w[0]) ** 2)
            if not exact:
                planes = []
                for a in range(3):
                    n = [0.0, 0.0, 0.0]
                    n[a] = 1.0
                    planes.append((*n, hi[a]))
                    n = [0.0, 0.0, 0.0]
                    n[a] = -1.0
                    planes.append((*n, -lo[a]))
            # how much of its box's surface the shape covers: a box is worst for
            # the least (a maple's trunk and forks: a 103 x 115 block round a
            # trunk 20 wide, bots stalled by maples in test seelow11coll)
            box_area = 2 * ((hi[0] - lo[0]) * (hi[1] - lo[1]) + (hi[1] - lo[1]) * (hi[2] - lo[2])
                            + (hi[0] - lo[0]) * (hi[2] - lo[2]))
            fill = area / box_area if box_area else 1.0
            out.append((r[9], r[10], lo, hi, planes, exact, tris, fill))
        hulls[name] = out
    # the non-convex hulls as meshes, worst-filled first, within MESH_BUDGET triangles
    # placed (each .cgb holds ~21800 mesh triangles, the ground ~10000 of them).
    # 30000 (2026-11-02): at 20000 312 props stood in boxes fuller than their
    # shapes (the user: invisible things everywhere), now 127; 32000 and up put
    # a .cgb over 65535 mesh points (cgb.build refuses)
    count_of = Counter(p["model"] for p in places)
    meshed = set()
    budget = int(os.environ.get("SEELOW_MESH_BUDGET", "60000"))   # (4 files: every hull meshed, 0 in boxes)
    for fill, name, k, n in sorted((h[7], name, k, len(h[6])) for name, hs in hulls.items()
                                   for k, h in enumerate(hs) if not h[5] and h[0] & 1 and h[3][2] - h[2][2] >= 12):
        if n * count_of[name] <= budget:
            budget -= n * count_of[name]
            meshed.add((name, k))
    mat_of = {}
    brushes, terrain = [], []
    for pl in places:
        for k, (contents, surf, lo, hi, planes, exact, tris, _fill) in enumerate(hulls.get(pl["model"], [])):
            if not contents & 1:
                report["model hulls left out: not solid (canopies)"] += 1
                continue
            if hi[2] - lo[2] < 12:
                report["model hulls left out: lower than a step (rubble)"] += 1
                continue
            pl["solid"] = True
            if (surf, contents) not in mat_of:
                mat_of[(surf, contents)] = len(materials)
                materials.append((f"model collision {surf:#x}", surf, contents))
            _p, ax = smd.transform((0.0, 0.0, 0.0), pl["inv"], pl["origin"])
            if (pl["model"], k) in meshed:
                for n, corners in tris:
                    front = tuple(sum(n[r] * ax[r][c] for r in range(3)) for c in range(3))
                    w = tuple(tuple(smd.transform(c, pl["inv"], pl["origin"])[0]) for c in corners)
                    terrain.append((mat_of[(surf, contents)], front, w))
                report["model hulls: meshes (their triangles)"] += 1
                report[f"  meshed: {pl['model']}"] += 1
                continue
            # world planes: N = n turned by the axes, N.w <= d * scale + N.origin
            scale = math.sqrt(sum(x * x for x in ax[0]))
            o = pl["origin"]
            world = []
            for nx, ny, nz, d in planes:
                n3 = [(nx * ax[0][c] + ny * ax[1][c] + nz * ax[2][c]) / scale for c in range(3)]
                ln = math.sqrt(sum(x * x for x in n3)) or 1.0
                n3 = [x / ln for x in n3]
                world.append((*n3, (d * scale + sum(n3[c] * o[c] for c in range(3)) * ln) / ln))
            corners = [smd.transform((x, y, zz), pl["inv"], o)[0] for x in (lo[0], hi[0]) for y in (lo[1], hi[1])
                       for zz in (lo[2], hi[2])]
            box = []
            for a in range(3):
                box += [min(c[a] for c in corners), max(c[a] for c in corners)]
            brushes.append((mat_of[(surf, contents)], [mat_of[(surf, contents)]], box, world))
            report["model hulls: exact (own planes)" if exact else "model hulls: their box, turned"] += 1
    return brushes, terrain


def main():
    zone, template, prefix = sys.argv[1:4]
    zones = int(sys.argv[4]) if len(sys.argv) > 4 else 4
    z = open(zone, "rb").read()
    materials, terrain, brushes, world, report = read(z)
    if os.environ.get("SEELOW_MODEL_COLLISION", "1") == "1":
        model_brushes, model_tris = model_collision(z, materials, report)
    if os.environ.get("SEELOW_SMALL_CLIP", "0") != "1":
        brushes = drop_small_clip(brushes, materials, terrain, report)
    if os.environ.get("SEELOW_MODEL_COLLISION", "1") == "1":
        brushes, terrain = brushes + model_brushes, terrain + model_tris
    boxes, brushes3 = cod2collision.convert_brushes(materials, brushes, report)
    meshes = cod2collision.convert_terrain(materials, terrain, report)
    for k, v in sorted(report.items()):
        print(f"  {k}: {v}")

    # split by x: each volume to the band holding its middle, bands of even cost
    def middle(v):
        if len(v) == 3:      # mesh (flags0, flags1, triangles)
            pts = [p for t in v[2] for p in t]
            return sum(p[0] for p in pts) / len(pts)
        return (v[2][0] + v[3][0]) / 2
    items = [("box", b) for b in boxes] + [("brush", b) for b in brushes3] + [("mesh", m) for m in meshes]
    items.sort(key=lambda it: middle(it[1]))
    header = Path(template).read_bytes()[:0xD0]
    # bands by what fills a .cgb (2026-11-02 evening): its meshes' index bytes (3 a
    # triangle) and packed points, at most 65535 each; equal shares of triangles
    # put one band over at a mesh budget of 35000 while the others had room. Each
    # band's share is the total over the bands, so all stay near even; a band
    # closes early where the next mesh would pass FILL.
    FILL = 64500
    use = [(3 * len(it[1][2]), len(cgb.pack_points([p for t in it[1][2] for p in t])[1])) if it[0] == "mesh" else (0, 0)
           for it in items]
    total = sum(u[0] for u in use)
    share = total / zones
    bands, cur, idx, pts = [], [], 0, 0
    for it, (ci, cp) in zip(items, use):
        if cur and len(bands) < zones - 1 and (idx + ci > FILL or pts + cp > FILL or idx >= share):
            bands.append(cur)
            cur, idx, pts = [], 0, 0
        cur.append(it)
        idx += ci
        pts += cp
    bands.append(cur)
    print(f"mesh index bytes {total} over {zones} files (each at most 65535)")
    for k, band in enumerate(bands, 1):
        bx = [b for kind, b in band if kind == "box"]
        br = [b for kind, b in band if kind == "brush"]
        me = [m for kind, m in band if kind == "mesh"]
        pts = [p for m in me for t in m[2] for p in t] + [v for b in bx + br for v in (b[2], b[3])]
        lo = [min(p[a] for p in pts) for a in range(3)]
        hi = [max(p[a] for p in pts) for a in range(3)]
        data = cgb.build(header, lo, hi, bx, br, me)
        out = f"{prefix}{k}.cgb"
        Path(out).write_bytes(data)
        c = cgb.Cgb(data)
        bad = c.check()
        print(f"{out}: x {lo[0]:.0f}..{hi[0]:.0f}, {len(data)} bytes, {c.nboxes} boxes, {c.nbrushes} brushes, "
              f"{c.nmesh} meshes, {c.npoints} points: {'OK' if not bad else bad}")


if __name__ == "__main__":
    main()
