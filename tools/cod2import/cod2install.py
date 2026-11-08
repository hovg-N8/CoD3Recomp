"""Copy a converted CoD2 map (cod2map.py and cod2assets.py's output) into the
CoD3 install's custom\\<map>\\ folder, for the port's map draw (0.5.667,
mp_fixes.cpp Cod2Map). Writes pieces.txt, one drawn piece a line:
  <mesh file> <colour png> <normal png or -> <specular png or ->
and spawns.txt (cod2map's spawn list). Sky materials are left out (CoD3's own
sky stays).

usage:
  python -I cod2install.py <converted folder> <map name> <custom folder>
e.g. python -I cod2install.py "D:\\Call of Duty 2 (USA)\\carentan" mp_carentan
       "D:\\Call of Duty 3 (USA, Europe)\\CoD3-PC-Port-0.4.0\\custom\\carentan"
"""
import shutil
import sys
from pathlib import Path


def safe_mesh(mat):
    return "".join(ch if ch.isalnum() or ch in "_-" else "_" for ch in mat)


def safe_image(image):
    return "".join(ch if ch.isalnum() or ch in "_-~&." else "_" for ch in image) + ".png"


def main():
    src, name, dst = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
    dst.mkdir(parents=True, exist_ok=True)
    maps = {}
    for line in (src / f"{name}_material_maps.txt").read_text().splitlines():
        words = line.split()
        if len(words) == 4:
            maps[words[0]] = words[1:]
    lines, missing, copied = [], [], set()

    def image(img):
        if img == "-" or img.startswith("$"):
            return "-"
        file = safe_image(img)
        if not (src / "images" / file).exists():
            missing.append(img)
            return "-"
        if file not in copied:
            shutil.copyfile(src / "images" / file, dst / file)
            copied.add(file)
        return file

    for mat in (src / f"{name}_materials.txt").read_text().split():
        if "sky" in mat.lower():
            continue
        mesh = f"{name}__{safe_mesh(mat)}.mesh"
        if not (src / mesh).exists():
            missing.append(mesh)
            continue
        shutil.copyfile(src / mesh, dst / mesh)
        colour, normal, spec = maps.get(mat, ["-", "-", "-"])
        lines.append(f"{mesh} {image(colour)} {image(normal)} {image(spec)}")
    (dst / "pieces.txt").write_text("\n".join(lines) + "\n")
    shutil.copyfile(src / f"{name}_spawns.txt", dst / "spawns.txt")
    print(f"{name}: {len(lines)} pieces, {len(copied)} images copied to {dst}")
    if missing:
        print(f"{len(missing)} missing: " + ", ".join(missing[:20]))


if __name__ == "__main__":
    main()
