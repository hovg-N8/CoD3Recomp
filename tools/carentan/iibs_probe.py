import struct, sys
from pathlib import Path
from collections import Counter
sys.path.insert(0, sys.argv[1])
import fli, apkf


def word(d, a):
    return struct.unpack_from(">I", d, a)[0]


def walk(d, table_word):
    # fli.pointers with the table offset taken from table_word
    table = word(d, table_word)
    count, listed = struct.unpack_from(">II", d, table)
    bits = word(d, listed)
    starts = [word(d, listed + 4 * i) for i in range(1, count)]
    width = 31 - bits
    found = {}
    for place in starts:
        while True:
            w = word(d, place)
            if w & 0x80000000:
                v = w if w & 0x40000000 else w & 0x7FFFFFFF
                found[place] = (place + (v - (1 << 32) if v & 0x80000000 else v)) & 0xFFFFFFFF
                break
            v = w & ((1 << width) - 1)
            if v & (1 << (width - 1)):
                v -= 1 << width
            found[place] = place + v
            step = (w & 0x7FFFFFFF) >> width
            if not step:
                break
            place += step
    return found, table, bits


C = Counter()
for p in sorted(Path(sys.argv[2]).rglob("*.xenon.ib")):
    d = p.read_bytes()
    ptr, table, bits = walk(d, 0x10)
    C["table = last 16" if table == len(d) - 16 else "table elsewhere"] += 1
    ncat = word(d, 8)
    cats = []
    at = 0x14
    for k in range(ncat):
        C["ids in order" if word(d, at) == k else "id out of order"] += 1
        name = d[at + 4:at + 16].split(b"\0")[0].decode()
        slots, tab, nnames, lst = struct.unpack_from(">4I", d, at + 16)
        cats.append((name, slots, ptr.get(at + 20), nnames, ptr.get(at + 28)))
        at += 32
    for name, slots, tab, nnames, lst in cats:
        if not slots:
            continue
        if tab is None or (nnames and lst is None):
            print("no pointer:", p.parent.name, name, slots, nnames); continue
        pairs = [struct.unpack_from(">II", d, tab + 8 * i) for i in range(slots)]
        used = [(h, i) for h, i in pairs if (h, i) != (0, 0)]
        C["used == names" if len(used) == nnames else f"used {len(used)} names {nnames}"] += 1
        # names via the list: (ptr, 0) pairs
        names = []
        for i in range(nnames):
            sp = ptr.get(lst + 8 * i)
            names.append(d[sp:d.index(b"\0", sp)].decode("latin-1"))
        for h, i in used:
            n = names[i]
            C["hash ok" if h == apkf.name_hash(n) else "hash bad"] += 1
        # slot rule: home = hash % slots, linear probe
        for s, (h, i) in enumerate(pairs):
            if (h, i) == (0, 0):
                continue
            home = h % slots
            dist = (s - home) % slots
            ok = all(pairs[(home + j) % slots] != (0, 0) for j in range(dist))
            C["slot = h % n, probed" if ok else "slot rule bad"] += 1
        C["slots>names"] += slots > nnames
        C[f"{name}: slots {slots} vs names {nnames}"] += 0
    C["bits=" + str(bits)] += 1
print(sorted((k, v) for k, v in C.items() if v))

# slot rules
R = Counter()
for p in sorted(Path(sys.argv[2]).rglob("*.xenon.ib")):
    d = p.read_bytes()
    ptr, _, _ = walk(d, 0x10)
    for k in range(word(d, 8)):
        at = 0x14 + 32 * k
        slots, tab = word(d, at + 16), ptr.get(at + 20)
        if not slots or tab is None:
            continue
        pairs = [struct.unpack_from(">II", d, tab + 8 * i) for i in range(slots)]
        for s, (h, i) in enumerate(pairs):
            if (h, i) == (0, 0):
                continue
            for rule, home in (("mask", h & slots), ("mod", h % slots), ("mod+1", h % (slots + 1)), ("hi", (h >> 16) % slots)):
                for dirn in (1, -1):
                    dist = ((s - home) * dirn) % slots
                    if all(pairs[(home + dirn * j) % slots] != (0, 0) for j in range(dist)):
                        R[(rule, dirn)] += 1
            R["total"] += 1
print(R)

# implicit search tree (breadth-first, children 2s+1, 2s+2)
T = Counter()
def inorder(pairs, s, out):
    if s >= len(pairs):
        return
    inorder(pairs, 2 * s + 1, out)
    if pairs[s] != (0, 0):
        out.append(pairs[s])
    inorder(pairs, 2 * s + 2, out)
def build(keys, size, mid):
    out = [(0, 0)] * size
    def put(lo, hi, s):
        if lo >= hi or s >= size:
            return
        m = mid(lo, hi)
        out[s] = keys[m]
        put(lo, m, 2 * s + 1)
        put(m + 1, hi, 2 * s + 2)
    put(0, len(keys), 0)
    return out
rules = {"(lo+hi)//2": lambda lo, hi: (lo + hi) // 2, "(lo+hi-1)//2": lambda lo, hi: (lo + hi - 1) // 2}
for p in sorted(Path(sys.argv[2]).rglob("*.xenon.ib")):
    d = p.read_bytes()
    ptr, _, _ = walk(d, 0x10)
    for k in range(word(d, 8)):
        at = 0x14 + 32 * k
        slots, tab = word(d, at + 16), ptr.get(at + 20)
        if not slots or tab is None:
            continue
        pairs = [struct.unpack_from(">II", d, tab + 8 * i) for i in range(slots)]
        seq = []
        inorder(pairs, 0, seq)
        T["in-order sorted" if [h for h, _ in seq] == sorted(h for h, _ in seq) else "not sorted"] += 1
        keys = sorted(seq)
        for name, mid in rules.items():
            T[name + (" exact" if build(keys, slots, mid) == pairs else " differs")] += 1
        n = len(keys)
        T["size = least 2^k-1 >= n" if slots == (1 << n.bit_length()) - 1 else f"size other"] += 1
print(T)
