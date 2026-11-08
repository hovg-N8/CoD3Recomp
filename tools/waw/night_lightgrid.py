"""Seelow Night's light grid (2026-11-02): the zones' .ng.lgridb are Rouen's,
lit by Rouen's afternoon sun, and they light what moves (the player's arms
and gun, the soldiers, dropped things): in Seelow Night those stayed lit as
by day (tests night1-4). The header (the same in Rouen's three zones), from
+0x28: f32 sun direction xyz (unit: 0.521 0.159 -0.839), 0, f32 sun colour
rgb (0.503 0.488 0.442), f32 xyz (1516 -936 222: the grid's place), 1.0,
f32 rgb (0.381 0.234 0.078). Both colours are scaled to moonlight here; the
grid's own samples are left as they are.

--fog: the level archive's fog effects instead (Rouen's global fog
gfog_mist_7c7c50z_wide_night and fog_mist_lt_7c7c1c_night, its tan haze at
night: test night5): each of their colour multipliers (1 1 1 1, not
pointers) scaled, alpha kept.

--sun: the worldspawn's sun instead, taken whole from another level (Longyard,
CoD3's night map). The tan haze is Rouen's suncolor 0.99 0.93 0.81: the world
program's pixel constant c22 (tests fog1/fog2, COD3_DRAWLOG_PSDUMP). The
worldspawn record holds, before its class name, 13 floats from ambient on
(ambient, diffusefraction, 0.4, suncolor rgb, sundiffusecolor rgb,
sundirection xy, 0, sunlight), and its keys again as text: both are copied,
the text within each value's own room.

--fogrec: THE TAN HAZE (tests draws1, fogpass_*): the frame's fog pass (pixel
program af5356c5ec765583, after the world) blends one colour, its c2, over the
picture by the depth. Rouen's c2 is 0.425 0.375 0.325; Longyard's 0.06 0.06 0.1.
The level archive's FLI section holds it in records of 9 floats: near, far, r, g, b, 0,
strength (c0.z), 1.0, far clip (Rouen 200 6000 .425 .375 .325 0 0.1 1 10000,
four of them; Longyard 400 10800 .06 .06 .1 0 0.195 1 16000, three). The first
8 are copied from the other level into each; the clip (draw distance) is kept.

usage: python -I night_lightgrid.py "r,g,b" <zone.cod> [more]   (rewritten in place)
       python -I night_lightgrid.py --fog "r,g,b" <level.cod>
       python -I night_lightgrid.py --sun <from_level.cod> <level.cod>
       python -I night_lightgrid.py --fogrec <from_level.cod> <level.cod>
       python -I night_lightgrid.py --lights "r,g,b" <zone.cod> [more]
       python -I night_lightgrid.py --sink-lamps <zone.cod> [more]   (day and night)
"""
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "kapf"))
import apkf  # noqa: E402
import kapf  # noqa: E402

SUN, SECOND = 0x38, 0x54   # byte offsets of the two colours


def fog(k, path):
    parts = kapf.decompose(kapf.Archive(path))
    ones = bytes.fromhex("3F800000" * 4)
    for blk in parts["sections"][0]["blocks"]:
        if blk["data"][:4] != b"APKF":
            continue
        p = apkf.read(blk["data"])
        fixups, _imports = p.tail_lists()
        d = bytearray(blk["data"])
        for f in p.files:
            name = f["name"].lower()
            if p.types[f["type"]]["name"] != "AEPS" or not name.startswith(("gfog_mist", "fog_mist")):
                continue
            rel = f["at"] - p.image
            pointers = {4 * i - rel for i in fixups if 0 <= 4 * i - rel < f["size"]}
            cpu = bytes(p.cpu(f))
            for o in range(0, f["size"] - 15, 4):
                if cpu[o:o + 16] == ones and not pointers & {o, o + 4, o + 8, o + 12}:
                    struct.pack_into(">4f", d, f["at"] + o, k[0], k[1], k[2], 1.0)
                    print(f"{path}: {f['name']} colour at +{o:X} -> {k}")
        blk["data"] = bytes(d)
    Path(path).write_bytes(kapf.compose(parts))


SUN_KEYS = (b"ambient", b"diffusefraction", b"suncolor", b"sundiffusecolor", b"sundirection", b"sunlight")
SUN_RECORD = 13 * 4   # the floats before the record's 12 bytes and "worldspawn"


def worldspawn(data):
    """The binary sun floats' offset and {key: (value offset, room)} of the text."""
    at = data.find(b"worldspawn\0")
    if at < 0 or data.find(b"worldspawn\0", at + 1) >= 0:
        return None
    floats = at - 12 - SUN_RECORD
    keys = {}
    for k in SUN_KEYS:
        m = re.compile(re.escape(k) + rb"\0+([0-9.\- ]+)(\0+)").search(data, at)
        if m is None:
            return None
        keys[k] = (m.start(1), len(m.group(1)) + len(m.group(2)) - 1)   # one zero kept
    return floats, keys


def take_sun(source, path):
    src = Path(source).read_bytes()
    found = worldspawn(src)
    if found is None:
        raise SystemExit(f"{source}: no single worldspawn")
    floats = src[found[0]:found[0] + SUN_RECORD]
    values = {k: re.match(rb"[0-9.\- ]+", src[o:]).group() for k, (o, _room) in found[1].items()}
    parts = kapf.decompose(kapf.Archive(path))
    done = 0
    for blk in parts["sections"][0]["blocks"]:
        here = worldspawn(blk["data"])
        if here is None:
            continue
        d = bytearray(blk["data"])
        old = struct.unpack_from(">13f", d, here[0])
        if not (0 < old[0] < 1 and 0 < old[12] < 4):
            raise SystemExit(f"{path}: not the layout expected ({old})")
        d[here[0]:here[0] + SUN_RECORD] = floats
        for k, (o, room) in here[1].items():
            if len(values[k]) > room:
                raise SystemExit(f"{path}: {k.decode()} {values[k].decode()} does not fit in {room}")
            d[o:o + room] = values[k].ljust(room, b"\0")
            print(f"{path}: {k.decode()} -> {values[k].decode()}")
        blk["data"] = bytes(d)
        done += 1
    if done != 1:
        raise SystemExit(f"{path}: {done} worldspawn records, not 1")
    Path(path).write_bytes(kapf.compose(parts))


def fog_records(data):
    """Offsets of the fog records (near, far, rgb, 0, strength, 1.0, clip)."""
    out = []
    for m in re.finditer(rb"\x3f\x80\x00\x00", data):
        at = m.start() - 28
        if at < 0:
            continue
        near, far, r, g, b, zero, strength, one, clip = struct.unpack_from(">9f", data, at)
        if zero == 0 and one == 1.0 and 10 <= near < far <= 100000 and 0 <= min(r, g, b) \
                and 0 < max(r, g, b) <= 1 and 0 < strength < 1 and far < clip <= 200000:
            out.append(at)
    return out


def take_fog(source, path):
    src = Path(source).read_bytes()
    found = {src[at:at + 32] for at in fog_records(src)}
    if len(found) != 1:
        raise SystemExit(f"{source}: {len(found)} different fog records, not 1")
    fog8 = found.pop()
    parts = kapf.decompose(kapf.Archive(path))
    done = 0
    for blk in (b for sec in parts["sections"] for b in sec["blocks"]):   # the FLI section holds them
        at_list = fog_records(blk["data"])
        if not at_list:
            continue
        d = bytearray(blk["data"])
        for at in at_list:
            old = struct.unpack_from(">8f", d, at)
            d[at:at + 32] = fog8
            print(f"{path}: fog {tuple(round(x, 3) for x in old)} -> {tuple(round(x, 3) for x in struct.unpack('>8f', fog8))}")
            done += 1
        blk["data"] = bytes(d)
    if done == 0:
        raise SystemExit(f"{path}: no fog record")
    Path(path).write_bytes(kapf.compose(parts))


def lights(k, paths):
    """The light grid's light list: header +0x04 its count, from +0x28 records of
    7 floats (position or direction xyz, w 1 point / 0 direction, colour rgb), +0x10
    the list's end: the level's 150 lights (record 0 the sun, then Rouen's fill
    lights and lamps, the same list in every zone). What lights the player's arms
    and gun is built from these (the arm program's c18-c26, 9 rgb terms: Seelow
    Night ~0.20, Longyard ~0.03; test arms_*). Records 0 and 1 are the main mode's
    'sun' and 'second colour'; records 2 on are scaled here."""
    for path in paths:
        parts = kapf.decompose(kapf.Archive(path))
        done = 0
        for blk in parts["sections"][0]["blocks"]:
            if blk["data"][:4] != b"APKF":
                continue
            p = apkf.read(blk["data"])
            for f in p.files:
                if not f["name"].lower().endswith(".lgridb"):
                    continue
                d = bytearray(p.cpu(f))
                n = struct.unpack_from(">I", d, 4)[0]
                if not (0 < n < 1000 and struct.unpack_from(">I", d, 0x10)[0] == 0x28 + 28 * n):
                    raise SystemExit(f"{path} {f['name']}: not the layout expected ({n} lights)")
                for i in range(2, n):
                    at = 0x28 + 28 * i + 16
                    rgb = struct.unpack_from(">3f", d, at)
                    if not all(0 <= c <= 8 for c in rgb):
                        raise SystemExit(f"{path} {f['name']}: light {i} colour {rgb}")
                    struct.pack_into(">3f", d, at, *(c * m for c, m in zip(rgb, k)))
                blk["data"] = apkf.rebuild(p, {f["name"]: bytes(d)})
                print(f"{path}: {f['name']} lights 2-{n - 1} scaled by {k}")
                done += 1
        if done:
            Path(path).write_bytes(kapf.compose(parts))
        else:
            print(f"{path}: no light grid")


def sink_lamps(paths):
    """Rouen's lamps out of the light list (day and night): the level's lights
    each get a corona, the glow sprite (global.cod's dynamiclight) drawn in front
    of everything and faded by the depth in front of it (cpu_depth.cpp). Rouen's
    9 warm point lights (in pairs 8 units apart: its street lamps) float over
    Seelow as orange dots in the sky (the user's screenshots; tour skydot1 view 6
    lines up with lights 148 and 85/86). Each warm one (point, red over 1.5x its
    blue) is moved 60000 under the map, where the ground hides its glow."""
    for path in paths:
        parts = kapf.decompose(kapf.Archive(path))
        done = 0
        for blk in parts["sections"][0]["blocks"]:
            if blk["data"][:4] != b"APKF":
                continue
            p = apkf.read(blk["data"])
            for f in p.files:
                if not f["name"].lower().endswith(".lgridb"):
                    continue
                d = bytearray(p.cpu(f))
                n = struct.unpack_from(">I", d, 4)[0]
                if not (0 < n < 1000 and struct.unpack_from(">I", d, 0x10)[0] == 0x28 + 28 * n):
                    raise SystemExit(f"{path} {f['name']}: not the layout expected ({n} lights)")
                sunk = []
                for i in range(1, n):
                    at = 0x28 + 28 * i
                    x, y, z, w, r, g, b = struct.unpack_from(">7f", d, at)
                    if w == 1.0 and r > 0.02 and r > 1.5 * b:
                        struct.pack_into(">f", d, at + 8, -60000.0)
                        sunk.append((i, round(x), round(y), round(z)))
                blk["data"] = apkf.rebuild(p, {f["name"]: bytes(d)})
                print(f"{path}: {f['name']} {len(sunk)} warm lights sunk: {sunk}")
                done += 1
        if done:
            Path(path).write_bytes(kapf.compose(parts))
        else:
            print(f"{path}: no light grid")


def main():
    if sys.argv[1] == "--sink-lamps":
        sink_lamps(sys.argv[2:])
        return
    if sys.argv[1] == "--fog":
        fog([float(x) for x in sys.argv[2].split(",")], sys.argv[3])
        return
    if sys.argv[1] == "--sun":
        take_sun(sys.argv[2], sys.argv[3])
        return
    if sys.argv[1] == "--fogrec":
        take_fog(sys.argv[2], sys.argv[3])
        return
    if sys.argv[1] == "--lights":
        lights([float(x) for x in sys.argv[2].split(",")], sys.argv[3:])
        return
    k = [float(x) for x in sys.argv[1].split(",")]
    for path in sys.argv[2:]:
        parts = kapf.decompose(kapf.Archive(path))
        done = 0
        for blk in parts["sections"][0]["blocks"]:
            if blk["data"][:4] != b"APKF":
                continue
            p = apkf.read(blk["data"])
            for f in p.files:
                if not f["name"].lower().endswith(".lgridb"):
                    continue
                d = bytearray(p.cpu(f))
                sun = struct.unpack_from(">3f", d, SUN)
                second = struct.unpack_from(">3f", d, SECOND)
                if not (0 < max(sun) <= 4 and 0 <= max(second) <= 4):
                    raise SystemExit(f"{path} {f['name']}: not the layout expected ({sun}, {second})")
                struct.pack_into(">3f", d, SUN, *(c * m for c, m in zip(sun, k)))
                struct.pack_into(">3f", d, SECOND, *(c * m for c, m in zip(second, k)))
                blk["data"] = apkf.rebuild(p, {f["name"]: bytes(d)})
                print(f"{path}: {f['name']} sun {tuple(round(c, 3) for c in sun)} and {tuple(round(c, 3) for c in second)} scaled by {k}")
                done += 1
        if done:
            Path(path).write_bytes(kapf.compose(parts))
        else:
            print(f"{path}: no light grid")


if __name__ == "__main__":
    main()
