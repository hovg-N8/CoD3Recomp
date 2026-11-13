"""World at War audit, step 1: dump the text and the asset names of the user's own
install into a folder OUTSIDE the source tree. Reads the install, never writes to it.

Zone facts used (checked on oki2, patch, ui, nazi_zombie_factory):
  raw file: s32 name (-1), u32 size, s32 data (-1), name\\0, size bytes of plain text, \\0
  localize: s32 value (-1), s32 name (-1), value\\0, NAME\\0
  sound:    ....wav\\0RIFF
  xmodel:   0xE4 bytes that start with a name pointer -1 and sane counts, then its name
  map entities: the text blocks of { "key" "value" } lines

usage: python -I dump.py <WaW folder> <out folder>
"""
import collections
import json
import re
import struct
import sys
import time
import zipfile
import zlib
from pathlib import Path

RAW = re.compile(rb"\xff\xff\xff\xff(....)\xff\xff\xff\xff([\w./\\ -]{3,160}\.[a-zA-Z_0-9]{1,10})\0", re.S)
LOC = re.compile(rb"\xff\xff\xff\xff\xff\xff\xff\xff([^\0]{0,4000})\0([A-Z0-9_]{2,120})\0", re.S)
WAV = re.compile(rb"([\w./\\ ,'()-]{3,200}\.wav)\0RIFF(....)", re.S)
STR = re.compile(rb"(?<=\0)[\x20-\x7e]{5,300}(?=\0)")
ENT = b'{\n"'
TEXT_EXT = {".gsc", ".csc", ".cfg", ".csv", ".menu", ".txt", ".arena", ".vision", ".shock", ".str", ".atr",
            ".def", ".ents", ".gdt", ".script", ".sun", ".rmb", ".info", ".cgp", ".lst", ".inc", ".h", ""}


def safe(name):
    name = name.replace("\\", "/")
    parts = [re.sub(r"[^\w.+-]", "_", p) for p in name.split("/") if p not in ("", ".", "..")]
    return "/".join(parts) or "_"


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def asset_types(zone):
    count = struct.unpack_from("<I", zone, 0x24)[0]
    at = 0x34 + 4 * count
    for i in range(count):
        if struct.unpack_from("<i", zone, 0x34 + 4 * i)[0] != 0:
            at = zone.index(b"\0", at) + 1
    assets = struct.unpack_from("<I", zone, 0x2C)[0]
    types = collections.Counter()
    for i in range(assets):
        types[struct.unpack_from("<I", zone, at + 8 * i)[0]] += 1
    return count, assets, dict(sorted(types.items()))


def script_strings(zone):
    count = struct.unpack_from("<I", zone, 0x24)[0]
    at = 0x34 + 4 * count
    out = []
    for i in range(count):
        if struct.unpack_from("<i", zone, 0x34 + 4 * i)[0] != 0:
            end = zone.index(b"\0", at)
            out.append(zone[at:end].decode("latin-1"))
            at = end + 1
    return out


def xmodels(zone):
    out = set()
    for m in re.finditer(rb"(?<=[\s\S]{228})[a-zA-Z_][\w@ -]{3,100}\0", zone) if False else ():
        pass
    # candidates: every string after a 0xE4 header (start -1, sane bone/surface counts)
    for m in re.finditer(rb"\xff\xff\xff\xff[\x01-\x80].[\x01-\x20]", zone, re.S):
        name_at = m.start() + 0xE4
        end = zone.find(b"\0", name_at, name_at + 120)
        if end <= name_at + 3:
            continue
        name = zone[name_at:end]
        if re.fullmatch(rb"[a-zA-Z_][\w@-]+", name):
            out.add(name.decode("latin-1"))
    return out


def map_entities(zone):
    """The entity text: a NUL-bounded run holding many '{\\n"' blocks."""
    out = []
    at = 0
    while True:
        at = zone.find(ENT, at)
        if at < 0:
            return out
        start = zone.rfind(b"\0", max(0, at - 8), at) + 1 or at
        end = zone.find(b"\0", at)
        block = zone[at:end]
        if block.count(ENT) >= 5 and b'"classname"' in block:
            out.append(block)
        at = end + 1


def dump_ff(path, out, summary):
    stem = path.stem
    zone = zlib.decompress(path.read_bytes()[12:])
    scripts, assets, types = asset_types(zone)
    raws = bad = 0
    names = []
    for m in RAW.finditer(zone):
        size = struct.unpack("<I", m.group(1))[0]
        name = m.group(2).decode("latin-1")
        if size > len(zone) - m.end() or zone[m.end() + size:m.end() + size + 1] != b"\0":
            bad += 1
            continue
        write(out / "raw" / stem / safe(name), zone[m.end():m.end() + size])
        names.append(f"{name}\t{size}")
        raws += 1
    loc = sorted({(m.group(2).decode("latin-1"), m.group(1).decode("latin-1").replace("\n", "\\n")) for m in LOC.finditer(zone)})
    wavs = sorted({f"{m.group(1).decode('latin-1')}\t{struct.unpack('<I', m.group(2))[0]}" for m in WAV.finditer(zone)})
    models = sorted(xmodels(zone))
    ents = map_entities(zone)
    strings = sorted({m.group(0).decode("latin-1") for m in STR.finditer(zone)})
    (out / "index").mkdir(parents=True, exist_ok=True)
    (out / "index" / f"{stem}.raw.txt").write_text("\n".join(names), encoding="latin-1")
    (out / "index" / f"{stem}.localize.tsv").write_text("\n".join(f"{a}\t{b}" for a, b in loc), encoding="latin-1")
    (out / "index" / f"{stem}.sounds.txt").write_text("\n".join(wavs), encoding="latin-1")
    (out / "index" / f"{stem}.xmodels.txt").write_text("\n".join(models), encoding="latin-1")
    (out / "index" / f"{stem}.scriptstrings.txt").write_text("\n".join(script_strings(zone)), encoding="latin-1")
    (out / "index" / f"{stem}.strings.txt").write_text("\n".join(strings), encoding="latin-1")
    for i, e in enumerate(ents):
        write(out / "ents" / f"{stem}.{i}.ents", e)
    summary[stem] = {"zone_bytes": len(zone), "script_strings": scripts, "assets": assets, "types": types,
                     "raw": raws, "raw_bad": bad, "localize": len(loc), "sounds": len(wavs), "xmodels": len(models),
                     "ents": len(ents), "strings": len(strings)}
    print(f"{stem}: zone {len(zone)/1e6:.0f}MB raw {raws} (bad {bad}) loc {len(loc)} snd {len(wavs)} xm {len(models)} ents {len(ents)}", flush=True)


def dump_iwd(path, out, summary):
    with zipfile.ZipFile(path) as z:
        rows, kept = [], 0
        for i in z.infolist():
            if i.is_dir():
                continue
            rows.append(f"{i.filename}\t{i.file_size}\t{i.date_time[0]:04}-{i.date_time[1]:02}-{i.date_time[2]:02}")
            ext = Path(i.filename).suffix.lower()
            if ext in TEXT_EXT and i.file_size < 30_000_000:
                write(out / "iwd" / path.stem / safe(i.filename), z.read(i))
                kept += 1
    (out / "index").mkdir(parents=True, exist_ok=True)
    (out / "index" / f"{path.stem}.iwd.txt").write_text("\n".join(rows), encoding="latin-1")
    summary[path.name] = {"files": len(rows), "text_kept": kept}
    print(f"{path.name}: {len(rows)} files, {kept} text", flush=True)


def dump_exe(path, out):
    data = path.read_bytes()
    found = sorted({m.group(0).decode("latin-1") for m in re.finditer(rb"[\x20-\x7e]{5,400}", data)})
    (out / "index").mkdir(parents=True, exist_ok=True)
    (out / "index" / f"{path.stem}.exe.strings.txt").write_text("\n".join(found), encoding="latin-1")
    print(f"{path.name}: {len(data):,} bytes, {len(found)} strings", flush=True)


def main():
    root, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    summary = {}
    t = time.time()
    for p in sorted((root / "zone").rglob("*.ff")):
        try:
            dump_ff(p, out, summary)
        except Exception as e:
            print(f"FAILED {p.name}: {e!r}", flush=True)
    for p in sorted((root / "main").glob("*.iwd")):
        try:
            dump_iwd(p, out, summary)
        except Exception as e:
            print(f"FAILED {p.name}: {e!r}", flush=True)
    for p in root.glob("*.exe"):
        dump_exe(p, out)
    (out / "summary.json").write_text(json.dumps(summary, indent=1))
    print(f"DONE in {time.time() - t:.0f}s", flush=True)


if __name__ == "__main__":
    main()
