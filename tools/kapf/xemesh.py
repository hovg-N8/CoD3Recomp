"""CoD3 meshes (APKF type MESH) from the user's own .cod archives to OBJ.

A MESH file's CPU part is its description (pointers resolved by apkf.Pack):
  +0x00 name record          +0x08 u32 submeshes, +0x0C -> list of (u32, ->submesh)
  +0x14 u32 lods, +0x18 -> list of (->mesh, f32 distance)
  +0x1C u32 triangles        +0x20.. bounds
a submesh:
  +0x00 f32 sphere centre xyz, radius   +0x10 min xyzw   +0x20 max xyzw
  +0x30 f32                  +0x34 -> MAT description (or an import)
  +0x38 -> bone palette (u16s) or 0, +0x3C u32
  +0x40 -> vertex buffer     +0x48 u32 vertices
  +0x50 -> index buffer      +0x58 u32 indices
  +0x60 -> declaration (u32 stride, -> elements)   +0x64 u32 primitive (4: list)
a vertex buffer: D3D header (6 words), then the fetch constant: word 0 =
  offset in the mesh's GPU part | 3, word 1 = bytes / 4 << 2 | endian (2:
  32-bit words big-endian). An index buffer: D3D header (6 words; common
  0x20200002: 16-bit), u32 offset in the GPU part, u32 bytes.
an element (12 bytes): u16 stream, u16 offset, u32 type (0x2A23B9 FLOAT3,
  0x2C23A5 FLOAT2, 0x2A2187 DEC3N, 0x182886 COLOR, 0x1A2286 UBYTE4,
  0x1A2086 UBYTE4N, ...), u8 method, u8 usage (0 position, 3 normal,
  5 uv, 10 colour, 6 tangent, 7 binormal, 1 weight, 2 indices), u8 index;
  ends at stream 0xFF.

usage: python -I xemesh.py <archive.cod> <mesh name prefix> <out.obj>
       python -I xemesh.py <archive.cod> --all <out.obj>    every mesh of it
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402

FLOAT3, FLOAT2, FLOAT4, FLOAT16_2, FLOAT16_4 = 0x2A23B9, 0x2C23A5, 0x1A23A6, 0x2C235F, 0x1A2360
SHORT2N, SHORT4N = 0x2C2159, 0x1A215A


def w(d, a):
    return struct.unpack_from(">I", d, a)[0]


class Mesh:
    def __init__(self, pack, f):
        self.pack, self.f, self.d = pack, f, pack.d
        self.gpu = pack.gpu(f)
        self.submeshes = []
        d, at = self.d, f["at"]
        count, lst = w(d, at + 8), pack.target(w(d, at + 0x0C))
        for i in range(count):
            self.submeshes.append(self.submesh(pack.target(w(d, lst + 8 * i + 4))))

    def submesh(self, s):
        d, p = self.d, self.pack
        vb, ib, decl = p.target(w(d, s + 0x40)), p.target(w(d, s + 0x50)), p.target(w(d, s + 0x60))
        stride, elements = w(d, decl), []
        e = p.target(w(d, decl + 4))
        while struct.unpack_from(">H", d, e)[0] != 0xFF and len(elements) < 32:
            stream, offset, kind = struct.unpack_from(">HHI", d, e)
            elements.append({"offset": offset, "type": kind, "usage": d[e + 9], "index": d[e + 10]})
            e += 12
        vfetch = struct.unpack_from(">2I", d, vb + 24)
        istart, ibytes = struct.unpack_from(">2I", d, ib + 24)
        material = w(d, s + 0x34)
        return {"at": s, "vertices": w(d, s + 0x48), "indices": w(d, s + 0x58), "stride": stride, "elements": elements,
                "vstart": vfetch[0] & ~3, "vbytes": (vfetch[1] >> 2) * 4, "istart": istart, "ibytes": ibytes,
                "index32": w(d, ib) & 0x20000000 == 0 and False, "material": p.target(material), "prim": w(d, s + 0x64)}

    def element(self, sm, usage, index=0):
        return next((e for e in sm["elements"] if e["usage"] == usage and e["index"] == index), None)

    def read(self, sm, v, e):
        at = sm["vstart"] + v * sm["stride"] + e["offset"]
        g, t = self.gpu, e["type"]
        if t == FLOAT3:
            return struct.unpack_from(">3f", g, at)
        if t == FLOAT2:
            return struct.unpack_from(">2f", g, at)
        if t == FLOAT4:
            return struct.unpack_from(">4f", g, at)
        if t == FLOAT16_2:
            return struct.unpack_from(">2e", g, at)
        if t == FLOAT16_4:
            return struct.unpack_from(">4e", g, at)
        if t == SHORT2N:
            return tuple(x / 32767 for x in struct.unpack_from(">2h", g, at))
        if t == SHORT4N:
            return tuple(x / 32767 for x in struct.unpack_from(">4h", g, at))
        raise ValueError(f"element type {t:#x}")

    def triangles(self, sm):
        n = sm["indices"]
        idx = struct.unpack_from(f">{n}H", self.gpu, sm["istart"])
        if sm["prim"] == 4:
            return [idx[i:i + 3] for i in range(0, n - 2, 3)]
        if sm["prim"] == 6:   # strip
            out = []
            for i in range(n - 2):
                a, b, c = idx[i:i + 3]
                if a != b and b != c and a != c:
                    out.append((a, b, c) if i % 2 == 0 else (b, a, c))
            return out
        raise ValueError(f"primitive {sm['prim']}")


def meshes(archive):
    for b in range(len(archive.blocks())):
        d = archive.block(b)
        if d[:4] != b"APKF":
            continue
        p = apkf.read(d)
        for f in p.files:
            if p.types[f["type"]]["name"] == "MESH" and f["gpu"] is not None:
                yield b, p, f


def write_obj(out, items):
    lines, base = [], 1
    for b, p, f in items:
        m = Mesh(p, f)
        for k, sm in enumerate(m.submeshes):
            pos, uv = m.element(sm, 0), m.element(sm, 5)
            lines.append(f"o b{b}_{f['name']}_{k}")
            for v in range(sm["vertices"]):
                x, y, z = m.read(sm, v, pos)[:3]
                lines.append(f"v {x:.4f} {y:.4f} {z:.4f}")
            if uv:
                for v in range(sm["vertices"]):
                    s, t = m.read(sm, v, uv)[:2]
                    lines.append(f"vt {s:.5f} {1 - t:.5f}")
            for a, bb, c in m.triangles(sm):
                if uv:
                    lines.append(f"f {a + base}/{a + base} {bb + base}/{bb + base} {c + base}/{c + base}")
                else:
                    lines.append(f"f {a + base} {bb + base} {c + base}")
            base += sm["vertices"]
    Path(out).write_text("\n".join(lines) + "\n")


def main():
    archive = kapf.Archive(sys.argv[1])
    found = [x for x in meshes(archive) if sys.argv[2] == "--all" or x[2]["name"].startswith(sys.argv[2])]
    for b, p, f in found:
        m = Mesh(p, f)
        for k, sm in enumerate(m.submeshes):
            kinds = " ".join(f"{e['usage']}.{e['index']}:{e['type']:06X}" for e in sm["elements"])
            print(f"block {b:2} {f['name']:30} sub {k}: {sm['vertices']:5} v {sm['indices']:6} i stride {sm['stride']:#x} prim {sm['prim']} [{kinds}]")
    write_obj(sys.argv[3], found)
    print(f"{sys.argv[3]}: {len(found)} meshes")


if __name__ == "__main__":
    main()
