"""The vehicles' pack's scene (0.5.731): Merville's level scene, kept in the
pack, made into a scene of vehicles alone. A map's vehicles drive only when
they are its scene's own (0.5.71-73): the game sets them up from their
script_vehicle records (model, kind, game types) and the host moves them to
the places vehicles.txt keeps; one the host made from nothing crashed
Seelow's start (test seelow20tanks) and was never drivable. Loaded beside a
map, an archive's scene is spawned with the map's (test rouenA_merv: Merville's
10 vehicles on Rouen). So:

  - every record that is not a vehicle is silenced: its class name's first
    letter made 'x' (as Rouen's trees' speedtree key, scene.py), so no spawn
    point, flag, trigger or script origin of Merville's comes along;
  - records are made vehicles (a 228-byte record copied from Merville's own
    record of that kind: its class, model, property list and link pointers,
    its bytes) until there are COUNTS of each kind;
  - every vehicle record shows in every game type (Merville's tanks were only
    in the team ones): its game types pointed at the BMW's list;
  - every vehicle parked 60000 under the map: the host moves those a map
    wants into place, the rest stay out of sight.
  - the scene's model instances, groups and effects off (Merville's props).

usage: python -I vehicle_scene.py <pack.cod> [<vehicles.txt> <map>]     (rewritten in place)
  with the placements: as many of each kind as that map has placed, each at
  its place (a Kangaroo is made from a WC51), the others left out, none parked
  (2026-11-03: 22 parked in one spot hung Seelow's load)
"""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402
import scnb  # noqa: E402

COUNTS = {"mp_panzeriv": 6, "mp_shermantank": 6, "mp_wc51": 3, "mp_horch": 3, "mp_motorcycle_bmw": 4}
RECORD = 228
POINTERS = (0x04, 0x14, 0xD8, 0xE0)
PARKED = -60000.0


def edit(scene, report):
    d = scene.data
    ents = list(scene.entities)
    info = {e: scene.entity_info(e) for e in ents}
    vehicles = {}
    for e in ents:
        if info[e]["class"] == "script_vehicle":
            vehicles.setdefault(dict(info[e]["properties"])["vehicletype"].lower(), []).append(e)
    bmw = vehicles["mp_motorcycle_bmw"][0]

    def pairs(e):
        count = struct.unpack_from(">I", d, e + 0xD4)[0]
        at = scene.targets[e + 0xD8]
        return [(at + 8 * k, at + 8 * k + 4) for k in range(count)]
    every = next(v for k, v in pairs(bmw) if scene.string(scene.targets[k]) == "gametypes")
    every_target = scene.targets[every]
    # spare records: those with a vehicle's pointer slots exactly (the
    # scene's pointers are chains: a slot can be pointed elsewhere, not made
    # or taken away): 191 of Merville's script origins
    def slots(e):
        return tuple(o for o in range(0, RECORD, 4) if (e + o) in scene.targets)
    spare = [e for e in ents if info[e]["class"] != "script_vehicle" and slots(e) == POINTERS]
    made = 0
    counts = dict(COUNTS)
    if PLACES:   # (2026-11-03: as many of each kind as are placed, none more)
        counts = {k: 0 for k in vehicles}
        for kind, _p in PLACES:
            counts[kind] = counts.get(kind, 0) + 1
    for kind, want in counts.items():
        have = vehicles.get(kind, [])
        if not have:
            continue
        if os.environ.get("VEHICLE_NO_COPIES"):   # (test: Merville's own records only)
            want = min(want, len(have))
        template = have[0]
        assert slots(template) == POINTERS
        for _ in range(want - len(have)):
            e = spare.pop(0)
            d[e:e + RECORD] = d[template:template + RECORD]   # (pointer words: rewritten from targets)
            for o in POINTERS:
                scene.targets[e + o] = scene.targets[template + o]
            have.append(e)
            info[e] = {"class": "script_vehicle"}
            made += 1
        vehicles[kind] = have
    report["records made vehicles"] = made
    # every vehicle record: all game types, parked
    lists = set()
    for kind, have in vehicles.items():
        for e in have:
            struct.pack_into(">3f", d, e + 8, 0.0, 0.0, PARKED)
            at = scene.targets[e + 0xD8]
            if at in lists:
                continue
            lists.add(at)
            for k, v in pairs(e):
                if scene.string(scene.targets[k]) == "gametypes":
                    scene.targets[v] = every_target
    report["vehicles"] = {k: len(v) for k, v in vehicles.items()}
    # the rest silenced: their class names' first letter 'x'
    renamed = set()
    for e in ents:
        if info[e]["class"] == "script_vehicle" or any(e in v for v in vehicles.values()):
            continue
        at = scene.targets.get(e + 4)
        if at is not None and at not in renamed and scene.string(at) != "script_vehicle":
            d[at:at + 1] = b"x"
            renamed.add(at)
    report["class names silenced"] = len(renamed)
    if PLACES:
        # each placed vehicle at its own place; Merville's others silenced (their
        # class pointer to a silenced name): none parked together (test
        # seelowveh2, 2026-11-03: 22 parked in one spot 60000 down hung the load
        # in the vehicles' spawn, sub_82561448 under sub_8247B780)
        quiet = next(iter(renamed))
        left = {k: list(v) for k, v in vehicles.items()}
        placed = 0
        for kind, (x, y, z) in PLACES:
            if left.get(kind):
                e = left[kind].pop(0)
                struct.pack_into(">3f", d, e + 8, x, y, z)
                placed += 1
        silenced = 0
        for kind, rest in left.items():
            for e in rest:
                scene.targets[e + 4] = quiet
                silenced += 1
        report["vehicles placed"] = placed
        report["vehicles left out"] = silenced
    return scene.with_instances_disabled()


PLACES = []   # (kind, (x, y, z)) from vehicles.txt for one map (a Kangaroo as the WC51 it is made from)


def read_places(file, map_name):
    out = []
    for line in Path(file).read_text(encoding="utf-8").splitlines():
        w = line.split("#")[0].split()
        if len(w) >= 5 and w[0].lower() == map_name.lower():
            kind = w[1].lower()
            out.append(("mp_wc51" if kind == "mp_kangaroo" else kind, (float(w[2]), float(w[3]), float(w[4]))))
    return out


def main():
    path = sys.argv[1]
    if len(sys.argv) > 3:   # <pack.cod> <vehicles.txt> <map>: the map's placements in the scene
        PLACES[:] = read_places(sys.argv[2], sys.argv[3])
    parts = kapf.decompose(kapf.Archive(path))
    report = {}
    changed = 0
    for blk in parts["sections"][0]["blocks"]:
        if blk["data"][:4] != b"APKF":
            continue
        p = apkf.read(blk["data"])
        for f in p.files:
            if f["name"].lower().endswith(".scnb"):
                scene = edit(scnb.Scene(bytearray(p.cpu(f))), report)
                new = scene.write()
                assert len(new) == f["size"], "the scene changed size"
                blk["data"] = apkf.rebuild(p, {f["name"]: bytes(new)})
                changed += 1
    if not changed:
        raise SystemExit("no scene in the pack")
    Path(path).write_bytes(kapf.compose(parts))
    for k, v in report.items():
        print(f"  {k}: {v}")
    print(f"{path}: the vehicles' scene written")


if __name__ == "__main__":
    main()
