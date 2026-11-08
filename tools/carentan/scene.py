"""Carentan's gameplay places in the Rouen scratch harness: Rouen's scene
records kept (scnb.py writes fixed-size scenes only), their values changed.

level: the level scene (mp_rouen.scnb in the pack's carentan.cod): every
  spawn class gets CoD2 Carentan's spawns of the same kind (deathmatch from
  mp_dm_spawn, team deathmatch from mp_tdm_spawn, every sided class from
  mp_ctf_spawn_allied / _axis, CTF primary = the side's spawns nearest its
  flag), the CTF flags and their stands go to CoD2's allied_flag / axis_flag,
  info_player_start and spawn_intermission to CoD2's, and Rouen's SpeedTree
  trees (script_origin with a speedtree property) 8000 units under the map.
  Origin at entity +0x08, angles (pitch, yaw, roll) at +0x50.
zone: the zone scenes (mp_rouen_zoneNN.scnb): instances, groups and effects
  off (scnb.Scene.with_instances_disabled): Rouen's props and fires.

usage: python -I scene.py level <source carentan.cod> <out .cod> <mp_carentan.d3dbsp>
       python -I scene.py zone <zone .cod> [more]        (rewritten in place)
"""
import math
import os
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "kapf"))
sys.path.insert(1, str(Path(__file__).resolve().parent))  # floor.py (python -I leaves the script's folder out)
import apkf  # noqa: E402
import kapf  # noqa: E402
import scnb  # noqa: E402


def cod2_entities(path):
    text = Path(path).read_bytes().decode("latin-1")
    out = []
    for e in re.findall(r"\{[^{}]*\}", text):
        kv = dict(re.findall(r'"([^"]*)" "([^"]*)"', e))
        if "classname" in kv:
            out.append(kv)
    if str(path).lower().endswith(".ents"):
        out = waw_entities(out)
    return out


def waw_entities(ents):
    """World at War's map entities (a .ents text, Seelow 2026-11-02) in CoD2's
    names: CTF spawns mp_ctf_spawn_allies -> _allied; the CTF flags (script
    models ctf_flag_allies / ctf_flag_axis) as CoD2's trigger_radius
    allied_flag / axis_flag; no info_player_start in WaW's MP maps: the
    allies' first team deathmatch start."""
    out = []
    for e in ents:
        e = dict(e)
        if e["classname"] == "mp_ctf_spawn_allies":
            e["classname"] = "mp_ctf_spawn_allied"
        if e["classname"] == "script_model" and e.get("targetname") in ("ctf_flag_allies", "ctf_flag_axis"):
            e = {"classname": "trigger_radius", "script_gameobjectname": "ctf", "origin": e["origin"],
                 "targetname": "allied_flag" if e["targetname"] == "ctf_flag_allies" else "axis_flag"}
        out.append(e)
    if not any(e["classname"] == "info_player_start" for e in out):
        start = next(e for e in out if e["classname"] == "mp_tdm_spawn_allies_start")
        out.append({"classname": "info_player_start", "origin": start["origin"],
                    "angles": start.get("angles", "0 0 0")})
    return out


def vec(s):
    return tuple(float(x) for x in s.split())


def edit_scenes(archive_path, out_path, edit):
    """Every .scnb of the archive through edit(name, Scene) -> Scene or None."""
    parts = kapf.decompose(kapf.Archive(archive_path))
    changed = 0
    for blk in parts["sections"][0]["blocks"]:
        data = blk["data"]
        if data[:4] != b"APKF":
            continue
        p = apkf.read(data)
        out = bytearray(data)
        for f in p.files:
            if not f["name"].lower().endswith(".scnb"):
                continue
            old = p.cpu(f)
            scene = edit(f["name"], scnb.Scene(old))
            if scene is None:
                continue
            new = scene.write()
            assert len(new) == len(old)
            out[f["at"]:f["at"] + len(new)] = new
            changed += 1
        blk["data"] = bytes(out)
    Path(out_path).write_bytes(kapf.compose(parts))
    return changed


def one_cell(path, cell=1):
    """The level's BSP tree (.btrb, 'BSPT') with every leaf in one cell, in place.
    Layout (Rouen's, 2026-11-01): header words: 0x08 node count, 0x10 plane count
    (planes nx ny nz d), 0x18 cell count, ...; nodes from 0x58, 16 bytes each:
    (-2, plane, front, back) inside, (cell, ...) a leaf; cell 0xFFFF = none
    (Rouen's solid and outside: 141 of 519 leaves; cell 1 205, 0 74, 7 45, 6 12).
    The camera's cell comes from this tree: where Rouen had a building or another
    cell, Carentan (all in cell 001) was not drawn and the view went white (the
    user's roofs at 1404 2494 375 and 1657 2184 154: 19k triangles drawn of 409k)."""
    parts = kapf.decompose(kapf.Archive(path))
    moved = 0
    for blk in parts["sections"][0]["blocks"]:
        data = blk["data"]
        if data[:4] != b"APKF":
            continue
        p = apkf.read(data)
        out = bytearray(data)
        for f in p.files:
            if not f["name"].lower().endswith(".btrb"):
                continue
            at = f["at"]
            assert out[at:at + 4] == b"BSPT"
            nodes = struct.unpack_from(">I", out, at + 8)[0]
            for k in range(nodes):
                o = at + 0x58 + 16 * k
                c = struct.unpack_from(">i", out, o)[0]
                if c != -2 and c != cell:
                    struct.pack_into(">i", out, o, cell)
                    moved += 1
        blk["data"] = bytes(out)
    Path(path).write_bytes(kapf.compose(parts))
    print(f"{path}: {moved} BSP leaves put in cell {cell}")


def drop(path, suffixes):
    """Files left out of an archive's directory (the loader walks the directory by
    type, whatever the name): Rouen's leftovers in the level archive. 2026-11-01:
    level.bgib ('_BGI', 12 areas): Rouen's grass, the user's "grass in the middle
    of the road" (Carentan's own grass is CoD2's placed brush_newgrass_* models)."""
    parts = kapf.decompose(kapf.Archive(path))
    gone = []
    for sec in parts["sections"]:
        keep = []
        for e in sec["entries"]:
            if e[0].lower().endswith(tuple(suffixes)):
                gone.append(e[0])
            else:
                keep.append(e)
        sec["entries"] = keep
    Path(path).write_bytes(kapf.compose(parts))
    print(f"{path}: left out of the directory: {', '.join(gone) or 'nothing'}")


def level(src, out, bsp):
    ents = cod2_entities(bsp)

    def spawns(cls):
        return [(vec(e["origin"]), vec(e.get("angles", "0 0 0"))) for e in ents if e["classname"] == cls]

    flags = {e["targetname"]: (vec(e["origin"]), vec(e.get("angles", "0 0 0")))
             for e in ents if e["classname"] == "trigger_radius" and e.get("script_gameobjectname") == "ctf"}
    allied_flag, axis_flag = flags["allied_flag"][0], flags["axis_flag"][0]
    side = {"allies": spawns("mp_ctf_spawn_allied"), "axis": spawns("mp_ctf_spawn_axis")}
    near = {s: sorted(side[s], key=lambda sp: math.dist(sp[0][:2], (allied_flag if s == "allies" else axis_flag)[:2]))
            for s in side}
    start = [(vec(e["origin"]), vec(e["angles"])) for e in ents if e["classname"] == "info_player_start"]
    inter = [(vec(e["origin"]), vec(e["angles"])) for e in ents if e["classname"] == "mp_global_intermission"]
    sources = {
        "spawn_deathmatch": spawns("mp_dm_spawn"),
        "spawn_teamdeathmatch": spawns("mp_tdm_spawn"),
        "info_player_start": start,
        "spawn_intermission": inter,
    }
    for s in ("allies", "axis"):
        for cls in (f"spawn_war_{s}", f"spawn_dom_{s}", f"spawn_hq_{s}_primary", f"spawn_single_ctf_{s}"):
            sources[cls] = side[s]
        sources[f"spawn_ctf_{s}_primary"] = near[s][:12]
        sources[f"spawn_ctf_{s}_secondary"] = near[s][12:] + near[s][:12]
    report = {}

    def edit(name, scene):
        if name.lower() != "mp_rouen.scnb":
            return None
        d = scene.data

        def put(e, origin, angles=None):
            struct.pack_into(">3f", d, e + 8, *origin)
            if angles is not None:
                struct.pack_into(">3f", d, e + 0x50, *angles)

        infos = [(e, scene.entity_info(e)) for e in scene.entities]
        by_class = {}
        for e, i in infos:
            by_class.setdefault(i["class"], []).append((e, i))
        for cls, src_list in sources.items():
            for k, (e, _i) in enumerate(by_class.get(cls, [])):
                origin, angles = src_list[k % len(src_list)]
                put(e, origin, angles)
            report[cls] = (len(by_class.get(cls, [])), len(src_list))
        # CTF: the flags by targetname; each stand moves with the flag it stood by
        ctf = [(e, i) for e, i in by_class.get("script_model", []) if dict(i["properties"]).get("gametypes") == "ctf"]
        flag_of = {}
        for e, i in ctf:
            tn = scene.string(scene.targets[e + 0x28]) if (e + 0x28) in scene.targets else None
            if tn in ("ctf_allies", "ctf_axis"):
                flag_of[tn] = (e, i["origin"])
        target = {"ctf_allies": allied_flag, "ctf_axis": axis_flag}
        moved = 0
        for e, i in ctf:
            nearest = min(flag_of, key=lambda tn: math.dist(i["origin"], flag_of[tn][1]))
            dz = i["origin"][2] - flag_of[nearest][1][2]
            t = target[nearest]
            put(e, (t[0], t[1], t[2] + dz))
            moved += 1
        report["ctf flags and stands"] = (moved, 2)
        trees = 0
        renamed = set()
        for e, i in by_class.get("script_origin", []):
            if "speedtree" in dict(i["properties"]):
                o = i["origin"]
                # 2026-11-02: 60000 down, past the view distance (r_zfar 9000): at
                # 8000 down the user still saw Rouen's trees under Seelow
                put(e, (o[0], o[1], -60000.0))
                trees += 1
                # 2026-11-01: sunk, they were still drawn (the user saw Rouen's trees
                # under the map): the property's key renamed, same length
                # (xpeedtree), so no tree is made for them at all
                start = scene.targets[e + 0xD8]
                for k in range(struct.unpack_from(">I", d, e + 0xD4)[0]):
                    at = scene.targets[start + 8 * k]
                    if scene.string(at) == "speedtree" and at not in renamed:
                        d[at:at + 1] = b"x"
                        renamed.add(at)
        report["speedtrees sunk, their key renamed"] = (trees, len(renamed))
        # Rouen's own scripted brush models (+0x14 'sbmodel...': origin-relative
        # brushes): out of the map with their origin
        sunk = 0
        for e, i in by_class.get("script_model", []):
            if (e + 0x14) in scene.targets and scene.string(scene.targets[e + 0x14]).startswith("sbmodel"):
                o = i["origin"]
                put(e, (o[0], o[1], -60000.0))
                sunk += 1
        report["Rouen scripted brush models sunk"] = (sunk, 0)
        objectives(scene, by_class, allied_flag, axis_flag, spawns("mp_dm_spawn"), put, report)
        return scene

    n = edit_scenes(src, out, edit)
    for k, v in report.items():
        print(f"  {k}: {v[0]} records <- {v[1]} Carentan places")
    print(f"{out}: {n} scene rewritten")


CGB = Path(r"D:\Call of Duty 3 (USA, Europe)\carentan-work\c2\carentan_all.cgb")
# another map's (Seelow 2026-11-02): SCENE_CGB its collision files ('|' between them),
# SCENE_GROUND "lo,hi" its open ground's height (Carentan's streets: -130..60)
GROUND = tuple(float(x) for x in os.environ.get("SCENE_GROUND", "-130,60").split(","))


def collision_files():
    import cgb
    paths = os.environ.get("SCENE_CGB")
    return [cgb.Cgb(Path(p).read_bytes()) for p in (paths.split("|") if paths else [CGB])]


def open_ground(collision, x, y, reach=None, step=48):
    """The open-ground point nearest (x, y): our collision's top surface there
    is the street level (-130..60: no roof, not a floor upstairs)."""
    import floor  # tools/carentan/floor.py (needs kapf on the path)
    reach = reach or int(os.environ.get("SCENE_REACH", "300"))   # (Seelow: its barns are wider)
    best = None
    r = 0
    while r <= reach and best is None:
        ring = [(x, y)] if r == 0 else [(x + r * math.cos(a), y + r * math.sin(a))
                                          for a in [k * 2 * math.pi / max(6, int(2 * math.pi * r / step)) for k in
                                                    range(max(6, int(2 * math.pi * r / step)))]]
        for px, py in ring:
            top = floor.floor_below(collision, px, py, 3000)
            ground = floor.floor_below(collision, px, py, 100)
            if top is not None and ground is not None and abs(top - ground) < 2 and GROUND[0] < ground < GROUND[1]:
                best = (px, py, ground)
                break
        r += step
    if best is None:
        raise ValueError(f"no open ground near {x:.0f} {y:.0f}")
    return best


def objectives(scene, by_class, allied_flag, axis_flag, dm_spawns, put, report):
    """War (3), domination (5) and HQ (10) objectives, and single-flag CTF:
    the objective records (script_model / hq_point) to open-ground points of
    Carentan; each trigger_multiple they name (record +0x24 -> the trigger's
    targetname at +0x28) moved with them: its brush is the source map's in
    world space at origin 0, so its origin becomes (new - old) of its object."""
    collision = collision_files()

    def text(e, off):
        return scene.string(scene.targets[e + off]) if (e + off) in scene.targets else None

    triggers = {text(e, 0x28): (e, i) for e, i in by_class.get("trigger_multiple", [])}
    a, b = allied_flag, axis_flag
    # objective points on CoD2's own spawn points (ground players and bots reach: test
    # carentan-war690's open-ground middle point was never reached, closest bot 90 away),
    # the one nearest each place along the allies -> axis line that is under open sky
    import floor
    walk = [s[0] for s in dm_spawns]
    used = []

    def on_spawn(x, y):
        for p in sorted(walk, key=lambda q: math.dist(q[:2], (x, y))):
            if any(math.dist(p[:2], u[:2]) < 300 for u in used):
                continue
            top = floor.floor_below(collision, p[0], p[1], 3000)
            ground = floor.floor_below(collision, p[0], p[1], p[2] + 40)
            if top is not None and ground is not None and abs(top - ground) < 2:
                used.append(p)
                return (p[0], p[1], ground)
        return open_ground(collision, x, y)
    line = [a] + [on_spawn(a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])) for t in (.25, .5, .75)] + [b]
    # Rouen's sides: allies' war spawns lie east, the axis' west: order the source objects allies -> axis
    sa = [i["origin"] for _e, i in by_class.get("spawn_war_allies", [])]
    sx = [i["origin"] for _e, i in by_class.get("spawn_war_axis", [])]

    def mean(pts):
        return [sum(p[k] for p in pts) / len(pts) for k in range(2)]

    # (taken before the spawns were moved? no: by_class infos hold the source values)
    ra, rx = mean(sa), mean(sx)

    def along(o):
        d = (rx[0] - ra[0], rx[1] - ra[1])
        return ((o[0] - ra[0]) * d[0] + (o[1] - ra[1]) * d[1]) / (d[0] ** 2 + d[1] ** 2)

    def move(objs, places, label):
        n = 0
        for (e, i), p in zip(objs, places):
            old = i["origin"]
            put(e, p)
            tn = text(e, 0x24)
            if tn in triggers:
                te, ti = triggers[tn]
                o = ti["origin"]
                put(te, (o[0] + p[0] - old[0], o[1] + p[1] - old[1], o[2] + p[2] - old[2]))
                n += 1
        report[label] = (len(objs), n)

    models = by_class.get("script_model", [])
    war = sorted([(e, i) for e, i in models if dict(i["properties"]).get("gametypes") == "war"], key=lambda ei: along(ei[1]["origin"]))
    move(war, [line[0], line[2], line[4]], "war flags (triggers moved)")
    dom = sorted([(e, i) for e, i in models if dict(i["properties"]).get("gametypes") == "dom"], key=lambda ei: along(ei[1]["origin"]))
    move(dom, line, "dom flags (triggers moved)")
    # HQ: ten points spread over the deathmatch spawns (farthest-point sampling from the middle)
    pts = [s[0] for s in dm_spawns]
    chosen = [min(pts, key=lambda p: math.dist(p[:2], line[2][:2]))]
    while len(chosen) < 10:
        chosen.append(max(pts, key=lambda p: min(math.dist(p[:2], c[:2]) for c in chosen)))
    hq = [open_ground(collision, p[0], p[1]) for p in chosen]
    move(by_class.get("hq_point", []), hq, "hq points (triggers moved)")
    # single-flag CTF: the flag (and its stand) in the middle, the bases (triggers with
    # their own origins, brushes relative) and their stands on the CTF flags' spots
    scf = [(e, i) for e, i in models if dict(i["properties"]).get("gametypes") == "scf"]
    centre = [(e, i) for e, i in scf if text(e, 0x28) == "scf_flag_point"]
    n = 0
    if centre:
        c0 = centre[0][1]["origin"]
        for e, i in scf:
            o = i["origin"]
            if math.dist(o[:2], c0[:2]) < 150:
                put(e, (line[2][0], line[2][1], line[2][2] + o[2] - c0[2] + (c0[2] - min(c0[2], o[2])) * 0))
                n += 1
    for name, spot in (("scf_base_allies", a), ("scf_base_axis", b)):
        if name in triggers:
            te, ti = triggers[name]
            o = ti["origin"]
            stands = [(e, i) for e, i in scf if text(e, 0x28) != "scf_flag_point" and math.dist(i["origin"][:2], o[:2]) < 150]
            ground = stands[0][1]["origin"][2] if stands else o[2]
            put(te, (spot[0], spot[1], spot[2] + o[2] - ground))
            for e, i in stands:
                put(e, spot)
            n += 1 + len(stands)
    report["single-flag CTF records"] = (n, 3)
    report["line allies->axis"] = ([tuple(round(v) for v in p) for p in line], 5)


def fires(scene, off, spots, fire="vfire_"):
    """Rouen's zone scene with everything off (off) but len(spots) effects:
    from its first fire record on, each named the scene's own vehicle fire
    (vfire_panzerIV: Rouen's burning Panzer), upright, at a spot. An effect
    record (96 bytes): rows of a 3x4 matrix (x y z w), origin at +48, its
    effect's name pointer at +64. 2026-11-03: WaW's five burning cars on
    Seelow (its createfx mp_fire_medium, every one at a junked car) showed
    their light but no flames (WaW's particle effects never play here)."""
    records = scene.effects
    named = [(r, scene.string(scene.targets[r + 64])) for r in records if r + 64 in scene.targets]
    first = next(i for i, (r, n) in enumerate(named) if n.lower().startswith(("fire", "vfire")))
    target = next(scene.targets[r + 64] for r, n in named if n.lower().startswith(fire))
    use = [r for r, _n in named[first:first + len(spots)]]
    if len(use) < len(spots):
        raise SystemExit(f"fires: {len(spots)} spots, {len(use)} effect records from the first fire")
    struct.pack_into(">I", off.data, 0x30, len(use))
    off.targets[0x34] = use[0]
    for r, spot in zip(use, spots):
        off.targets[r + 64] = target
        for row, axis in enumerate(((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))):
            struct.pack_into(">3f", off.data, r + 16 * row, *axis)
        struct.pack_into(">3f", off.data, r + 48, *spot)
    return off


def zone(paths):
    # SCENE_FIRES "x y z|x y z|...": fires (above) in the zone 1 scene
    spots = [tuple(float(v) for v in s.split()) for s in os.environ.get("SCENE_FIRES", "").split("|") if s.strip()]
    for path in paths:
        def edit(name, scene):
            if "_zone" not in name.lower():
                return None
            off = scene.with_instances_disabled()
            if spots and "_zone01" in name.lower():
                off = fires(scene, off, spots)
                print(f"{name}: {len(spots)} fires (vfire_panzerIV) at {spots}")
            return off
        n = edit_scenes(path, path, edit)
        print(f"{path}: {n} zone scene(s): instances, groups, effects off")


# WaW vehicle types -> CoD3 multiplayer vehicle kinds (Merville's game database,
# the vehicles' database pack) and their models (the campaign's vehicle archives)
VEHICLE_KINDS = {"panzer4_mp": ("mp_panzeriv", "v_panzer_iv"), "t34_mp": ("mp_shermanTank", "v_shermanm4a3")}


def vehicles(path, ents_path):
    """The map's own drivable vehicles (2026-11-04, Seelow): WaW's script_vehicle
    places as script_vehicle records in the level scene (mp_rouen.scnb), in place.
    A map's vehicles drive only when they are its scene's own (0.5.731), and the
    scene cannot grow (scnb.py): Rouen's sunk SpeedTree script_origins (the
    vehicle record's pointer slots exactly: +4 class, +0x14 model, +0xD8
    properties, +0xE0 their hash index) are made vehicles, as Merville's are:
    class id +0 0x36, class "script_vehicle", model, origin, angles, two
    properties (gametypes, vehicletype) and the index rows (name hash, value).
    The strings go where those records' own lighting values were (26 characters,
    used by nothing else). Run after level. The kinds come from the vehicles'
    database pack the host loads beside the map (mp_merv_vehicles.cod), the
    models from a campaign vehicle archive (sp\\saint_lo\\vehicles.cod)."""
    spots = [(VEHICLE_KINDS[e["vehicletype"]], vec(e["origin"]), vec(e.get("angles", "0 0 0")))
             for e in cod2_entities(ents_path)
             if e["classname"] == "script_vehicle" and e.get("vehicletype") in VEHICLE_KINDS]
    report = {}

    def edit(name, scene):
        if name.lower() != "mp_rouen.scnb":
            return None
        d = scene.data
        word = lambda at: struct.unpack_from(">I", d, at)[0]
        trees = [e for e in scene.entities if scene.string(scene.targets[e + 4]) == "script_origin"
                 and any(k in ("xpeedtree", "speedtree") for k, _v in scene.properties(e))]
        if len(trees) < len(spots):
            raise SystemExit(f"{len(spots)} vehicles, {len(trees)} spare records")
        used = trees[:len(spots)]
        # every string's users, to take only strings nothing else reads
        users = {}
        for e in scene.entities:
            for o in (4, 0x14, 0x24, 0x28):
                if e + o in scene.targets:
                    users.setdefault(scene.targets[e + o], set()).add(e)
            for table, rows in ((0xD8, word(e + 0xD4)), (0xE0, word(e + 0xDC))):
                if rows:
                    start = scene.targets[e + table]
                    for k in range(rows):
                        for o in ((0, 4) if table == 0xD8 else (4,)):
                            if start + 8 * k + o in scene.targets:
                                users.setdefault(scene.targets[start + 8 * k + o], set()).add(e)
        free = []
        for e in used:
            for k, v in scene.properties(e):
                if k == "lightingPrecalc":
                    start = scene.targets[e + 0xD8]
                    at = next(scene.targets[start + 8 * j + 4] for j in range(word(e + 0xD4))
                              if scene.string(scene.targets[start + 8 * j]) == "lightingPrecalc")
                    if users.get(at) == {e}:
                        free.append(at)
        strings = {}

        def text(s):
            if s in strings:
                return strings[s]
            for at in sorted(free, key=lambda a: len(scene.string(a))):
                room = len(scene.string(at))
                if room >= len(s):
                    free.remove(at)
                    d[at:at + room + 1] = s.encode("latin-1") + b"\0" * (room + 1 - len(s))
                    strings[s] = at
                    return at
            raise SystemExit(f"no room for the string {s!r}")
        keys = {scene.string(scene.targets[scene.targets[e + 0xD8] + 8 * k]): scene.targets[scene.targets[e + 0xD8] + 8 * k]
                for e in scene.entities for k in range(word(e + 0xD4))}
        gametypes_key = keys.get("gametypes") or text("gametypes")
        vehicletype_key = text("vehicletype")
        every = text("dm dom ctf tdm war scf hq")
        cls = text("script_vehicle")
        for e, ((kind, model), origin, angles) in zip(used, spots):
            struct.pack_into(">I", d, e, 0x36)
            scene.targets[e + 4] = cls
            scene.targets[e + 0x14] = text(model)
            struct.pack_into(">3f", d, e + 8, *origin)
            struct.pack_into(">3f", d, e + 0x50, *angles)
            pairs, index = scene.targets[e + 0xD8], scene.targets[e + 0xE0]
            if word(e + 0xD4) < 2 or word(e + 0xDC) < 2:
                raise SystemExit(f"record {e:X}: fewer than 2 property rows")
            value = text(kind)
            for k, (key, key_text, val) in enumerate(((gametypes_key, "gametypes", every), (vehicletype_key, "vehicletype", value))):
                scene.targets[pairs + 8 * k] = key
                scene.targets[pairs + 8 * k + 4] = val
                struct.pack_into(">I", d, index + 8 * k, apkf.name_hash(key_text))
                scene.targets[index + 8 * k + 4] = val
            struct.pack_into(">I", d, e + 0xD4, 2)
            struct.pack_into(">I", d, e + 0xDC, 2)
            report.setdefault(kind, []).append(tuple(round(v) for v in origin))
        return scene

    n = edit_scenes(path, path, edit)
    for k, v in report.items():
        print(f"  {k}: {len(v)} at {v}")
    print(f"{path}: {n} scene(s): {sum(len(v) for v in report.values())} vehicles made from sunk tree records")


if __name__ == "__main__":
    if sys.argv[1] == "vehicles":
        vehicles(sys.argv[2], sys.argv[3])
    elif sys.argv[1] == "level":
        level(*sys.argv[2:5])
    elif sys.argv[1] == "zone":
        zone(sys.argv[2:])
    elif sys.argv[1] == "onecell":
        one_cell(sys.argv[2])
    elif sys.argv[1] == "drop":
        drop(sys.argv[2], sys.argv[3:])
    else:
        print(__doc__)
