"""World at War audit, step 5: the honest orphan test. Every name-shaped token in every
decompressed zone, with no assumption about what sits before it (a name that follows a
struct has no zero byte in front: xref_assets' string list missed those and called
nearly every image unused). A stray leading byte that happens to be a word character
glues onto the name, so each token also counts without its first one or two characters.

usage: python -I xref_tokens.py <WaW folder> <dump folder>
"""
import collections
import re
import sys
import zlib
from pathlib import Path

game, root = Path(sys.argv[1]), Path(sys.argv[2])
index, report = root / "index", root / "report"
TOKEN = re.compile(rb"[\w~#&@$.+-]{4,}")

tokens = set()
for p in sorted((game / "zone").rglob("*.ff")):
    zone = zlib.decompress(p.read_bytes()[12:])
    tokens.update(TOKEN.findall(zone.lower()))
    del zone
print("raw tokens", len(tokens), flush=True)
expanded = set()
for t in tokens:
    s = t.decode("latin-1")
    expanded.update((s, s[1:], s[2:]))
    if "/" not in s and s.endswith(".wav"):
        expanded.add(s[:-4])
del tokens

text = []
for base in ("raw", "iwd"):
    for p in (root / base).rglob("*"):
        if p.is_file():
            text.append(p.read_text("latin-1"))
for p in (root / "ents").glob("*.ents"):
    text.append(p.read_text("latin-1"))
expanded.update(re.findall(r"[\w~#&@$.+-]{4,}", "\n".join(text).lower()))
print("tokens with scripts", len(expanded), flush=True)


def used(stem):
    return stem in expanded or stem + ".wav" in expanded or stem + ".iwi" in expanded


images, audio = [], []
n_img = n_aud = 0
for p in sorted(index.glob("*.iwd.txt")):
    for line in p.read_text("latin-1").split("\n"):
        if "\t" not in line:
            continue
        name, size, date = line.split("\t")
        low = name.lower()
        stem = low.rsplit("/", 1)[-1].rsplit(".", 1)[0]
        if low.endswith(".iwi"):
            n_img += 1
            base = re.sub(r"_\d{3}$", "", stem)
            if not used(stem) and not used(base):
                images.append((stem, int(size), date, p.name[:-8]))
        elif low.endswith(".wav"):
            n_aud += 1
            if not used(stem):
                audio.append((name, int(size), date, p.name[:-8]))
for fname, rows in (("image_orphans.txt", images), ("audio_orphans.txt", audio)):
    with (report / fname).open("w", encoding="latin-1") as f:
        for r in sorted(rows):
            f.write("\t".join(map(str, r)) + "\n")
print(f"images {n_img} orphans {len(images)} ({sum(r[1] for r in images) / 1e6:.0f} MB)")
print(f"audio {n_aud} orphans {len(audio)} ({sum(r[1] for r in audio) / 1e6:.0f} MB)")
print(collections.Counter(r[0].lstrip('~').split("_")[0] for r in images).most_common(40))

rows = []
for sub in ("sp", "mp"):
    names = set()
    for d in (root / "iwd").glob(f"*/weapons/{sub}"):
        names.update(p.name for p in d.iterdir() if p.is_file())
    for n in sorted(names):
        low = n.lower()
        hit = low in expanded or f"{sub}/{low}" in expanded or f"weapons/{sub}/{low}" in expanded or (low + "_" + sub) in expanded
        rows.append((sub, n, hit))
with (report / "weapon_files.txt").open("w", encoding="latin-1") as f:
    for r in rows:
        f.write("\t".join(map(str, r)) + "\n")
print("weapon files", len(rows), "unused:", [f"{a}/{b}" for a, b, c in rows if not c])
