"""World at War audit, step 2: level names anywhere in the dump that are not a shipped
level, and developer comments that talk about cut or unfinished things.

usage: python -I xref_levels.py <dump folder>
"""
import collections
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
shipped = {p.name[:-len(".raw.txt")] for p in (root / "index").glob("*.raw.txt")}

# level-shaped tokens: campaign codes, mp_*, nazi_zombie_*
LEVEL = re.compile(r"\b(mak\d?|(?:pel|see|ber|oki|sni|pby|rhi|fin|jap|ger|rus|usa|pac|eur)\d[a-z]?|mp_[a-z0-9_]{3,30}|nazi_zombie_[a-z0-9_]{3,30}|zombie_[a-z]+_map)\b")
CUT = re.compile(r"(\bcut\b|removed|no longer|unused|not used|never used|old way|deprecated|TEMP\b|HACK|TODO|FIXME|placeholder|not finished|unfinished|easter|secret|hidden|DLC|ship\b|shipping|E3\b|demo\b|milestone|designer|producer|activision)", re.I)


def texts():
    for base in ("raw", "iwd"):
        for p in (root / base).rglob("*"):
            if p.is_file():
                yield p, p.read_text("latin-1")
    for p in (root / "ents").glob("*.ents"):
        yield p, p.read_text("latin-1")
    for p in (root / "index").glob("*.txt"):
        yield p, p.read_text("latin-1")
    for p in (root / "index").glob("*.tsv"):
        yield p, p.read_text("latin-1")


levels = collections.defaultdict(collections.Counter)
comments = collections.Counter()
comment_where = {}
total_comments = 0
for p, t in texts():
    rel = p.relative_to(root).as_posix()
    source = rel.split("/")[1] if "/" in rel else rel
    for m in LEVEL.finditer(t):
        levels[m.group(1)][source] += 1
    if p.suffix in (".gsc", ".csc", ".cfg", ".menu", ".script"):
        for m in re.finditer(r"//([^\n]*)", t):
            c = m.group(1).strip()
            if not c:
                continue
            total_comments += 1
            if CUT.search(c):
                comments[c] += 1
                comment_where.setdefault(c, rel)

not_shipped = {k: v for k, v in levels.items() if k not in shipped and k + "_load" not in shipped}
out = root / "report"
out.mkdir(exist_ok=True)
with (out / "levels_not_shipped.txt").open("w", encoding="latin-1") as f:
    for k, v in sorted(not_shipped.items(), key=lambda kv: -sum(kv[1].values())):
        f.write(f"{k}\t{sum(v.values())}\t{', '.join(f'{a}:{b}' for a, b in v.most_common(8))}\n")
with (out / "comments_keywords.txt").open("w", encoding="latin-1") as f:
    for c, n in sorted(comments.items(), key=lambda kv: kv[0].lower()):
        f.write(f"{n}\t{comment_where[c]}\t{c}\n")
print("level-shaped names not shipped:", len(not_shipped))
print("comments total:", total_comments, "matching keywords:", len(comments))
