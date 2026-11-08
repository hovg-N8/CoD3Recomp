"""World at War's animations (XAnimParts) read from the user's own fastfiles,
for CoD3's port (the flamethrower's own arm animations, 2026-11-01). Reads,
never runs, them.

The asset (0x58 bytes, little-endian; Call of Duty 4's layout, checked on
viewmodel_flamethrower_idle/fire/pullout/putaway in common_mp.ff):
  +0x00 name, u16 dataByteCount, dataShortCount, dataIntCount,
  randomDataByteCount, randomDataIntCount, numframes, u8 loop, u8 delta,
  +0x12 u8 boneCount[10] by part type: rotations 0 none, 1 half quat, 2 full
  quat, 3 half quat (no keys), 4 full quat (no keys); translations 5 small
  (byte keys), 6 full (short keys), 7 constant, 8 none; 9 all bones.
  +0x1C u8 notifyCount, assetType, isDefault; +0x20 u32 randomDataShortCount,
  u32 indexCount; +0x28 f32 framerate, f32 frequency; +0x30 pointers: names,
  dataByte, dataShort, dataInt, randomDataShort, randomDataByte,
  randomDataInt, indices, notify, deltaPart (-1: its data follows, 0: none).
The counts tie out exactly (idle: 75 constant quats x 4 = 300 shorts; 63
constant translations x 3 = 189 ints; 78 bone index bytes), so the streams
are read in order as below; read() checks every stream ends where it should.
"""
import math
import struct
import sys

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import wawimport  # noqa: E402

PART_NAMES = ["no quat", "half quat", "full quat", "half quat const", "full quat const",
              "small trans", "trans", "trans const", "no trans", "all"]


def find(zone, name):
    key = name.encode() + b"\0"
    at = 0
    while True:
        at = zone.find(key, at)
        if at < 0:
            raise ValueError(f"xanim {name} not found")
        s = at - 0x58
        if s >= 0 and struct.unpack_from("<i", zone, s)[0] == -1 and zone[s + 0x12 + 9] > 0:
            return s
        at += 1


def header(zone, s):
    h = zone[s:s + 0x58]
    c = dict(zip(["dataByte", "dataShort", "dataInt", "randByte", "randInt", "frames"], struct.unpack_from("<6H", h, 4)))
    c["loop"], c["delta"] = h[16], h[17]
    c["bones"] = list(h[18:28])
    c["notify"] = h[28]
    c["randShort"], c["index"] = struct.unpack_from("<2I", h, 0x20)
    c["framerate"], c["frequency"] = struct.unpack_from("<2f", h, 0x28)
    c["ptrs"] = struct.unpack_from("<10i", h, 0x30)
    return c


class Stream:
    def __init__(self, data, fmt, count, label):
        self.size = struct.calcsize("<" + fmt)
        self.vals = list(struct.unpack_from("<%d%s" % (count, fmt), data, 0)) if count else []
        self.at, self.label = 0, label

    def take(self, n=1):
        if self.at + n > len(self.vals):
            raise ValueError(f"{self.label}: ran past its end ({self.at + n} > {len(self.vals)})")
        v = self.vals[self.at:self.at + n]
        self.at += n
        return v if n > 1 else v[0]

    def done(self):
        return self.at == len(self.vals)


def layout(zone, s, c, order):
    """The streams' places: after the asset, its name, then the arrays in
    'order' (names of the pointers written -1)."""
    at = zone.index(b"\0", s + 0x58) + 1
    sizes = {"names": 2 * c["bones"][9], "notify": 8 * c["notify"], "dataByte": c["dataByte"],
             "dataShort": 2 * c["dataShort"], "dataInt": 4 * c["dataInt"], "randShort": 2 * c["randShort"],
             "randByte": c["randByte"], "randInt": 4 * c["randInt"],
             "index": (c["index"] * (1 if c["frames"] < 256 else 2))}
    present = dict(zip(["names", "dataByte", "dataShort", "dataInt", "randShort", "randByte", "randInt", "index", "notify", "delta"], c["ptrs"]))
    places = {}
    for key in order:
        if present.get(key, 0) != -1:
            continue
        places[key] = at
        at += sizes[key]
    return places, at


def read(zone, name, order=("names", "notify", "dataByte", "dataShort", "dataInt", "randShort", "randByte", "randInt", "index"), strings=None):
    s = find(zone, name)
    c = header(zone, s)
    places, end = layout(zone, s, c, order)
    get = lambda key: zone[places[key]:] if key in places else b""
    db = Stream(get("dataByte"), "B", c["dataByte"], "dataByte")
    ds = Stream(get("dataShort"), "h", c["dataShort"], "dataShort")
    di = Stream(get("dataInt"), "f", c["dataInt"], "dataInt")
    rs = Stream(get("randShort"), "h", c["randShort"], "randomDataShort")
    rb = Stream(get("randByte"), "B", c["randByte"], "randomDataByte")
    names = list(struct.unpack_from("<%dH" % c["bones"][9], zone, places["names"])) if "names" in places else []
    if strings:
        names = [strings[n] if n < len(strings) else f"#{n}" for n in names]
    frames = c["frames"] + 1
    b = c["bones"]
    rot, trans = {}, {}
    bone = 0

    def keys(count_word):
        n = count_word + 1        # (the stored word is keys - 1: checked by the totals)
        return n, [db.take() for _ in range(n)]

    # rotations, by part type in order; the names are in the same order
    bone += b[0]
    for k in range(b[1]):         # half quat, keyed
        n, idx = keys(ds.take())
        rot[bone] = [(f, (0.0, 0.0, rs.take() / 32767.0, rs.take() / 32767.0)) for f in idx]
        bone += 1
    for k in range(b[2]):         # full quat, keyed
        n, idx = keys(ds.take())
        rot[bone] = [(f, tuple(v / 32767.0 for v in rs.take(4))) for f in idx]
        bone += 1
    for k in range(b[3]):         # half quat, constant
        rot[bone] = [(0, (0.0, 0.0, ds.take() / 32767.0, ds.take() / 32767.0))]
        bone += 1
    for k in range(b[4]):         # full quat, constant
        rot[bone] = [(0, tuple(v / 32767.0 for v in ds.take(4)))]
        bone += 1
    # translations: each names its bone (a byte, into the names)
    for k in range(b[5]):         # small: byte keys between mins and mins + size
        tb = db.take()
        n, idx = keys(ds.take())
        mins, size = di.take(3), di.take(3)
        trans[tb] = [(f, tuple(mins[j] + size[j] * rb.take() / 255.0 for j in range(3))) for f in idx]
    for k in range(b[6]):         # full: short keys
        tb = db.take()
        n, idx = keys(ds.take())
        mins, size = di.take(3), di.take(3)
        trans[tb] = [(f, tuple(mins[j] + size[j] * (rs.take() & 0xFFFF) / 65535.0 for j in range(3))) for f in idx]
    for k in range(b[7]):         # constant
        tb = db.take()
        trans[tb] = [(0, tuple(di.take(3)))]
    for k in range(b[8]):         # none (named only)
        db.take()
    left = {st.label: len(st.vals) - st.at for st in (db, ds, di, rs, rb) if not st.done()}
    return {"name": name, "header": c, "frames": frames, "names": names, "rot": rot, "trans": trans, "left": left, "places": places, "end": end}


def main():
    zone = open(sys.argv[1], "rb").read()
    strings = wawimport.script_strings(zone)
    for name in sys.argv[2:]:
        a = read(zone, name, strings=strings)
        c = a["header"]
        bad = [n for n, ks in a["rot"].items() for f, q in ks if abs(math.sqrt(sum(v * v for v in q)) - 1.0) > 0.02]
        print(f"{name}: {a['frames']} frames at {c['framerate']} fps, bones {c['bones']}, streams left over {a['left'] or 'none'}, "
              f"quats not unit: {len(bad)} of {sum(len(k) for k in a['rot'].values())}")
        for n in (0, 1, 2, 3, 4):
            if n < len(a["names"]):
                print("   ", n, a["names"][n], a["rot"].get(n, [])[:2], a["trans"].get(n, [])[:2])


if __name__ == "__main__":
    main()
