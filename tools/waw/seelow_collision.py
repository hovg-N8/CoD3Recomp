"""A first collision for Seelow (2026-11-02): its drawn world's solid
triangles as CoD3 collision meshes (tools/cod2import/cod2collision.py
convert_terrain, the way CoD2's terrain triangles go), so the level can be
walked before World at War's own clip map (brushes, player clip) is read.

Left out: sky, decals, ivy and foliage (seen through, walked through in WaW),
the water's surface (the river bed under it is walked), and everything
outside the level's box. A triangle's front is the side its vertex normals
face. Surface types (footsteps, bullet effects) by the layer 0 image's name;
CoD2's and CoD3's numbering (checked on Carentan's materials): 2 brick,
5 concrete, 6 dirt, 9 glass, 10 grass, 11 gravel, 13 metal, 14 mud, 16
plaster, 17 rock, 21 wood.

usage: python -I seelow_collision.py <mp_seelow.zone> <materials.tsv> <template.cgb> <out.cgb>
  template.cgb: a real CoD3 zone's (carentan-work\\rouen-coll\\mp_rouen_zone01.cgb)
"""
import struct
import sys
from array import array
from collections import Counter
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "cod2import"))
sys.path.insert(0, str(HERE.parent / "kapf"))
import seelow_world as sw   # noqa: E402
import seelow_cells as sc   # noqa: E402
import cod2collision   # noqa: E402
import cgb   # noqa: E402

TYPES = [("grass", 10), ("mud", 14), ("mulch", 6), ("dirt", 6), ("farmfield", 6), ("gravel", 11),
         ("rubble", 17), ("rock", 17), ("stone", 17), ("brick", 2), ("concrete", 5), ("plaster", 16),
         ("wallpaper", 16), ("metal", 13), ("rust", 13), ("corrugated", 13), ("window", 9),
         ("wood", 21), ("plank", 21), ("door", 21), ("roof", 21)]


def surface_type(image):
    for key, t in TYPES:
        if key in image:
            return t
    return 5


def passable(name, image):
    low = name.lower()
    return (low.startswith("wc/sky") or "decal" in image or "decal" in low or "ivy" in low
            or "foliage" in low or "water" in low or image in ("case64blue",))


def main():
    zone, tsv, template, out = sys.argv[1:5]
    z = open(zone, "rb").read()
    vstart, nv = sw.find_vertices(z)
    istart = sw.find_draw(z, nv) + 758
    sstart, ns = sw.find_surfaces(z, nv)
    world_mats = [line.rstrip("\n").split("\t") for line in open(tsv)]
    # CoD2-shaped materials: (name, surface flags, contents 1 = solid), one a world material
    materials = []
    for _i, name, images in world_mats:
        image = images.split()[0]
        materials.append((name, surface_type(image) << 20, 0 if passable(name, image) else 1))
    terrain, report = [], Counter()
    box = sc.BOX
    for i in range(ns):
        _vld, fv, vc, tc, bi, _w, mat = struct.unpack_from("<iiHHiII", z, sstart + 52 * i)
        m = (((mat - 1) & 0x1FFFFFFF) - sc.LIST_BLOCK) // 8
        a = array("H")
        a.frombytes(z[istart + 2 * bi:istart + 2 * (bi + 3 * tc)])
        for k in range(0, len(a), 3):
            tri = a[k:k + 3]
            if max(tri) >= vc:
                continue
            pts = [struct.unpack_from("<3f", z, vstart + 44 * (fv + j)) for j in tri]
            if any(not (box[0] <= p[0] <= box[3] and box[1] <= p[1] <= box[4] and box[2] <= p[2] <= box[5]) for p in pts):
                report["outside the level"] += 1
                continue
            normals = [sc.unpack_unit(struct.unpack_from("<I", z, vstart + 44 * (fv + j) + 36)[0]) for j in tri]
            front = [sum(n[c] for n in normals) for c in range(3)]
            terrain.append((m, front, tuple(pts)))
    meshes = cod2collision.convert_terrain(materials, terrain, report)
    header = Path(template).read_bytes()[:0xD0]
    data = cgb.build(header, list(box[:3]), list(box[3:6]), [], [], meshes)
    Path(out).write_bytes(data)
    c = cgb.Cgb(data)
    bad = c.check()
    for k, v in sorted(report.items()):
        print(f"  {k}: {v}")
    print(f"{out}: {len(data)} bytes, {c.nmesh} meshes, {c.npoints} points; tree {c.roots} roots, "
          f"{c.levels} levels: {'OK' if not bad else bad}")


if __name__ == "__main__":
    main()
