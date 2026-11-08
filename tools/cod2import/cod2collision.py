"""Call of Duty 2's collision for a map (maps/mp/<name>.d3dbsp, IBSP 4) as
CoD3's zone collision (.cgb, tools/kapf/cgb.py), from the user's own CoD2.

CoD2's side (lump numbers and records after riicchhaarrd/bsp.c, lump.h;
each size checked against mp_carentan: every lump divides exactly):
  4 planes, 16: f32 normal xyz, distance (inside: n.p <= d)
  5 brush sides, 8: the first six of a brush are its box as f32 xmin, xmax,
    ymin, ymax, zmin, zmax; the rest s32 plane; then s32 material
  6 brushes, 4: u16 sides, u16 material
  29 collision vertices, 16: s32 stamp, f32 xyz
  31 collision triangles, 72: plane, svec, tvec (f32 x4 each), u32 vertices
    x3, u32 edges x3
  33 partitions, 12: u16 stamp, u8 triangles, u8 borders, u32 first
    triangle, u32 first border
  34 collision aabb tree, 32: f32 origin xyz, half size xyz, s16 material,
    s16 children, s32 first child or (no children) partition
  35 models, 48: f32 mins, maxs, u32 first/count triangle soups, aabb
    nodes, brushes; model 0 is the world (the rest are entities' brushes)
  0 materials, 72: name[64], u32 surface flags, u32 contents

CoD2 -> CoD3 (both games' infoParms tables read from their executables
(CoD2MP_s.exe, codmp_xenonf.xex at 0x825D9B58): the same bits but these):
  contents 0x20000000: CoD2's compiler's, in no CoD3 volume: cleared.
  contents 0x40: CoD2 canshootclip, CoD3 physicsclip; CoD3's own clip
    volumes (player and monster clip) have it 14549 times of 14614: so
    clips get it, other volumes do not.
  contents 0x01000000 with surface 0x02000000/0x04000000: CoD2's mantle
    brushes; 0x01000000 is CoD3's origin/hmgstanding and CoD3 has no
    mantling: left out.
  surface 0x40000: CoD2 nocastshadow, CoD3 castshadow: cleared.
  volumes with no contents bits left but detail/structural (0x08000000,
    0x10000000: portals, hints) collide with nothing: left out.
  meshes: CoD3's never have 0x08000000/0x10000000: cleared.
  a brush whose material has no surface type (caulk: nodraw) takes the
    surface flags of its sides' most common material with one (CoD3's
    solid brushes all but never have type 0; the caulk ones' visible faces
    are stone, plaster, ...).
  brushes: CoD3 stores no axial planes (0 of 115284), so CoD2's extra
    axial sides narrow the box; a brush with no angled planes is a box.
  terrain triangles: wound clockwise seen from the front (as 48493 of
    49200 CoD3 ground triangles), the front by CoD2's triangle plane;
    grouped into meshes of at most 19 triangles and 24 points (the largest
    in CoD3's maps) and 510 x 510 x 250.

usage: python -I cod2collision.py <cod2 main> <map> <template.cgb> <out.cgb>
  [zone min x y z max x y z [--crop]]   (default: the world model's bounds)
template.cgb: a real CoD3 zone's .cgb, for the header's constant words.
--crop [min x y z max x y z]: only the volumes that reach into the zone, or
  into this box (as CoD3's zones hold the volumes touching them, some
  reaching past the zone's bounds).
"""
import math
import struct
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "kapf"))
import cgb  # noqa: E402
import cod2import  # noqa: E402

DETAIL_STRUCTURAL = 0x18000000
PLAYER_MONSTER_CLIP = 0x30000
TYPE_MASK = 0x1F00000
MANTLE = 0x06000000


def lumps(d):
    if d[:4] != b"IBSP" or struct.unpack_from("<i", d, 4)[0] != 4:
        raise ValueError("not a CoD2 IBSP 4 map")
    table = [struct.unpack_from("<II", d, 8 + 8 * i) for i in range(39)]
    return lambda i: d[table[i][1]:table[i][1] + table[i][0]]


def records(data, fmt):
    size = struct.calcsize(fmt)
    return [struct.unpack_from(fmt, data, i) for i in range(0, len(data) - size + 1, size)]


def contents3(cont, mesh=False):
    c = cont & ~0x20000000 & ~0x40
    if c & PLAYER_MONSTER_CLIP == PLAYER_MONSTER_CLIP:
        c |= 0x40
    if mesh:
        c &= ~DETAIL_STRUCTURAL
    return c


def surface3(surf):
    return surf & ~0x40000


def collides(cont):
    return cont & ~0x20000000 & ~DETAIL_STRUCTURAL != 0


def read(d):
    lump = lumps(d)
    materials = [(m[:64].split(b"\0")[0].decode("latin-1"), *struct.unpack_from("<II", m, 64))
                 for m in (lump(0)[i:i + 72] for i in range(0, len(lump(0)), 72))]
    planes = records(lump(4), "<4f")
    sides = lump(5)
    brushes = records(lump(6), "<HH")
    models = records(lump(35), "<6f6I")
    world = models[0]
    first_brush, n_brush = world[10], world[11]
    out_brushes, at = [], 0
    for k, (n, mat) in enumerate(brushes):
        box = [struct.unpack_from("<f", sides, 8 * (at + s))[0] for s in range(6)]
        side_mats = [struct.unpack_from("<i", sides, 8 * (at + s) + 4)[0] for s in range(n)]
        angled = [planes[struct.unpack_from("<i", sides, 8 * (at + s))[0]] for s in range(6, n)]
        if first_brush <= k < first_brush + n_brush:
            out_brushes.append((mat, side_mats, list(box), angled))
        at += n
    if 8 * at != len(sides):
        raise ValueError("brush sides do not add up")
    verts = [v[1:] for v in records(lump(29), "<i3f")]
    tris = records(lump(31), "<12f6I")
    parts = records(lump(33), "<HBBII")
    nodes = records(lump(34), "<6fhhi")
    terrain, seen = [], set()
    for node in nodes:  # leaves: a partition each, with the material
        if node[7] > 0:
            continue
        mat, part = node[6], parts[node[8]]
        for t in range(part[3], part[3] + part[1]):
            pts = corners(tris[t], verts)
            if pts is None:
                continue
            # the same triangle in two partitions once: by its corners (by its
            # vertex numbers, rebuilt ones sharing a 0xFFFFFFFF were taken
            # for one another)
            key = tuple(sorted(tuple(round(c, 2) for c in p) for p in pts))
            if key in seen:
                continue
            seen.add(key)
            terrain.append((mat, tris[t][0:3], pts))
    return materials, out_brushes, terrain, world


def corners(tri, verts):
    """A collision triangle's three corners. 2026-11-01: 266 of Carentan's
    13283 have one vertex number 0xFFFFFFFF; they were left out, and players
    and bots fell through those floors (streets at 0..512 2048..2560 and
    more). CoD2 collides by the triangle's plane and its two edge equations,
    not by the vertices: s = svec.p + svec.w, t = tvec.p + tvec.w give its
    corners (s0, t0), (s0 + 1, t0), (s0, t0 + 1) (checked on every whole
    triangle), so a missing corner is the point on the plane with the
    corner's s and t, found from any one known corner."""
    idx = tri[12:15]
    missing = [k for k in range(3) if idx[k] >= len(verts)]
    if not missing:
        return [verts[i] for i in idx]
    plane, sv, tv = tri[0:4], tri[4:8], tri[8:12]
    if len(missing) == 3:
        # none known (2 of Carentan's): its corners are collision vertices on
        # its plane whose s and t are (s0, t0), (s0 + 1, t0), (s0, t0 + 1)
        on = []
        for v in verts:
            if abs(sum(plane[i] * v[i] for i in range(3)) - plane[3]) < 0.05:
                on.append((sum(sv[i] * v[i] for i in range(3)) + sv[3], sum(tv[i] * v[i] for i in range(3)) + tv[3], v))
        for s0, t0, v0 in on:
            v1 = next((v for s, t, v in on if abs(s - s0 - 1) < 0.002 and abs(t - t0) < 0.002), None)
            v2 = next((v for s, t, v in on if abs(s - s0) < 0.002 and abs(t - t0 - 1) < 0.002), None)
            if v1 is not None and v2 is not None:
                return [v0, v1, v2]
        return None
    # the corners' offsets from corner 0: (0, 0), (1, 0), (0, 1); one known
    # corner fixes s0 and t0 (16 of Carentan's have two corners missing)
    offset = {0: (0.0, 0.0), 1: (1.0, 0.0), 2: (0.0, 1.0)}
    k0 = next(k for k in range(3) if k not in missing)
    p0 = verts[idx[k0]]
    s0 = sum(sv[i] * p0[i] for i in range(3)) + sv[3] - offset[k0][0]
    t0 = sum(tv[i] * p0[i] for i in range(3)) + tv[3] - offset[k0][1]

    def det3(a):
        return (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
                + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))

    out = []
    for k, i in enumerate(idx):
        if k not in missing:
            out.append(verts[i])
            continue
        s, t = s0 + offset[k][0], t0 + offset[k][1]
        a = [list(plane[0:3]), list(sv[0:3]), list(tv[0:3])]
        b = [plane[3], s - sv[3], t - tv[3]]
        det = det3(a)
        if abs(det) < 1e-12:
            return None
        p = []
        for col in range(3):   # Cramer's rule
            m3 = [row[:] for row in a]
            for r in range(3):
                m3[r][col] = b[r]
            p.append(det3(m3) / det)
        out.append(tuple(p))
    return out


def brush_flags(materials, mat, side_mats):
    name, surf, cont = materials[mat]
    if surf & TYPE_MASK == 0:
        typed = Counter(materials[m][1] for m in side_mats if materials[m][1] & TYPE_MASK)
        if typed:
            surf = typed.most_common(1)[0][0]
    return contents3(cont), surface3(surf)


LIMIT = (510, 510, 250)  # the largest corner-set brush in CoD3's maps


def faces_of(lo, hi, planes):
    """The planes that cut the brush: a plane is left out when the brush
    without it already keeps to it (by 0.01); CoD3's brushes list no
    plane that misses its box."""
    keep = list(planes)
    for p in list(planes):
        rest = [q for q in keep if q is not p]
        vs = cgb.brush_vertices(lo, hi, rest)
        if all(p[0] * v[0] + p[1] * v[1] + p[2] * v[2] <= p[3] + 0.01 for v in vs):
            keep = rest
    return keep


def split(lo, hi, planes):
    """The brush (box lo..hi cut by planes) in axial slices of at most
    LIMIT, each slice's box tight on its own corners; slices the planes
    leave empty are dropped. Together they are the same solid."""
    cuts = []
    for a in range(3):
        n = max(1, math.ceil((hi[a] - lo[a]) / LIMIT[a]))
        cuts.append([lo[a] + (hi[a] - lo[a]) * i / n for i in range(n)] + [hi[a]])
    out = []
    for i in range(len(cuts[0]) - 1):
        for j in range(len(cuts[1]) - 1):
            for k in range(len(cuts[2]) - 1):
                slo = [cuts[0][i], cuts[1][j], cuts[2][k]]
                shi = [cuts[0][i + 1], cuts[1][j + 1], cuts[2][k + 1]]
                vs = cgb.brush_vertices(slo, shi, planes)
                if len(vs) < 4:
                    continue
                vlo = [max(slo[a], min(v[a] for v in vs)) for a in range(3)]
                vhi = [min(shi[a], max(v[a] for v in vs)) for a in range(3)]
                if all(vhi[a] - vlo[a] > 1e-3 for a in range(3)):
                    out.append((vlo, vhi))
    return out


def convert_brushes(materials, brushes, report):
    boxes, out = [], []
    for mat, side_mats, box, angled in brushes:
        name, surf, cont = materials[mat]
        if cont & 0x01000000 and surf & MANTLE:
            report["left out: mantle"] += 1
            continue
        if not collides(cont):
            report[f"left out: no collision ({name})"] += 1
            continue
        lo, hi = [box[0], box[2], box[4]], [box[1], box[3], box[5]]
        keep = []
        for nx, ny, nz, dist in angled:
            # axial only when the other two are exactly 0 (a plane tilted by a thousandth is not)
            axis = [a for a, x in enumerate((nx, ny, nz)) if abs(x) == 1 or (nx, ny, nz).count(0.0) == 2 and x != 0]
            if axis:
                a = axis[0]
                if (nx, ny, nz)[a] > 0:
                    hi[a] = min(hi[a], dist)
                else:
                    lo[a] = max(lo[a], -dist)
            else:
                keep.append((nx, ny, nz, dist))
        if any(lo[a] >= hi[a] for a in range(3)):
            report["left out: empty box"] += 1
            continue
        flags = brush_flags(materials, mat, side_mats)
        if keep:
            vs = cgb.brush_vertices(lo, hi, keep)
            if len(vs) < 4:
                report["left out: no volume"] += 1
                continue
            # the box tight on the corners (the angled planes may cut it)
            lo = [max(lo[a], min(v[a] for v in vs)) for a in range(3)]
            hi = [min(hi[a], max(v[a] for v in vs)) for a in range(3)]
            pieces = [(lo, hi)]
            if flags[0] & cgb.HAS_CORNERS and any(hi[a] - lo[a] > LIMIT[a] for a in range(3)):
                pieces = split(lo, hi, keep)
                report["brushes split for corner packing"] += 1
                report["  into pieces"] += len(pieces)
            for plo, phi in pieces:
                faces = faces_of(plo, phi, keep)
                report["redundant planes dropped"] += len(keep) - len(faces)
                if faces:
                    out.append((*flags, plo, phi, faces))
                    report["brushes"] += 1
                else:
                    boxes.append((*flags, plo, phi))
                    report["boxes (brushes whose planes only touch the box)"] += 1
        else:
            boxes.append((*flags, lo, hi))
            report["boxes"] += 1
    return boxes, out


def fits(points):
    lo = [min(p[a] for p in points) for a in range(3)]
    hi = [max(p[a] for p in points) for a in range(3)]
    return hi[0] - lo[0] <= 510 and hi[1] - lo[1] <= 510 and hi[2] - lo[2] <= 250


def halve(tri):
    """The triangle cut at its longest edge's middle until each piece fits a
    mesh (same plane, same winding)."""
    if fits(tri):
        return [tri]
    a, b, c = tri
    edges = [(math.dist(a, b), 0), (math.dist(b, c), 1), (math.dist(c, a), 2)]
    k = max(edges)[1]
    p, q, r = (a, b, c), (b, c, a), (c, a, b)
    u, v, w = (p, q, r)[k]  # the longest edge u-v, w across it
    m = tuple((u[i] + v[i]) / 2 for i in range(3))
    return halve((u, m, w)) + halve((m, v, w))


def convert_terrain(materials, terrain, report):
    """Triangles into meshes, a mesh one material's neighbours in the order
    CoD2's tree keeps them (its partitions are small and close together)."""
    meshes, cur, cur_key = [], [], None

    def close():
        if cur:
            meshes.append((*cur_key, list(cur)))
            cur.clear()

    for mat, plane, (a, b, c) in terrain:
        name, surf, cont = materials[mat]
        if not collides(cont) or (cont & 0x01000000 and surf & MANTLE):
            report[f"terrain left out ({name})"] += 1
            continue
        n = ((b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]),
             (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
             (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
        if n[0] ** 2 + n[1] ** 2 + n[2] ** 2 < 1e-12:
            report["terrain left out: no area"] += 1
            continue
        # clockwise seen from the front: (b - a) x (c - a) away from the plane's normal
        tri = (a, c, b) if n[0] * plane[0] + n[1] * plane[1] + n[2] * plane[2] > 0 else (a, b, c)
        key = (contents3(cont, mesh=True), surface3(surf))
        pieces = halve(tri)
        if len(pieces) > 1:
            report["terrain triangles halved to fit a mesh"] += 1
        for tri in pieces:
            pts = [p for t in cur for p in t] + list(tri)
            unique = len({tuple(math.trunc(x * 4) for x in p) for p in pts})
            if key != cur_key or len(cur) + 1 > cgb.MESH_TRIS or 4 * math.ceil(unique / 4) > cgb.MESH_POINTS or not fits(pts):
                close()
                cur_key = key
            cur.append(tri)
            report["terrain triangles"] += 1
    close()
    report["meshes"] = len(meshes)
    return meshes


def main():
    main_dir, name, template, out = sys.argv[1:5]
    d = cod2import.Archives(main_dir).read(f"maps/mp/{name}.d3dbsp")
    materials, brushes, terrain, world = read(d)
    zone = [float(x) for x in sys.argv[5:11]] if len(sys.argv) >= 11 else list(world[:6])
    report = Counter()
    boxes, brushes3 = convert_brushes(materials, brushes, report)
    meshes = convert_terrain(materials, terrain, report)
    if "--crop" in sys.argv:
        at = sys.argv.index("--crop")
        box = [float(x) for x in sys.argv[at + 1:at + 7]] if len(sys.argv) >= at + 7 else zone

        def touches(lo, hi):
            return all(lo[a] <= box[3 + a] and hi[a] >= box[a] for a in range(3))
        boxes = [b for b in boxes if touches(b[2], b[3])]
        brushes3 = [b for b in brushes3 if touches(b[2], b[3])]
        meshes = [m for m in meshes if touches([min(p[a] for t in m[2] for p in t) for a in range(3)],
                                               [max(p[a] for t in m[2] for p in t) for a in range(3)])]
        report["after the crop: volumes"] = len(boxes) + len(brushes3) + len(meshes)
    header = Path(template).read_bytes()[:0xD0]
    data = cgb.build(header, zone[:3], zone[3:6], boxes, brushes3, meshes)
    Path(out).write_bytes(data)
    c = cgb.Cgb(data)
    bad = c.check()
    for k, v in sorted(report.items()):
        print(f"  {k}: {v}")
    print(f"{out}: {len(data)} bytes, {c.nboxes} boxes, {c.nbrushes} brushes ({c.ncorner_sets} with corners), "
          f"{c.nmesh} meshes, {c.nplanes} planes, {c.ncorners} corners, {c.npoints} points; tree {c.roots} roots, "
          f"{c.levels} levels: {'OK' if not bad else bad}")


if __name__ == "__main__":
    main()
