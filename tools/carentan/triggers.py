"""The objective triggers (War, Domination, HQ, single-flag CTF) of a map in the
Rouen harness (Carentan, Seelow) moved INTO their brush models (2026-11-07).

Found 2026-11-07 (0.5.851, the user: War "the flag shows but it wouldn't actually
start capturing", Carentan and Seelow): the game made no capture in 20 minutes of
War on Carentan (the flag's holder field 53377998 stayed 0 with bots on it), and
Seelow's install had dropped the .dcgb from the level's directory altogether.

mp_rouen.dcgb (the level's "dynamic collision": every brush model *N, big-endian):
  0x00 u32 model count (61 in Rouen), then one 112-byte record a model from 0x10:
    +0x00 u16 boxes, u16 brushes, u32 volumes (Rouen's triggers: 1, 0, 1: one box),
    +0x30 f32 min xyz, +0x40 max xyz, +0x50 middle xyz (each 12 bytes in a 16),
    +0x60 f32 radius, +0x64 radius squared, +0x68 u32 the model's number (*N; a
    name hash for the props' models). The words between are the exporter's stale
    memory (bits of its .map text: not read).
  then the volumes (as in a .cgb: u32 flags 0x08000001, u32 0x80, f32 middle xyz,
    half sizes xyz, radius; 48 bytes with filler), in WORLD space for Rouen's own
    brush models (*2 = war_flag_trigger1 at Rouen's 1904..2168 512..720).
Rouen's trigger_multiple records stand at origin 0 (world-space brushes). The scene
step (scene.py objectives) gave each moved trigger the origin (new - old) of its
objective, as a Quake trigger would take it; CoD3 places these brushes as stored.
Here each such trigger's record and volume are moved by its origin and the origin
put back to 0, Rouen's own arrangement.

--seelow (a level whose .dcgb was left out of the directory: Rouen's doors and
breakables made invisible walls there, 2026-11-02): the file is put back in the
directory and every other brush model's bounds (and its volumes that match them)
sunk 60000 under the map, so only the objective triggers are left.

usage: python -I triggers.py <level .cod> [--seelow] [--dry]
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "kapf"))
import apkf  # noqa: E402
import kapf  # noqa: E402
import scnb  # noqa: E402

OBJECTIVE = {"war", "dom", "hq", "scf"}
SINK = -60000.0
REC, VOL = 112, 48


def records(d):
    count = struct.unpack_from(">I", d, 0)[0]
    out = {}
    for k in range(count):
        at = 0x10 + REC * k
        mn = struct.unpack_from(">3f", d, at + 0x30)
        mx = struct.unpack_from(">3f", d, at + 0x40)
        mid = struct.unpack_from(">3f", d, at + 0x50)
        r, r2 = struct.unpack_from(">2f", d, at + 0x60)
        n = struct.unpack_from(">I", d, at + 0x68)[0]
        heads = struct.unpack_from(">HHI", d, at)
        ok = all(abs(mid[j] - (mn[j] + mx[j]) / 2) < 0.01 for j in range(3)) and abs(r * r - r2) <= 0.01 * r2 + 0.01
        out.setdefault(n, []).append({"at": at, "min": mn, "max": mx, "mid": mid, "heads": heads, "ok": ok})
    return count, out


def volumes(d, start):
    """Every 4-aligned place past the records that reads as a volume (flags 0x08000001)."""
    out = []
    for at in range(start, len(d) - 36, 4):
        if d[at:at + 4] != b"\x08\x00\x00\x01":
            continue
        mid = struct.unpack_from(">3f", d, at + 8)
        half = struct.unpack_from(">3f", d, at + 20)
        out.append((at, mid, half))
    return out


def shift3(d, at, delta):
    v = struct.unpack_from(">3f", d, at)
    struct.pack_into(">3f", d, at, *(v[j] + delta[j] for j in range(3)))


def main():
    path = Path(sys.argv[1])
    seelow, dry = "--seelow" in sys.argv, "--dry" in sys.argv
    parts = kapf.decompose(kapf.Archive(path))
    where = {}
    for b, blk in enumerate(parts["sections"][0]["blocks"]):
        if blk["data"][:4] != b"APKF":
            continue
        p = apkf.read(blk["data"])
        for f in p.files:
            low = f["name"].lower()
            if low == "mp_rouen.scnb" or low == "mp_rouen.dcgb":
                where[low] = (b, p, f)
    if "mp_rouen.scnb" not in where or "mp_rouen.dcgb" not in where:
        sys.exit(f"{path}: no mp_rouen.scnb / mp_rouen.dcgb in its blocks")
    sb, sp, sf = where["mp_rouen.scnb"]
    db, dp, df = where["mp_rouen.dcgb"]
    scene = scnb.Scene(sp.cpu(sf))
    dc = bytearray(dp.cpu(df))
    count, recs = records(dc)
    vols = volumes(dc, 0x10 + REC * count)

    def text(e, off):
        return scene.string(scene.targets[e + off]) if (e + off) in scene.targets else None

    # the objective each trigger belongs to (its +0x24 names the trigger's +0x28): a
    # box that does not reach that objective's height after the move is put on its
    # ground (Rouen's dom_flag_trigger3 box stands at 8..120 under a flag at 315:
    # moved as it was, it lay 300 under Carentan's street)
    owner = {}
    for e in scene.entities:
        tn = text(e, 0x24)   # (War's spawns name their flag's trigger too: the objective models only)
        if tn and tn not in owner and scene.entity_info(e)["class"] in ("script_model", "hq_point"):
            owner[tn] = struct.unpack_from(">3f", scene.data, e + 8)
    keep, moved, already = set(), 0, 0
    for e in scene.entities:
        info = scene.entity_info(e)
        if info["class"] != "trigger_multiple":
            continue
        model, kinds = text(e, 0x14) or "", dict(info["properties"]).get("gametypes", "")
        if not model.startswith("*") or not (set(kinds.split()) & OBJECTIVE):
            continue
        n = int(model[1:])
        keep.add(n)
        o = struct.unpack_from(">3f", scene.data, e + 8)
        rs = recs.get(n, [])
        at = owner.get(text(e, 0x28))
        if o == (0.0, 0.0, 0.0) and (at is None or not rs or (abs(rs[0]["mid"][0] - at[0]) < 1 and abs(rs[0]["mid"][1] - at[1]) < 1)):
            already += 1
            continue
        if len(rs) != 1 or not rs[0]["ok"] or rs[0]["heads"] != (1, 0, 1):
            sys.exit(f"{model} ({text(e, 0x28)}): not one plain box record ({rs}): stopped, nothing written")
        r = rs[0]
        half = tuple((r["max"][j] - r["min"][j]) / 2 for j in range(3))
        match = [v for v in vols if all(abs(v[1][j] - r["mid"][j]) < 0.01 and abs(v[2][j] - half[j]) < 0.01 for j in range(3))]
        if len(match) != 1:
            sys.exit(f"{model}: {len(match)} volumes match its box: stopped, nothing written")
        o = list(o)
        lift = ""
        if at is not None:
            # centred on its objective (Seelow's war_flag_trigger2 box stood 54 south of its
            # flag, as Rouen's did: the bots on the north side of the pole were not in it)
            o[0] = at[0] - r["mid"][0]
            o[1] = at[1] - r["mid"][1]
            lo, hi = r["min"][2] + o[2], r["max"][2] + o[2]
            if not lo - 8 <= at[2] <= hi:
                o[2] += (at[2] - 16) - lo
                lift = f", height put on its objective's ({at[2]:.1f})"
        for off in (0x30, 0x40, 0x50):
            shift3(dc, r["at"] + off, o)
        shift3(dc, match[0][0] + 8, o)
        struct.pack_into(">3f", scene.data, e + 8, 0.0, 0.0, 0.0)
        new = struct.unpack_from(">3f", dc, r["at"] + 0x50)
        print(f"  {model:4s} {text(e, 0x28):20s} {kinds:4s}: box middle {r['mid'][0]:8.1f} {r['mid'][1]:8.1f} {r['mid'][2]:7.1f}"
              f" -> {new[0]:8.1f} {new[1]:8.1f} {new[2]:7.1f} (origin -> 0{lift})")
        moved += 1
    sunk = 0
    if seelow:
        for n, rs in recs.items():
            if n in keep or n > 4096:   # the props' own models (name hashes) stay: local, unplaced
                continue
            for r in rs:
                if r["min"][2] < SINK / 2:
                    continue
                half = tuple((r["max"][j] - r["min"][j]) / 2 for j in range(3))
                for v in vols:
                    if all(abs(v[1][j] - r["mid"][j]) < 0.01 and abs(v[2][j] - half[j]) < 0.01 for j in range(3)):
                        shift3(dc, v[0] + 8, (0.0, 0.0, SINK))
                for off in (0x30, 0x40, 0x50):
                    shift3(dc, r["at"] + off, (0.0, 0.0, SINK))
                sunk += 1
    print(f"{path.name}: {moved} objective triggers moved into their brushes, {already} already at origin 0"
          f"{f', {sunk} other brush models sunk' if seelow else ''}")
    if dry:
        print("(dry: nothing written)")
        return
    new_scene = scene.write()
    assert len(new_scene) == sf["size"] and len(dc) == df["size"]
    for b, f, data in ((sb, sf, new_scene), (db, df, bytes(dc))):
        blk = parts["sections"][0]["blocks"][b]
        out = bytearray(blk["data"])
        out[f["at"]:f["at"] + len(data)] = data
        blk["data"] = bytes(out)
    entries = parts["sections"][0]["entries"]
    if not any(e[0].lower() == "mp_rouen.dcgb" for e in entries):
        if not seelow:
            sys.exit("the .dcgb is not in the directory (use --seelow): stopped, nothing written")
        index = (df["type"] << 16) | df["entry"]
        entries.append(("mp_rouen.dcgb", "c:\\cod\\obj\\assets\\image\\mp_rouen\\mp_rouen.dcgb", df["size"], db, index))
        print(f"  mp_rouen.dcgb put back in the directory: block {db}, index {index:#x}")
    path.write_bytes(kapf.compose(parts))
    print(f"{path}: written")


if __name__ == "__main__":
    main()
