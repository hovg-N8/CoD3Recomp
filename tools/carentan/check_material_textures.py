"""Validate converted materials use bundled Carentan normal/spec maps.
Usage: python -I check_material_textures.py <cells.pkl>
"""
import pickle
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "kapf"))
import apkf
import cellmat
conv = pickle.loads(Path(sys.argv[1]).read_bytes())
textures = set(conv["textures"])
counts = {"normalmap_texture": 0, "specularmap_texture": 0}
for data in conv["blocks"]:
    p = apkf.read(data)
    fixups, imports = p.tail_lists()
    ptrs = {p.image + 4 * i for i in fixups}
    imps = {p.image + 4 * i: (t, n) for i, t, n in imports}
    for f in p.files:
        if p.types[f["type"]]["name"] != "MAT":
            continue
        for name, kind, value in cellmat.parse(p, f, ptrs, imps):
            if kind != cellmat.TEXTURE:
                continue
            assert value[0] == "import", (f["name"], name, value)
            assert value[1] in textures, (f["name"], name, "texture not bundled", value[1])
            if name in counts:
                counts[name] += 1
assert all(counts.values()), counts
print("PASS: all material texture imports bundled;", counts)
