"""World at War audit, step 4: images and weapon files that nothing names.

usage: python -I xref_assets.py <dump folder>
"""
import collections
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
index = root / "index"
report = root / "report"

strings = set()
for p in index.glob("*.strings.txt"):
    if p.name.startswith("CoDWaW"):
        continue
    strings.update(s.lower() for s in p.read_text("latin-1").split("\n"))
text = []
for p in (root / "raw").rglob("*"):
    if p.is_file():
        text.append(p.read_text("latin-1"))
for p in (root / "ents").glob("*.ents"):
    text.append(p.read_text("latin-1"))
words = set(re.findall(r"[\w@-]+", "\n".join(text).lower()))

orphans = []
total = 0
for p in sorted(index.glob("*.iwd.txt")):
    for line in p.read_text("latin-1").split("\n"):
        if not line.lower().endswith(".iwi", 0, line.find("\t") if "\t" in line else None):
            continue
        name, size, date = line.split("\t")
        total += 1
        stem = name.rsplit("/", 1)[-1][:-4].lower()
        trimmed = re.sub(r"(_\d{3})$", "", stem)
        tilde = stem.lstrip("~").replace("-", "_")
        if stem in strings or trimmed in strings or stem in words or trimmed in words or tilde in strings:
            continue
        orphans.append((stem, int(size), date, p.name[:-8]))
orphans.sort()
with (report / "image_orphans.txt").open("w", encoding="latin-1") as f:
    for o in orphans:
        f.write("\t".join(map(str, o)) + "\n")
print(f"images {total}, not named by any zone or script {len(orphans)}, {sum(o[1] for o in orphans) / 1e6:.0f} MB")
print(collections.Counter(o[0].lstrip('~').split("_")[0] for o in orphans).most_common(50))

# weapon files
rows = []
for sub in ("sp", "mp"):
    names = set()
    for d in (root / "iwd").glob(f"*/weapons/{sub}"):
        names.update(p.name for p in d.iterdir() if p.is_file())
    for n in sorted(names):
        low = n.lower()
        in_zone = low in strings or f"{sub}/{low}" in strings or f"weapons/{sub}/{low}" in strings
        in_script = low in words or low + f"_{sub}" in words
        rows.append((sub, n, in_zone, in_script))
with (report / "weapon_files.txt").open("w", encoding="latin-1") as f:
    for r in rows:
        f.write("\t".join(map(str, r)) + "\n")
print("weapon files:", len(rows), "in no zone and no script:", [r[:2] for r in rows if not r[2] and not r[3]][:120])
