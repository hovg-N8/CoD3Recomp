"""CoD2's scoped Mosin-Nagant for CoD3's multiplayer (0.5.726): custom\\mosin\\
beside the game, from the user's own CoD2 (cod2import.py's meshes and images,
made first into a work folder):

  viewmodel_mosin.mesh   viewmodel_mosinnagant_cloth posed by viewmodel_mosincloth_idle,
                         the stripper clip left out (it is parked 98 units off), its
                         three materials' UVs moved into one atlas
  weapon_mosin.mesh      weapon_mosinnagantscoped_cloth, the same atlas
  mosin_c.png            the atlas, 1024 x 512: weapon_mosin_c (512 x 512) at 0,0;
                         weapon_mosin_cloth_c (512 x 256) at 512,0; mosin_cloth_frayed_c
                         (256 x 128) at 512,256 (every surface's UVs lie in 0..1)
  slots.txt              flat normal, grey specular (as the M2's)
  nagant_sniper.wav, nagant_rld.wav, hud_scoped_nagant.png

The game draws one colour map a mesh (Render::SetMeshTexture), hence the atlas.

usage: python -I make_mosin.py <work folder (cod2import outputs)> <custom\\mosin out>
"""
import shutil
import struct
import sys
from pathlib import Path

from PIL import Image

ATLAS = (1024, 512)
PLACES = {  # material: (x, y, width, height) in the atlas
    "mtl_weapon_mosin": (0, 0, 512, 512, "weapon_mosin_c.png"),
    "mtl_nagant_cloth": (512, 0, 512, 256, "weapon_mosin_cloth_c.png"),
    "mtl_nagant_cloth_frayed": (512, 256, 256, 128, "mosin_cloth_frayed_c.png"),
}


def read_mesh(path):
    d = Path(path).read_bytes()
    _magic, _v, nv, nt, ns = struct.unpack_from("<4sIIII", d, 0)
    verts = [list(struct.unpack_from("<3f3f2fI", d, 20 + 36 * i)) for i in range(nv)]
    o = 20 + 36 * nv
    tris = [struct.unpack_from("<3H", d, o + 6 * i) for i in range(nt)]
    o += 6 * nt
    surfaces = []
    for _ in range(ns):
        first, count, length = struct.unpack_from("<III", d, o)
        o += 12
        surfaces.append((first, count, d[o:o + length].decode()))
        o += length
    tail = d[o:]   # the bones, kept as they are
    return verts, tris, surfaces, tail


def write_mesh(path, verts, tris, surfaces, tail):
    with open(path, "wb") as f:
        f.write(b"C2MS" + struct.pack("<IIII", 1, len(verts), len(tris), len(surfaces)))
        for v in verts:
            f.write(struct.pack("<3f3f2fI", *v))
        for t in tris:
            f.write(struct.pack("<3H", *t))
        for first, count, name in surfaces:
            f.write(struct.pack("<III", first, count, len(name)) + name.encode())
        f.write(tail)


def atlas_mesh(src, dst):
    verts, tris, surfaces, tail = read_mesh(src)
    keep_tris, keep_surfs = [], []
    used = {}
    out_verts = []
    for first, count, name in surfaces:
        if name not in PLACES:
            print(f"  {Path(src).name}: surface {name} ({count} triangles) left out")
            continue
        x, y, w, h, _ = PLACES[name]
        start = len(keep_tris)
        for t in tris[first:first + count]:
            nt = []
            for k in t:
                key = (k, name)
                if key not in used:
                    v = list(verts[k])
                    v[6] = (x + v[6] * w) / ATLAS[0]
                    v[7] = (y + v[7] * h) / ATLAS[1]
                    used[key] = len(out_verts)
                    out_verts.append(v)
                nt.append(used[key])
            keep_tris.append(tuple(nt))
        keep_surfs.append((start, len(keep_tris) - start, name))
    write_mesh(dst, out_verts, keep_tris, keep_surfs, tail)
    print(f"  {dst.name}: {len(out_verts)} vertices, {len(keep_tris)} triangles, {len(keep_surfs)} surfaces")


def main():
    work, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    atlas = Image.new("RGBA", ATLAS, (128, 128, 128, 255))
    for name, (x, y, w, h, png) in PLACES.items():
        atlas.paste(Image.open(work / png).convert("RGBA").resize((w, h)), (x, y))
    atlas.save(out / "mosin_c.png")
    atlas_mesh(work / "viewmodel_mosin.mesh", out / "viewmodel_mosin.mesh")
    atlas_mesh(work / "weapon_mosin.mesh", out / "weapon_mosin.mesh")
    flame = out.parent / "flamethrower"
    for f in ("flat_nm.png", "grey.png", "dark_cube.png"):
        if (flame / f).exists():
            shutil.copyfile(flame / f, out / f)
    (out / "slots.txt").write_text("2=dark_cube.png 3=flat_nm.png 5=grey.png\n")
    for f in ("nagant_sniper.wav", "nagant_rld.wav", "hud_scoped_nagant.png"):
        shutil.copyfile(work / f, out / f)
    print(f"{out}: done")


if __name__ == "__main__":
    main()
