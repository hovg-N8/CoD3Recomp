"""World at War audit, step 3: files on disk that no zone or script names (orphans),
and localized strings nothing references.

A file counts as used if its base name (with or without extension) is a whole string
in any zone, or appears in a script. Streamed sounds store folder and name apart, so
base names are the only safe test; the result can only UNDER-report orphans if two
files share a name.

usage: python -I xref_audio.py <dump folder>
"""
import collections
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
index = root / "index"
report = root / "report"
report.mkdir(exist_ok=True)

strings = set()
for p in index.glob("*.strings.txt"):
    for line in p.read_text("latin-1").split("\n"):
        strings.add(line.lower())
        strings.add(line.lower().replace("\\", "/").rsplit("/", 1)[-1])

script_text = []
for base in ("raw", "iwd"):
    for p in (root / base).rglob("*"):
        if p.is_file():
            script_text.append(p.read_text("latin-1"))
for p in (root / "ents").glob("*.ents"):
    script_text.append(p.read_text("latin-1"))
blob = "\n".join(script_text).lower()
words = set(re.findall(r"[\w@.-]+", blob))

MEDIA = (".wav", ".mp3", ".ogg", ".flac")
rows = collections.defaultdict(lambda: [0, 0, 0, 0])   # folder -> files, bytes, orphans, orphan bytes
orphans = []
exts = collections.Counter()
for p in sorted(index.glob("*.iwd.txt")):
    for line in p.read_text("latin-1").split("\n"):
        if not line:
            continue
        name, size, date = line.split("\t")
        exts[Path(name).suffix.lower()] += 1
        if not name.lower().endswith(MEDIA):
            continue
        low = name.lower()
        base = low.rsplit("/", 1)[-1]
        stem = base.rsplit(".", 1)[0]
        used = base in strings or stem in strings or base in words or stem in words
        folder = "/".join(name.split("/")[:4])
        r = rows[folder]
        r[0] += 1
        r[1] += int(size)
        if not used:
            r[2] += 1
            r[3] += int(size)
            orphans.append(f"{name}\t{size}\t{date}\t{p.name[:-8]}")

(report / "audio_orphans.txt").write_text("\n".join(orphans), encoding="latin-1")
with (report / "audio_folders.txt").open("w", encoding="latin-1") as f:
    for k, r in sorted(rows.items(), key=lambda kv: -kv[1][2]):
        f.write(f"{k}\t{r[0]}\t{r[1]}\t{r[2]}\t{r[3]}\n")
print("iwd file types:", exts.most_common(12))
print("audio files:", sum(r[0] for r in rows.values()), "orphans:", len(orphans),
      f"orphan MB: {sum(r[3] for r in rows.values()) / 1e6:.0f}")

# localized text: keys that nothing references
refs = set(re.findall(r'&"([^"]+)"', "\n".join(script_text)))
refs = {r.upper() for r in refs}
at = set()
for p in index.glob("*.strings.txt"):
    for line in p.read_text("latin-1").split("\n"):
        if line.startswith("@"):
            at.add(line[1:].upper())
exe = (index / "CoDWaW.exe.strings.txt").read_text("latin-1") + (index / "CoDWaWmp.exe.strings.txt").read_text("latin-1")
exe_words = set(re.findall(r"[A-Z0-9_]{4,}", exe))
loc = {}
where = {}
for p in index.glob("*.localize.tsv"):
    for line in p.read_text("latin-1").split("\n"):
        if "\t" in line:
            k, v = line.split("\t", 1)
            loc[k] = v
            where[k] = p.name[:-len(".localize.tsv")]
unref = sorted(k for k in loc if k not in refs and k not in at and k not in exe_words and not k.startswith("SUBTITLE_"))
with (report / "localize_unreferenced.tsv").open("w", encoding="latin-1") as f:
    for k in unref:
        f.write(f"{k}\t{where[k]}\t{loc[k]}\n")
print("localized keys:", len(loc), "unreferenced (not SUBTITLE):", len(unref))
print(collections.Counter(k.split("_")[0] for k in unref).most_common(25))
missing = sorted(r for r in refs if r not in loc)
with (report / "localize_missing.txt").open("w", encoding="utf-8") as f:
    f.write("\n".join(missing))
print("referenced by scripts but text missing:", len(missing))
print(collections.Counter(k.split("_")[0] for k in missing).most_common(25))
