"""Every file of .cod archives written out, for studying a level's formats
(scene, collision, game database, meshes, textures, ...): a file's CPU part
as <name>, a resource's GPU part (pixels, vertices) as <name>.gpu. Names are
the pack's (28 characters at most), prefixed by block and type when typed.
The archives are the user's own game files; the output is game data: keep it
out of the repository.

usage: python -I extract_plain.py <out folder> <archive.cod> [more archives]
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402


def main():
    out = Path(sys.argv[1])
    for path in sys.argv[2:]:
        archive = kapf.Archive(path)
        folder = out / Path(path).stem
        folder.mkdir(parents=True, exist_ok=True)
        count = 0
        for b in range(len(archive.blocks())):
            d = archive.block(b)
            if d[:4] != b"APKF":
                print(f"{path} block {b}: not an APKF pack ({d[:4]!r})")
                continue
            p = apkf.read(d)
            for f in p.files:
                name = f["name"].replace(":", "_").replace("/", "_").replace("\\", "_")
                if not f["plain"]:
                    name = f"b{b:02}.{p.types[f['type']]['name']}.{f['entry']:03}.{name}"
                (folder / name).write_bytes(p.cpu(f))
                if f["gpu"] is not None:
                    (folder / (name + ".gpu")).write_bytes(p.gpu(f))
                count += 1
        print(f"{path}: {count} files to {folder}")


if __name__ == "__main__":
    main()
