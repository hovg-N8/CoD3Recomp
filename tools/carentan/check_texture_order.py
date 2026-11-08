"""Check the shipped zone invariant: texture directory entries precede cells.
Usage: python -I check_texture_order.py <zone.cod> [...]
Reads local game data only; never writes archives.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "kapf"))
import kapf

for arg in sys.argv[1:]:
    archive = kapf.Archive(arg)
    entries = archive.files()
    textures = [i for i, e in enumerate(entries) if e[0].endswith(".xetex")]
    cells = [i for i, e in enumerate(entries) if e[0].endswith(".xecell")]
    assert textures and cells, f"{arg}: requires textures and cells"
    assert max(textures) < min(cells), (f"{arg}: texture entry {max(textures)} follows first cell {min(cells)}")
    print(f"PASS {Path(arg).name}: {len(textures)} textures before {len(cells)} cells")
