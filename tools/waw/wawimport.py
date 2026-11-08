"""Call of Duty: World at War (PC) assets from the user's own install, for
CoD3's port (the flamethrower, 2026-11-01). Reads, never runs, them.

Fastfiles (zone\\english\\*.ff): 'IWffu100', u32 version 0x183, then one zlib
stream: the zone. The zone (little-endian):
  0x00 u32 size, u32 external size, block sizes; 0x24 u32 script strings,
  s32 their pointer (-1), u32 assets, s32 their pointer (-1); then the
  script strings' pointers (0 or -1) and the strings; then the asset list
  (u32 type, s32 pointer) and the assets' data, each written depth-first in
  the order the game loads it, with NO padding (alignment is applied in
  memory only). A pointer -1 means "its data follows here".
XModel (0xE4 bytes; checked on viewmodel_usa_flamethrower):
  +0x00 name, u8 bones, u8 root bones, u8 surfaces, u8 lod ramp,
  +0x08 -> bone names (u16 script strings), parents (u8, relative), quats
  (4 x s16 a non-root bone), trans (3 x f32 a non-root bone, then 4 bytes
  a non-root bone: 16 x non-root in all; checked on 13- and 2-bone models), part classes (u8 a bone), base pose (DObjAnimMat: f32 quat[4],
  trans[3], weight; a bone), surfaces, materials; +0x28 lod info x4 (28
  bytes); +0xA4 bone info; +0xA8 radius, mins, maxs; then the name follows.
XSurface (0x40 bytes): u8 tile, u8 deformed, u16 vertices, u16 triangles,
  u16 base triangle, u16 base vertex, pad, +0x0C -> indices, +0x10 vertex
  info (s16 counts[4], -> blend), +0x1C -> vertices, +0x20 (0), +0x24 u32
  rigid lists, +0x28 -> rigid lists, then part bits.
  Its data: the vertices (32 bytes: f32 xyz in the model's base pose, f32
  binormal sign, colour, half2 uv, packed normal, packed tangent), the rigid
  lists (12 bytes: u16 bone offset (/64 = bone), u16 vertices, u16 first
  triangle, u16 triangles, -> collision tree: 40-byte header (trans, scale,
  u32 nodes, ->, u32 leaves, ->), 16-byte nodes, u16 leaves), then the
  triangles (3 x u16).
Sounds: RIFF WAVE files inline, each after its name ("....wav\\0RIFF");
  MS ADPCM (format 2): decoded here to 16-bit PCM.
Images: main\\iw_NN.iwd (zip) images\\<name>.iwi: 'IWi', version 6.

usage:
  python -I wawimport.py ff <file.ff> <out.zone>
  python -I wawimport.py model <zone> <xmodel name> <out.mesh>   (cod2import's C2MS mesh)
  python -I wawimport.py sound <zone> <wav name ending> <out.wav>
  python -I wawimport.py image <WaW main folder> <image name> <out.png>
  python -I wawimport.py strings <zone> <xmodel name>        (bone names)
"""
import math
import re
import struct
import sys
import zipfile
import zlib
from pathlib import Path


# --- fastfile and zone ------------------------------------------------------------

def decompress_ff(data):
    if data[:8] != b"IWffu100":
        raise ValueError("not a World at War fastfile")
    return zlib.decompress(data[12:])


def script_strings(zone):
    count = struct.unpack_from("<I", zone, 0x24)[0]
    at = 0x34 + 4 * count
    out = []
    for i in range(count):
        if struct.unpack_from("<i", zone, 0x34 + 4 * i)[0] == 0:
            out.append("")
            continue
        end = zone.index(b"\0", at)
        out.append(zone[at:end].decode("latin-1"))
        at = end + 1
    return out


# --- XModel -----------------------------------------------------------------------

class R:
    def __init__(self, d, at):
        self.d, self.at = d, at

    def take(self, fmt):
        v = struct.unpack_from("<" + fmt, self.d, self.at)
        self.at += struct.calcsize("<" + fmt)
        return v if len(v) > 1 else v[0]

    def bytes(self, n):
        b = self.d[self.at:self.at + n]
        self.at += n
        return b


def half(h):
    return struct.unpack("<e", struct.pack("<H", h))[0]


def unit_vec(packed):
    b = struct.pack("<I", packed)
    scale = (b[3] + 192.0) / 32385.0
    v = [(b[k] - 127.0) * scale for k in range(3)]
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return [c / n for c in v]


def find_xmodel(zone, name):
    key = name.encode() + b"\0"
    at = 0
    while True:
        at = zone.find(key, at)
        if at < 0:
            raise ValueError(f"xmodel {name} not found")
        # the XModel's own name, written right after it: the 0xE4 bytes
        # before start with its name pointer -1 and hold sane counts
        start = at - 0xE4
        if start >= 0 and struct.unpack_from("<i", zone, start)[0] == -1 and 0 < zone[start + 4] <= 128 and 0 < zone[start + 6] <= 32:
            return start
        at += 1


def read_xmodel(zone, name, start=None):
    """start: the header's place when its name is a pointer, not written after
    it (Seelow's sky dome skybox_see1, 2026-11-02): the data follow at once."""
    named_inline = start is None
    if start is None:
        start = find_xmodel(zone, name)
    r = R(zone, start)
    r.take("i")
    nbones, nroot, nsurfs, _ramp = r.take("BBBB")
    ptrs = r.take("8i")
    lods = [r.take("fHH5I") for _ in range(4)]
    r.at = start + 0xE4
    if named_inline:
        r.at = zone.index(b"\0", r.at) + 1          # the name
    strings = script_strings(zone)
    bones = [strings[i] if i < len(strings) else f"bone{i}" for i in (r.take("H") for _ in range(nbones))]
    nonroot = nbones - nroot
    parents = list(r.bytes(nonroot))
    r.at += 8 * nonroot                          # quats
    r.at += 16 * nonroot                         # trans (3 x f32 a non-root bone, then 4 bytes more each)
    r.at += nbones                               # part classes
    base = [r.take("8f") for _ in range(nbones)]  # quat xyzw, trans xyz, weight
    surf_heads = []
    for _ in range(nsurfs):
        s = r.at
        tile, deformed, nv, nt, btri, bvert = struct.unpack_from("<BBHHHH", zone, s)
        nrigid = struct.unpack_from("<I", zone, s + 0x24)[0]
        blend = struct.unpack_from("<4h", zone, s + 0x10)
        surf_heads.append({"vertices": nv, "triangles": nt, "rigid": nrigid, "blend": blend,
                           "blendptr": struct.unpack_from("<i", zone, s + 0x18)[0]})
        r.at += 0x40
    surfaces = []
    for h in surf_heads:
        if h["blendptr"] == -1:
            b = h["blend"]
            r.at += 2 * (b[0] + 3 * b[1] + 5 * b[2] + 7 * b[3])
        verts = []
        for _ in range(h["vertices"]):
            x, y, z, _sign, _col, u, v, n, _t = r.take("4fIHHII")
            verts.append(((x, y, z), unit_vec(n), (half(u), half(v))))
        # all of a surface's rigid records (12 bytes: bone offset, vertices, first
        # triangle, triangles, its collision tree), then the trees written inline
        # (2026-11-02: interleaved, a surface of several parts read its second
        # record as the first one's tree: Seelow's 66-bone Panzer wreck)
        rigid, trees = [], 0
        for _ in range(h["rigid"]):
            boff, rv, rtri, rnt, tree = r.take("HHHHi")
            rigid.append((boff // 64, rv))
            trees += tree == -1
        for _ in range(trees):
            nodes, nodesp, leaves, leavesp = struct.unpack_from("<IiIi", zone, r.at + 24)
            r.at += 40
            if nodesp == -1:
                r.at += 16 * nodes
            if leavesp == -1:
                r.at += 2 * leaves
        tris = [r.take("3H") for _ in range(h["triangles"])]
        bone_of = []
        for bone, count in rigid:
            bone_of += [bone] * count
        bone_of += [0] * (len(verts) - len(bone_of))
        surfaces.append({"verts": verts, "tris": tris, "bones": bone_of})
    after = r.at
    materials = re.findall(rb"(m[ct]/[A-Za-z0-9_/~\-]+|mtl_[A-Za-z0-9_~\-]+)\x00", zone[after:after + 0x4000])
    return {"start": start, "bones": bones, "parents": parents, "base": base, "surfaces": surfaces,
            "lods": lods, "materials": [m.decode() for m in materials], "end": after}


def write_mesh(path, model, materials=None):
    verts, tris, spans = [], [], []
    for i, s in enumerate(model["surfaces"]):
        first = len(verts)
        name = (materials or model["materials"] or [""] * 8)[i] if i < len(materials or model["materials"] or []) else ""
        spans.append((len(tris), len(s["tris"]), name))
        for (p, n, uv), bone in zip(s["verts"], s["bones"]):
            verts.append((p, n, uv, bone))
        # World at War's triangles wind the other way round from CoD2's as
        # cod2import writes them (it swaps CoD2's second and third)
        tris += [(a + first, b + first, c + first) for a, b, c in s["tris"]]
    out = bytearray(b"C2MS" + struct.pack("<IIII", 1, len(verts), len(tris), len(spans)))
    for p, n, uv, bone in verts:
        out += struct.pack("<3f3f2fI", *p, *n, *uv, bone)
    for t in tris:
        out += struct.pack("<3H", *t)
    for first, count, name in spans:
        nb = name.encode()
        out += struct.pack("<III", first, count, len(nb)) + nb
    out += struct.pack("<I", len(model["bones"]))
    for i, name in enumerate(model["bones"]):
        nb = name.encode()
        out += struct.pack("<I", len(nb)) + nb + struct.pack("<3f", *model["base"][i][4:7])
    Path(path).write_bytes(out)
    lo = [min(v[0][k] for v in verts) for k in range(3)]
    hi = [max(v[0][k] for v in verts) for k in range(3)]
    print(f"{path}: {len(verts)} vertices, {len(tris)} triangles, surfaces {[(s[1], s[2]) for s in spans]}; "
          f"box {[round(x, 2) for x in lo]} .. {[round(x, 2) for x in hi]}")


# --- sounds -----------------------------------------------------------------------

ADAPT = [230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230]
COEF1 = [256, 512, 0, 192, 240, 460, 392]
COEF2 = [0, -256, 0, 64, 0, -208, -232]


def msadpcm_to_pcm(fmt, data):
    channels, rate = struct.unpack_from("<HI", fmt, 2)
    block = struct.unpack_from("<H", fmt, 12)[0]
    per_block = struct.unpack_from("<H", fmt, 18)[0]
    out = []
    for b in range(0, len(data) - 7 * channels + 1, block):
        blk = data[b:b + block]
        if len(blk) < 7 * channels:
            break
        pred = list(blk[0:channels])
        o = channels
        delta = list(struct.unpack_from(f"<{channels}h", blk, o)); o += 2 * channels
        s1 = list(struct.unpack_from(f"<{channels}h", blk, o)); o += 2 * channels
        s2 = list(struct.unpack_from(f"<{channels}h", blk, o)); o += 2 * channels
        frames = [[s2[c] for c in range(channels)], [s1[c] for c in range(channels)]]
        nibbles = []
        for byte in blk[o:]:
            nibbles += [byte >> 4, byte & 0xF]
        c = 0
        frame = []
        for nib in nibbles:
            if len(frames) >= per_block:
                break
            p = pred[c] if pred[c] < 7 else 0
            sample = (s1[c] * COEF1[p] + s2[c] * COEF2[p]) // 256
            signed = nib - 16 if nib & 8 else nib
            sample = max(-32768, min(32767, sample + signed * delta[c]))
            s2[c], s1[c] = s1[c], sample
            delta[c] = max(16, (ADAPT[nib] * delta[c]) // 256)
            frame.append(sample)
            c += 1
            if c == channels:
                frames.append(frame)
                frame, c = [], 0
        for f in frames:
            out += f
    pcm = struct.pack(f"<{len(out)}h", *out)
    head = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, channels, rate, rate * 2 * channels, 2 * channels, 16) \
        + b"data" + struct.pack("<I", len(pcm))
    return head + pcm, channels, rate, len(out) // channels


def xwma_to_pcm(riff, dlls):
    """World at War's xWMA sounds (RIFF XWMA, WMA v2 in 2230-byte packets: the
    signal flare's flare_exp and burning trail) to 16-bit PCM, by FFmpeg's
    wmav2 from the port's install (dlls: avcodec-62.dll, avutil-60.dll).
    FFmpeg's xwma demuxer gives WMA v2 six bytes of extradata, 31 at [4]. The
    header's bytes a second (12000 for flare_00) is not the coding's rate:
    decoded at it, every packet misreads (its superframes' offsets are read
    with the rate's bit width). The rate whose length matches the file's own
    decoded size (its dpds chunk's last count) is used: 48000 bits for
    flare_00 (2026-11-03)."""
    import ctypes
    import os
    ch = riff_chunks(riff)
    fmt, data = ch[b"fmt "], ch[b"data"]
    dpds = ch.get(b"dpds", b"")
    channels, rate, _per_second, block = struct.unpack_from("<HIIH", fmt, 2)
    want = struct.unpack_from("<I", dpds, len(dpds) - 4)[0] // (2 * channels) if len(dpds) >= 4 else 0
    os.add_dll_directory(str(dlls))
    avutil = ctypes.CDLL(str(Path(dlls) / "avutil-60.dll"))
    avcodec = ctypes.CDLL(str(Path(dlls) / "avcodec-62.dll"))
    vp = ctypes.c_void_p
    avcodec.avcodec_find_decoder_by_name.restype = vp
    avcodec.avcodec_alloc_context3.restype = vp
    avcodec.avcodec_alloc_context3.argtypes = [vp]
    avcodec.avcodec_open2.argtypes = [vp, vp, vp]
    avcodec.av_packet_alloc.restype = vp
    avcodec.av_new_packet.argtypes = [vp, ctypes.c_int]
    avcodec.av_packet_unref.argtypes = [vp]
    avcodec.avcodec_send_packet.argtypes = [vp, vp]
    avcodec.avcodec_receive_frame.argtypes = [vp, vp]
    avutil.av_frame_alloc.restype = vp
    avutil.av_malloc.restype = vp
    avutil.av_malloc.argtypes = [ctypes.c_size_t]
    avutil.av_opt_set_int.argtypes = [vp, ctypes.c_char_p, ctypes.c_int64, ctypes.c_int]
    avutil.av_opt_set.argtypes = [vp, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    avutil.av_log_set_level(-8)

    def decode(bits):
        codec = avcodec.avcodec_find_decoder_by_name(b"wmav2")
        ctx = avcodec.avcodec_alloc_context3(codec)
        avutil.av_opt_set_int(ctx, b"ar", rate, 0)
        avutil.av_opt_set_int(ctx, b"b", bits, 0)
        avutil.av_opt_set_int(ctx, b"block_align", block, 0)
        avutil.av_opt_set(ctx, b"ch_layout", b"mono" if channels == 1 else b"stereo", 0)
        extra = avutil.av_malloc(70)
        ctypes.memset(extra, 0, 70)
        ctypes.c_uint8.from_address(extra + 4).value = 31
        ctypes.c_void_p.from_address(ctx + 72).value = extra      # AVCodecContext.extradata (FFmpeg 8, 64-bit)
        ctypes.c_int.from_address(ctx + 80).value = 6             # .extradata_size
        if avcodec.avcodec_open2(ctx, codec, None) < 0:
            return []
        pkt, frame = avcodec.av_packet_alloc(), avutil.av_frame_alloc()
        out = []
        for at in range(0, len(data) - block + 1, block):
            avcodec.av_new_packet(pkt, block)
            ctypes.memmove(ctypes.c_void_p.from_address(pkt + 24).value, data[at:at + block], block)   # AVPacket.data
            avcodec.avcodec_send_packet(ctx, pkt)
            avcodec.av_packet_unref(pkt)
            while avcodec.avcodec_receive_frame(ctx, frame) == 0:
                n = ctypes.c_int.from_address(frame + 112).value          # AVFrame.nb_samples
                ext = ctypes.c_void_p.from_address(frame + 96).value      # .extended_data (float planes)
                planes = [(ctypes.c_float * n).from_address(ctypes.c_void_p.from_address(ext + 8 * c).value) for c in range(channels)]
                for i in range(n):
                    for c in range(channels):
                        out.append(max(-32768, min(32767, int(planes[c][i] * 32767.0))))
        return out

    best = None
    for kbits in (20, 32, 48, 64, 96, 128, 160, 192):
        out = decode(kbits * 1000)
        frames = len(out) // channels
        if out and (best is None or abs(frames - want) < abs(best[1] - want)):
            best = (out, frames, kbits)
    if not best:
        raise ValueError("FFmpeg's wmav2 decoded nothing")
    out, frames, kbits = best
    pcm = struct.pack(f"<{len(out)}h", *out)
    head = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, channels, rate, rate * 2 * channels, 2 * channels, 16) \
        + b"data" + struct.pack("<I", len(pcm))
    return head + pcm, channels, rate, frames, kbits, want


def find_wav(zone, ending):
    for m in re.finditer(rb"([A-Za-z0-9_/\\.\-]{3,120}\.wav)\x00RIFF", zone):
        if m.group(1).decode("latin-1").endswith(ending):
            at = m.end() - 4
            size = struct.unpack_from("<I", zone, at + 4)[0]
            return m.group(1).decode("latin-1"), zone[at:at + 8 + size]
    raise ValueError(f"no wav ending {ending}")


def riff_chunks(riff):
    at, out = 12, {}
    while at + 8 <= len(riff):
        kind, size = riff[at:at + 4], struct.unpack_from("<I", riff, at + 4)[0]
        out[kind] = riff[at + 8:at + 8 + size]
        at += 8 + size + (size & 1)
    return out


# --- images -----------------------------------------------------------------------

def find_iwi(main, name):
    for iwd in sorted(Path(main).glob("iw_*.iwd"), reverse=True):
        with zipfile.ZipFile(iwd) as z:
            for n in z.namelist():
                if n.lower() == f"images/{name.lower()}.iwi":
                    return z.read(n)
    raise ValueError(f"image {name} not found")


def decode_iwi(data):
    from PIL import Image
    import io
    if data[:3] != b"IWi" or data[3] != 6:
        raise ValueError(f"not a version 6 IWi ({data[:4]!r})")
    # 'IWi', version, format, usage, u16 width, height, depth, then u32 file
    # sizes for picmip (28 bytes); the levels smallest first: the largest is
    # the file's last bytes
    fmt, usage, w, h, depth = struct.unpack_from("<BBHHH", data, 4)
    if fmt in (0x0B, 0x0C, 0x0D):
        four = {0x0B: b"DXT1", 0x0C: b"DXT3", 0x0D: b"DXT5"}[fmt]
        size = max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * (8 if fmt == 0x0B else 16)
        dds = b"DDS " + struct.pack("<7I", 124, 0x1007 | 0x80000, h, w, size, 0, 1) + bytes(44) \
            + struct.pack("<2I4s5I", 32, 4, four, 0, 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
        img = Image.open(io.BytesIO(dds + data[-size:]))
        img.load()
        return img.convert("RGBA")
    if fmt == 0x01:   # ARGB32 (stored B G R A)
        b, g, r, a = Image.frombytes("RGBA", (w, h), data[-w * h * 4:]).split()
        return Image.merge("RGBA", (r, g, b, a))
    if fmt == 0x02:   # RGB24 (B G R)
        b, g, r = Image.frombytes("RGB", (w, h), data[-w * h * 3:]).split()
        return Image.merge("RGB", (r, g, b)).convert("RGBA")
    if fmt == 0x03:   # luminance and alpha
        return Image.frombytes("LA", (w, h), data[-w * h * 2:]).convert("RGBA")
    if fmt == 0x04:
        return Image.frombytes("L", (w, h), data[-w * h:]).convert("RGBA")
    raise ValueError(f"IWi format {fmt:#x}")


def main():
    cmd = sys.argv[1]
    if cmd == "ff":
        z = decompress_ff(Path(sys.argv[2]).read_bytes())
        Path(sys.argv[3]).write_bytes(z)
        print(f"{sys.argv[3]}: {len(z)} bytes")
    elif cmd in ("model", "strings"):
        zone = Path(sys.argv[2]).read_bytes()
        m = read_xmodel(zone, sys.argv[3])
        print(f"{sys.argv[3]} at {m['start']:#x}: {len(m['bones'])} bones, {len(m['surfaces'])} surfaces, materials {m['materials']}")
        for i, b in enumerate(m["bones"]):
            print(f"  bone {i:2} {b:24} base pos {[round(x, 3) for x in m['base'][i][4:7]]}")
        if cmd == "model":
            write_mesh(sys.argv[4], m)
    elif cmd == "sound":
        zone = Path(sys.argv[2]).read_bytes()
        name, riff = find_wav(zone, sys.argv[3])
        ch = riff_chunks(riff)
        fmt = ch[b"fmt "]
        if struct.unpack_from("<H", fmt, 0)[0] == 2:
            wav, channels, rate, frames = msadpcm_to_pcm(fmt, ch[b"data"])
        else:
            wav, channels, rate, frames = riff, *struct.unpack_from("<HI", fmt, 2), 0
        Path(sys.argv[4]).write_bytes(wav)
        print(f"{sys.argv[4]}: {name}, {channels} channel(s), {rate} Hz, {frames} frames ({frames / max(rate, 1):.2f} s)")
    elif cmd == "image":
        img = decode_iwi(find_iwi(sys.argv[2], sys.argv[3]))
        img.save(sys.argv[4])
        print(f"{sys.argv[4]}: {img.size}")


if __name__ == "__main__":
    main()
