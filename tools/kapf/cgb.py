"""CoD3's zone collision (.cgb), read from the user's own game files
(worked out 2026-11-01 on Rouen zone 1 and the small Forest/Poissons ones;
checked on every .cgb of the multiplayer maps). Big-endian.

Header (0xD0 bytes; words not named are the exporter's stale pointers):
  0x00 f32 min xyz, u32 0x58; 0x10 f32 max xyz; 0x20 f32 centre xyz;
  0x30 f32 radius; 0x38 u32 boxes << 16 | brushes; then nine (stale, count)
  pairs, counts at 0x3C + 8k: volumes, brushes, brush corner sets, meshes
  (twice: triangle ranges, point ranges), planes, brush corners, triangle
  indices, mesh points; 0x9C..0xAC f32 1.0 x5; 0xB8 u32 the tree's root
  entries, 0xBC u32 its levels, 0xC0 u32 its bytes.
The arrays follow in that order, each starting on 16 bytes (the gaps filled
with 0x43):
  volumes, 36 bytes: u32 flags, u32 flags, f32 centre xyz, half extents xyz,
    radius. First the plain boxes, then the brushes, then the meshes.
  brushes, u32: first plane << 16 | plane count (the brush = its volume's box
    cut by these planes).
  brush corner sets, 10 bytes: s16 origin xyz (floor of the brush's min),
    u16 first corner, u16 corner count. Brush k has set k (the last brushes
    may have none: Rouen zone 1 has 1732 sets for 1834 brushes).
  meshes' triangles, u32: first index << 16 | index count.
  meshes' points, 10 bytes: s16 origin xyz (the mesh volume's min), u16
    first point, u16 point count.
  planes, 16 bytes: f32 normal xyz, distance (inside: n.p <= d).
  brush corners and mesh points, 4 bytes: a little-endian 24-bit word w and
    a byte b: x = (w & 0x7FF) / 4, y = ((w >> 11) & 0x7FF) / 4, z = b +
    (w >> 22) / 4 (quarter units, from the origin: 0 to 511.75 across, 0 to
    255.75 up; the z quarters checked against the brushes' faces: the
    flagged corners' third-face distance median 0.40 without, 0.17 with).
  triangle indices: u8, three a triangle, into the mesh's points.
  the tree, 16 bytes an entry: s16 -lo x, hi x, -lo y, hi y, -lo z, hi z, u32
    a child group's byte offset (4 entries) or, at the last level, the
    volume's index; empty: 0x8001 x6, 0xFFFFFFFF. A leaf's lo/hi are C casts
    of the volume's f32 min - 1 and max + 1 (truncated toward zero); a
    parent's are its children's union exactly. Packed bottom-up: levels L is
    the least with ceil(volumes / 4^(L-1)) <= 16, that ceil being the roots;
    each level ceil(the level below / 4) entries, empties only at the end of
    its last group. Laid out by level (roots first), each level's run padded
    to 4 entries; the groups' order within a level is the exporter's own
    clustering (the game follows the offsets).
After the arrays, 0x43 filler up to 64 bytes, then the tree; the file ends
with it. Rules checked on all 49 MP zone files (2026-11-01):
  header centre = (min + max) / 2 (min/max: the zone's own bounds, not the
    volumes'); radius = max over volumes of |volume centre - centre| + the
    volume's radius, plus 1; a volume's radius = |half extents|.
  a brush has a corner set iff its first flags word has 0x1 or 0x200 (the
    exporter puts those brushes first); its corners are the brush's
    vertices, each coordinate truncated toward zero to a quarter unit, the
    origin the floor of their minimum, padded to a multiple of 4 by
    repeating the last (all fit 511.75 x 511.75 x 255.75).
  brushes hold no axial planes (0 of 115284: those are the box) and no
    plane that misses the box; a brush with none left is a plain box.
  meshes: points packed the same way (raw values, no rounding first: a
    98.99999 is floored to 98 and stored as 98.75), padded the same way,
    the volume their bounds; at most 24 points (padding included) and 19
    triangles, 510 x 510 x 250; ground triangles wound clockwise seen from
    above (48493 of 49200); flags never 0x08000000/0x10000000.

usage: python -I cgb.py <file.cgb> [more]     parse and check each
       python -I cgb.py --roundtrip <file.cgb> [more]
                write each back (and with a tree of our own) and compare
"""
import itertools
import math
import struct
import sys
from pathlib import Path

FILL = 0x43
EMPTY = struct.pack(">6hI", *[-32767] * 6, 0xFFFFFFFF)
SIZES = [36, 4, 10, 4, 10, 16, 4, 1, 4]
HAS_CORNERS = 0x201


def align16(x):
    return (x + 15) & ~15


def align64(x):
    return (x + 63) & ~63


def f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0]


def point(raw, origin):
    w = raw[0] | raw[1] << 8 | raw[2] << 16
    return (origin[0] + (w & 0x7FF) / 4.0, origin[1] + ((w >> 11) & 0x7FF) / 4.0, origin[2] + raw[3] + (w >> 22) / 4.0)


class Cgb:
    def __init__(self, d):
        self.d = d
        self.header = d[:0xD0]
        u = lambda a: struct.unpack_from(">I", d, a)[0]
        self.min = struct.unpack_from(">3f", d, 0x00)
        self.max = struct.unpack_from(">3f", d, 0x10)
        self.centre = struct.unpack_from(">3f", d, 0x20)
        self.radius = struct.unpack_from(">f", d, 0x30)[0]
        split = u(0x38)
        self.nboxes, self.nbrushes = split >> 16, split & 0xFFFF
        counts = [u(0x3C + 8 * k) for k in range(9)]
        (self.nvol, nb, self.ncorner_sets, self.nmesh, nmesh2, self.nplanes, self.ncorners, self.nindices, self.npoints) = counts
        self.counts = counts
        self.roots, self.levels, self.tree_bytes = u(0xB8), u(0xBC), u(0xC0)
        at = 0xD0
        self.offsets = []
        for n, s in zip(counts, SIZES):
            self.offsets.append(at)
            at = align16(at + n * s)
        self.tree_at = len(d) - self.tree_bytes
        self.end_of_arrays = at
        o = self.offsets
        self.volumes = [struct.unpack_from(">II7f", d, o[0] + 36 * i) for i in range(self.nvol)]
        self.brushes = [u(o[1] + 4 * i) for i in range(nb)]
        self.corner_sets = [struct.unpack_from(">3hHH", d, o[2] + 10 * i) for i in range(self.ncorner_sets)]
        self.tris = [u(o[3] + 4 * i) for i in range(self.nmesh)]
        self.point_sets = [struct.unpack_from(">3hHH", d, o[4] + 10 * i) for i in range(nmesh2)]
        self.planes = [struct.unpack_from(">4f", d, o[5] + 16 * i) for i in range(self.nplanes)]
        self.corners_raw = [d[o[6] + 4 * i:o[6] + 4 * i + 4] for i in range(self.ncorners)]
        self.indices = d[o[7]:o[7] + self.nindices]
        self.points_raw = [d[o[8] + 4 * i:o[8] + 4 * i + 4] for i in range(self.npoints)]
        self.tree = d[self.tree_at:]

    def tree_entry(self, offset):
        return struct.unpack_from(">6hI", self.d, self.tree_at + offset)

    def check(self):
        bad = {}
        def miss(what):
            bad[what] = bad.get(what, 0) + 1
        if self.nboxes + self.nbrushes + self.nmesh != self.nvol:
            miss("split")
        if self.end_of_arrays > self.tree_at:
            miss("arrays overrun the tree")
        b0 = self.nboxes
        for k, packed in enumerate(self.brushes):
            v = self.volumes[b0 + k]
            first, count = packed >> 16, packed & 0xFFFF
            for nx, ny, nz, dd in self.planes[first:first + count]:
                proj = abs(nx) * v[5] + abs(ny) * v[6] + abs(nz) * v[7]
                if abs(nx * v[2] + ny * v[3] + nz * v[4] - dd) > proj + 1.0:
                    miss("brush plane misses its box")
            if k < len(self.corner_sets):
                ox, oy, oz, first, count = self.corner_sets[k]
                for raw in self.corners_raw[first:first + count]:
                    p = point(raw, (ox, oy, oz))
                    if any(abs(p[c] - v[2 + c]) > v[5 + c] + 1.0 for c in range(3)):
                        miss("brush corner outside its box")
        m0 = self.nboxes + self.nbrushes
        for m, packed in enumerate(self.tris):
            v = self.volumes[m0 + m]
            first, count = packed >> 16, packed & 0xFFFF
            ox, oy, oz, pfirst, pcount = self.point_sets[m]
            if count % 3:
                miss("triangle indices not in threes")
            if any(i >= pcount for i in self.indices[first:first + count]):
                miss("triangle index past its points")
            for raw in self.points_raw[pfirst:pfirst + pcount]:
                p = point(raw, (ox, oy, oz))
                if any(abs(p[c] - v[2 + c]) > v[5 + c] + 1.0 for c in range(3)):
                    miss("mesh point outside its box")
        seen = {}
        def walk(off, count, depth, parent):
            for k in range(count):
                e = self.tree_entry(off + 16 * k)
                if e[6] == 0xFFFFFFFF:
                    continue
                box = (-e[0], e[1], -e[2], e[3], -e[4], e[5])
                if parent and not (box[0] >= parent[0] and box[1] <= parent[1] and box[2] >= parent[2] and box[3] <= parent[3]
                                   and box[4] >= parent[4] and box[5] <= parent[5]):
                    miss("tree child outside its parent")
                if depth == self.levels:
                    seen[e[6]] = seen.get(e[6], 0) + 1
                else:
                    walk(e[6], 4, depth + 1, box)
        walk(0, self.roots, 1, None)
        if len(seen) != self.nvol or any(c != 1 for c in seen.values()):
            miss("tree leaves not each volume once")
        return bad


def write(c):
    """The file's bytes from c's fields (a Cgb, or what build() makes): the
    header's other words (stale pointers, constants) come from c.header."""
    h = bytearray(c.header)
    struct.pack_into(">3f", h, 0x00, *c.min)
    struct.pack_into(">3f", h, 0x10, *c.max)
    struct.pack_into(">3f", h, 0x20, *c.centre)
    struct.pack_into(">f", h, 0x30, c.radius)
    struct.pack_into(">I", h, 0x38, c.nboxes << 16 | len(c.brushes))
    counts = [len(c.volumes), len(c.brushes), len(c.corner_sets), len(c.tris), len(c.point_sets), len(c.planes),
              len(c.corners_raw), len(c.indices), len(c.points_raw)]
    for k, n in enumerate(counts):
        struct.pack_into(">I", h, 0x3C + 8 * k, n)
    struct.pack_into(">3I", h, 0xB8, c.roots, c.levels, len(c.tree))
    arrays = [b"".join(struct.pack(">II7f", *v) for v in c.volumes),
              b"".join(struct.pack(">I", b) for b in c.brushes),
              b"".join(struct.pack(">3hHH", *s) for s in c.corner_sets),
              b"".join(struct.pack(">I", t) for t in c.tris),
              b"".join(struct.pack(">3hHH", *s) for s in c.point_sets),
              b"".join(struct.pack(">4f", *p) for p in c.planes),
              b"".join(c.corners_raw), bytes(c.indices), b"".join(c.points_raw)]
    out = bytearray(h)
    for a in arrays:
        out += a
        out += bytes([FILL]) * (align16(len(out)) - len(out))
    out += bytes([FILL]) * (align64(len(out)) - len(out))
    return bytes(out + c.tree)


def leaf_box(v):
    box = []
    for a in range(3):
        lo, hi = f32(v[2 + a] - v[5 + a]), f32(v[2 + a] + v[5 + a])
        box += [int(f32(lo - 1)), int(f32(hi + 1))]
    return box


def morton(v, lo, span):
    key = 0
    q = [min(1023, int((v[2 + a] - lo[a]) / span[a] * 1023)) for a in range(3)]
    for bit in range(10):
        for a in range(3):
            key |= (q[a] >> bit & 1) << (3 * bit + a)
    return key


def build_tree(volumes):
    """(roots, levels, bytes): a tree packed by the game's rules; the
    volumes are grouped in Morton order of their centres (the exporter's
    own grouping is not known, and the game only follows the offsets)."""
    n = len(volumes)
    if n == 0:
        return 0, 1, b""
    levels = 1
    while math.ceil(n / 4 ** (levels - 1)) > 16:
        levels += 1
    lo = [min(v[2 + a] for v in volumes) for a in range(3)]
    span = [max(max(v[2 + a] for v in volumes) - lo[a], 1.0) for a in range(3)]
    order = sorted(range(n), key=lambda i: morton(volumes[i], lo, span))
    tiers = [[(leaf_box(volumes[i]), i) for i in order]]  # bottom first: (box, volume index or group number)
    while len(tiers) < levels:
        below = tiers[-1]
        up = []
        for g in range(0, len(below), 4):
            kids = [b for b, _ in below[g:g + 4]]
            up.append(([min(k[0] for k in kids), max(k[1] for k in kids), min(k[2] for k in kids),
                        max(k[3] for k in kids), min(k[4] for k in kids), max(k[5] for k in kids)], g // 4))
        tiers.append(up)
    tiers.reverse()
    starts, at = [], 0
    for t in tiers:
        starts.append(at)
        at += 64 * math.ceil(len(t) / 4)
    out = bytearray()
    for depth, t in enumerate(tiers):
        for box, ref in t:
            link = ref if depth == levels - 1 else starts[depth + 1] + 64 * ref
            out += struct.pack(">6hI", -box[0], box[1], -box[2], box[3], -box[4], box[5], link)
        out += EMPTY * (4 * math.ceil(len(t) / 4) - len(t))
    return len(tiers[0]), levels, bytes(out)


def solve3(p, q, r):
    (a, b, c, d), (e, f, g, h), (i, j, k, l) = p, q, r
    det = a * (f * k - g * j) - b * (e * k - g * i) + c * (e * j - f * i)
    if abs(det) < 1e-9:
        return None
    return ((d * (f * k - g * j) - b * (h * k - g * l) + c * (h * j - f * l)) / det,
            (a * (h * k - g * l) - d * (e * k - g * i) + c * (e * l - h * i)) / det,
            (a * (f * l - h * j) - b * (e * l - h * i) + d * (e * j - f * i)) / det)


def brush_vertices(lo, hi, planes, eps=0.01):
    """The corners of the box lo..hi cut by planes (n.p <= d inside)."""
    sides = [(1, 0, 0, hi[0]), (-1, 0, 0, -lo[0]), (0, 1, 0, hi[1]), (0, -1, 0, -lo[1]), (0, 0, 1, hi[2]),
             (0, 0, -1, -lo[2])] + [tuple(p) for p in planes]
    out = []
    for t in itertools.combinations(sides, 3):
        p = solve3(*t)
        if p is None or any(n0 * p[0] + n1 * p[1] + n2 * p[2] - dd > eps for n0, n1, n2, dd in sides):
            continue
        if not any(math.dist(p, o) < 0.05 for o in out):
            out.append(p)
    return out


def snap(x):
    # computed brush corners: float noise within 1/1000 of a quarter put back on it (the exporter
    # worked from the map's exact corners; mesh points are taken as they are, as it did)
    s = round(x * 4)
    return s / 4 if abs(x * 4 - s) < 0.004 else x


def pack_points(points):
    """(origin, packed points, index of each point among the unique ones):
    each coordinate truncated toward zero to a quarter unit, the origin the
    floor of their minimum, padded to a multiple of 4 by repeating the last."""
    origin = [math.floor(min(p[a] for p in points)) for a in range(3)]
    raw = []
    for p in points:
        rx, ry, rz = (round((math.trunc(p[a] * 4) / 4 - origin[a]) * 4) for a in range(3))
        if not (0 <= rx <= 2047 and 0 <= ry <= 2047 and 0 <= rz <= 1023):
            raise ValueError(f"point {p} out of the packing range from {origin}")
        w = rx | ry << 11 | (rz & 3) << 22
        raw.append(bytes([w & 0xFF, w >> 8 & 0xFF, w >> 16 & 0xFF, rz >> 2]))
    unique = list(dict.fromkeys(raw))
    where = [unique.index(r) for r in raw]
    unique += [unique[-1]] * (4 * math.ceil(len(unique) / 4) - len(unique))
    return origin, unique, where


def corner_set(vertices):
    """(origin, packed corners) by the exporter's rules."""
    origin, raw, _ = pack_points([[snap(x) for x in p] for p in vertices])
    return origin, raw


def volume(flags, lo, hi):
    centre = [f32((lo[a] + hi[a]) / 2) for a in range(3)]
    half = [f32((hi[a] - lo[a]) / 2) for a in range(3)]
    return (flags[0], flags[1], *centre, *half, f32(math.sqrt(sum(x * x for x in half))))


class Built:
    pass


MESH_POINTS, MESH_TRIS = 24, 19  # the most any MP map's mesh has (points with padding)


def build(header, zone_min, zone_max, boxes, brushes, meshes=()):
    """A collision file of plain boxes, brushes and meshes.
    header: 0xD0 bytes of a real zone's .cgb (its stale/constant words kept).
    boxes: [(flags0, flags1, min xyz, max xyz)]; brushes: the same plus
    their angled planes [(nx, ny, nz, d)] (axial sides go in min/max);
    meshes: [(flags0, flags1, triangles)], a triangle three xyz points wound
    clockwise seen from its front, at most MESH_TRIS triangles and
    MESH_POINTS points (padding included) in 511.75 x 511.75 x 255.75."""
    c = Built()
    c.header = header
    c.min, c.max = tuple(zone_min), tuple(zone_max)
    c.centre = tuple(f32((c.min[a] + c.max[a]) / 2) for a in range(3))
    brushes = sorted(brushes, key=lambda b: not (b[0] & HAS_CORNERS))  # stable: corner-set brushes first
    c.nboxes = len(boxes)
    c.volumes = [volume(b[:2], b[2], b[3]) for b in boxes] + [volume(b[:2], b[2], b[3]) for b in brushes]
    c.brushes, c.planes, c.corner_sets, c.corners_raw = [], [], [], []
    for flags0, flags1, lo, hi, planes in brushes:
        if len(c.planes) + len(planes) > 0xFFFF:
            raise ValueError("more than 65535 planes")
        c.brushes.append(len(c.planes) << 16 | len(planes))
        c.planes += [tuple(f32(x) for x in p) for p in planes]
        if flags0 & HAS_CORNERS:
            vs = brush_vertices(lo, hi, planes)
            if len(vs) < 4:
                raise ValueError(f"brush {lo} {hi} has {len(vs)} corners")
            origin, raw = corner_set(vs)
            if len(c.corners_raw) + len(raw) > 0xFFFF:
                raise ValueError("more than 65535 brush corners")
            c.corner_sets.append((*origin, len(c.corners_raw), len(raw)))
            c.corners_raw += raw
    c.tris, c.point_sets, c.indices, c.points_raw = [], [], bytearray(), []
    for flags0, flags1, triangles in meshes:
        pts = [p for t in triangles for p in t]
        origin, raw, where = pack_points(pts)
        if len(triangles) > MESH_TRIS or len(raw) > MESH_POINTS:
            raise ValueError(f"mesh of {len(triangles)} triangles, {len(raw)} points")
        if len(c.indices) > 0xFFFF or len(c.points_raw) + len(raw) > 0xFFFF:
            raise ValueError("more than 65535 mesh indices or points")
        c.tris.append(len(c.indices) << 16 | len(where))
        c.point_sets.append((*origin, len(c.points_raw), len(raw)))
        c.indices += bytes(where)
        c.points_raw += raw
        c.volumes.append(volume((flags0, flags1), [min(p[a] for p in pts) for a in range(3)],
                                [max(p[a] for p in pts) for a in range(3)]))
    c.indices = bytes(c.indices)
    radius = max((math.dist(c.centre, v[2:5]) + v[8] for v in c.volumes), default=0.0) + 1
    c.radius = f32(radius)  # 1.0 with no volumes, as the empty unzoned files have
    c.roots, c.levels, c.tree = build_tree(c.volumes)
    return write(c)


def roundtrip(path):
    d = Path(path).read_bytes()
    c = Cgb(d)
    notes = ["bytes " + ("same" if write(c) == d else "DIFFER")]
    if c.nvol:
        r = max(math.dist(c.centre, v[2:5]) + v[8] for v in c.volumes) + 1
        notes.append(f"radius {'ok' if abs(r - c.radius) < 0.01 else f'{r} vs {c.radius}'}")
    roots, levels, tree = build_tree(c.volumes)
    c.roots, c.levels, c.tree = roots, levels, tree
    ours = Cgb(write(c))
    bad = ours.check()
    notes.append(f"our tree {'OK' if not bad else bad}, {len(tree)} bytes ({'same size' if len(tree) == c.tree_bytes else 'size differs'})")
    same = near = 0
    for k in range(c.ncorner_sets):
        v = c.volumes[c.nboxes + k]
        first, count = c.brushes[k] >> 16, c.brushes[k] & 0xFFFF
        lo = [v[2 + a] - v[5 + a] for a in range(3)]
        hi = [v[2 + a] + v[5 + a] for a in range(3)]
        try:
            origin, raw = corner_set(brush_vertices(lo, hi, c.planes[first:first + count]))
        except ValueError:
            continue
        ox, oy, oz, f0, n = c.corner_sets[k]
        theirs = c.corners_raw[f0:f0 + n]
        if tuple(origin) == (ox, oy, oz) and sorted(set(raw)) == sorted(set(theirs)):
            same += 1
        elif tuple(origin) == (ox, oy, oz) and len(set(raw)) == len(set(theirs)):
            near += 1
    if c.ncorner_sets:
        notes.append(f"corner sets rebuilt: {same} exact, {near} same count, of {c.ncorner_sets}")
    return notes


def main():
    if sys.argv[1:2] == ["--roundtrip"]:
        for path in sys.argv[2:]:
            print(f"{Path(path).name}: " + "; ".join(roundtrip(path)))
        return
    for path in sys.argv[1:]:
        c = Cgb(Path(path).read_bytes())
        bad = c.check()
        print(f"{Path(path).name}: {c.nboxes} boxes, {c.nbrushes} brushes ({c.ncorner_sets} with corners), {c.nmesh} meshes, "
              f"{c.nplanes} planes, {c.ncorners} corners, {c.nindices // 3} triangles, {c.npoints} points; tree {c.roots} roots, "
              f"{c.levels} levels: {'OK' if not bad else bad}")


if __name__ == "__main__":
    main()
