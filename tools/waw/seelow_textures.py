"""Seelow's world textures out of the user's own World at War install
(2026-11-02): every colour image seelow_materials.py names, decoded from the
main\\iw_NN.iwd archives (a later archive wins, as the game loads them) to
PNG. Images whose pixels sit in the zone itself (global_black ...) are drawn
as their flat colour.

usage: python -I seelow_textures.py <WaW main folder> <materials.tsv> <out folder>
"""
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from wawimport import decode_iwi   # noqa: E402

FLAT = {"global_black_c": (0, 0, 0, 255), "case64blue": (40, 60, 70, 255),
        "water_seelow1_c": (52, 58, 44, 255), "sky_see1_c": (150, 160, 170, 255)}


def iwd_index(main):
    index = {}
    for iwd in sorted(Path(main).glob("iw_*.iwd")):
        with zipfile.ZipFile(iwd) as z:
            for n in z.namelist():
                low = n.lower()
                if low.startswith("images/") and low.endswith(".iwi"):
                    index[low[7:-4]] = (iwd, n)
    return index


def main():
    from PIL import Image
    main_dir, tsv, out = sys.argv[1], sys.argv[2], Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    names = sorted({img for line in open(tsv) for img in line.rstrip("\n").split("\t")[2].split()})
    index = iwd_index(main_dir)
    missing = []
    for name in names:
        dest = out / f"{name.replace('#', '_')}.png"
        if dest.exists():
            continue
        if name in FLAT:
            Image.new("RGBA", (4, 4), FLAT[name]).save(dest)
            continue
        hit = index.get(name.lower()) or index.get(name.lower().split("#")[0])
        if not hit:
            missing.append(name)
            continue
        with zipfile.ZipFile(hit[0]) as z:
            img = decode_iwi(z.read(hit[1]))
        img.save(dest)
        print(f"{name}: {img.size[0]}x{img.size[1]} from {hit[0].name}")
    print(f"{len(names) - len(missing)} of {len(names)} images; missing: {missing}")


if __name__ == "__main__":
    main()
