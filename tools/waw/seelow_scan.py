"""World at War's Seelow (mp_seelow.ff) for CoD3's port: finding its world
geometry in the decompressed zone without a full zone reader (2026-11-02).

World at War's world vertices (GfxWorldVertex, 44 bytes): f32 xyz, f32
binormal sign (exactly +1 or -1), u32 colour, f32 uv[2], f32 lightmap uv[2],
u32 packed normal, u32 packed tangent. A run of records whose +12 is +-1.0,
whose xyz lie in a map's range and whose lightmap uv lie in 0..1 is the
world's vertex array; this finds the longest such runs.

usage: python -I seelow_scan.py <mp_seelow.zone> [min run]
"""
import struct
import sys

ONE, MINUS = struct.pack("<f", 1.0), struct.pack("<f", -1.0)


def good(z, at):
    if at + 44 > len(z):
        return False
    sign = z[at + 12:at + 16]
    if sign != ONE and sign != MINUS:
        return False
    x, y, w = struct.unpack_from("<3f", z, at)
    if not all(abs(v) < 40000 for v in (x, y, w)):
        return False
    lu, lv = struct.unpack_from("<2f", z, at + 28)
    return -0.01 <= lu <= 1.01 and -0.01 <= lv <= 1.01


def main():
    z = open(sys.argv[1], "rb").read()
    least = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    runs = []
    at = 0
    n = len(z)
    hits = 0
    # candidates: every place a +-1.0 float sits; a record starts 12 before it
    for sign in (ONE, MINUS):
        p = z.find(sign)
        while p >= 0:
            hits += 1
            p = z.find(sign, p + 1)
    print(f"{hits} +-1.0 floats in {n} bytes")
    seen = set()
    for sign in (ONE, MINUS):
        p = z.find(sign)
        while p >= 0:
            start = p - 12
            if start >= 0 and start not in seen and good(z, start):
                # walk back to the run's first record, then forward
                s = start
                while good(z, s - 44):
                    s -= 44
                e = s
                count = 0
                while good(z, e):
                    seen.add(e)
                    e += 44
                    count += 1
                if count >= least:
                    runs.append((count, s))
            p = z.find(sign, p + 1)
    runs.sort(reverse=True)
    for count, s in runs[:10]:
        xs = [struct.unpack_from("<3f", z, s + 44 * i) for i in range(0, count, max(1, count // 2000))]
        mn = [min(v[k] for v in xs) for k in range(3)]
        mx = [max(v[k] for v in xs) for k in range(3)]
        print(f"run of {count} vertices at {s:#x}..{s + 44 * count:#x}: box {[round(v) for v in mn]} .. {[round(v) for v in mx]}")


if __name__ == "__main__":
    main()
