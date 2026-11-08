"""Convert what a CoD2 map uses (after cod2map.py): each material's textures
(its colour, normal and specular maps, as PNG) and each placed model (as a
.mesh), from the user's own CoD2 install. 0.5.666 (Carentan).

A CoD2 material file (materials/<name>): u32 name offset, u32 its colour
map's image name offset, ..., u16 texture count and u16 constant count at 0x34, u32 table offset at
0x3C; the table's 12-byte entries: u32 slot name offset ("colorMap",
"normalMap", "specularMap", "detailMap"), u32 flags, u32 image name offset.

usage:
  python -I cod2assets.py <cod2 main folder> <map name> <out folder>
reads <out>/<map>_materials.txt and <out>/<map>_models.txt (cod2map.py's),
writes <out>/images/<image>.png, <out>/models/<model>.mesh and
<out>/<map>_material_maps.txt (material colorMap normalMap specularMap).
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cod2import  # noqa: E402


def cstr(d, at):
    end = d.index(b"\0", at)
    return d[at:end].decode("latin-1")


def material_maps(data):
    count, table = struct.unpack_from("<H", data, 0x34)[0], struct.unpack_from("<I", data, 0x3C)[0]   # u16 textures, u16 constants
    maps = {}
    for i in range(min(count, 16)):
        slot_off, _flags, image_off = struct.unpack_from("<III", data, table + 12 * i)
        maps[cstr(data, slot_off)] = cstr(data, image_off)
    return maps


def main():
    main_dir, name, out = sys.argv[1:4]
    out = Path(out)
    (out / "images").mkdir(parents=True, exist_ok=True)
    (out / "models").mkdir(parents=True, exist_ok=True)
    arch = cod2import.Archives(main_dir)
    images, lines, problems = set(), [], []

    def take_material(mat):
        try:
            maps = material_maps(arch.read(f"materials/{mat}"))
        except (FileNotFoundError, struct.error, ValueError) as e:
            problems.append(f"material {mat}: {e}")
            return
        lines.append(f"{mat} {maps.get('colorMap', '-')} {maps.get('normalMap', '-')} {maps.get('specularMap', '-')}")
        images.update(v for v in maps.values() if v)

    for mat in (out / f"{name}_materials.txt").read_text().split():
        take_material(mat)

    models = sorted({l.split()[0] for l in (out / f"{name}_models.txt").read_text().splitlines() if l.strip()})
    for m in models:
        model = m.split("/", 1)[-1]
        try:
            lods = cod2import.read_xmodel(arch.read(f"xmodel/{model}"))
            part = lods[0]["name"]
            bones = cod2import.read_parts(arch.read(f"xmodelparts/{part}"))
            surfaces = cod2import.read_surfaces(arch.read(f"xmodelsurfs/{part}"), bones)
            cod2import.write_mesh(str(out / "models" / f"{model}.mesh"), surfaces, lods[0]["materials"], bones)
            for mat in set(lods[0]["materials"]):
                take_material(mat)
        except (FileNotFoundError, struct.error, ValueError, IndexError) as e:
            problems.append(f"model {model}: {e}")

    (out / f"{name}_material_maps.txt").write_text("\n".join(sorted(set(lines))) + "\n")
    made = 0
    for image in sorted(images):
        target = out / "images" / ("".join(ch if ch.isalnum() or ch in "_-~&." else "_" for ch in image) + ".png")
        if target.exists():
            made += 1
            continue
        try:
            w, h, rgba, _fmt = cod2import.decode_image(arch.read(f"images/{image}.iwi"))
            cod2import.write_png(str(target), w, h, rgba)
            made += 1
        except (FileNotFoundError, struct.error, ValueError) as e:
            problems.append(f"image {image}: {e}")
    print(f"{name}: {len(set(lines))} materials, {len(models)} models, {made} of {len(images)} images")
    if problems:
        print(f"{len(problems)} not converted:")
        for p in problems[:40]:
            print("  " + p)


if __name__ == "__main__":
    main()
