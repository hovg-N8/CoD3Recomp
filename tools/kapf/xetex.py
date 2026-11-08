"""CoD3 textures (APKF type TEX) from the user's own .cod archives to PNG.

A TEX file's CPU part (0x60 bytes) is its description; the Xenos texture
fetch constant (6 words) is at +0x48:
  word 0: bits 0-1 type (2), bits 22-30 pitch / 32 texels, bit 31 tiled
  word 1: bits 0-5 format (0x12 DXT1, 0x13 DXT2/3, 0x14 DXT4/5, 0x06 8888,
          0x02 8), bits 6-7 endian (1: bytes swapped in pairs), bits 12-31
          base address (0 in the file: the loader adds the GPU placement)
  word 2: bits 0-12 width - 1, bits 13-25 height - 1
  word 5: bits 12-31 the mips' address (relative, like the base)
The GPU part is the pixels: the top level at the base, tiled the console's
way (render_resources.cpp TiledOffset, from Xenia).

usage: python -I xetex.py <archive.cod> <texture name> <out.png>
       python -I xetex.py <archive.cod> --all <out folder>
"""
import io
import struct
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import apkf  # noqa: E402
import kapf  # noqa: E402


def tiled_offset(x, y, pitch, log_bpp):
    aligned = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (aligned >> 5)) << (log_bpp + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << log_bpp
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) \
        + (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)


def describe(desc):
    f = struct.unpack_from(">6I", desc, 0x48)
    return {"tiled": bool(f[0] >> 31), "pitch": ((f[0] >> 22) & 0x1FF) * 32, "format": f[1] & 0x3F,
            "endian": (f[1] >> 6) & 3, "base": f[1] & 0xFFFFF000, "width": (f[2] & 0x1FFF) + 1,
            "height": ((f[2] >> 13) & 0x1FFF) + 1, "mips": f[5] & 0xFFFFF000}


BLOCK = {0x12: (4, 8), 0x13: (4, 16), 0x14: (4, 16), 0x06: (1, 4), 0x02: (1, 1)}  # format: texels a side, bytes


def log2_ceil(x):
    return max(x - 1, 0).bit_length()


def next_pow2(x):
    return 1 << log2_ceil(x)


def align(x, a):
    return (x + a - 1) // a * a


def packed_offset(width, height, mip, side):
    """Xenia's GetPackedMipOffset (2D): a packed level's place in its tile,
    in blocks (side: texels a block side)."""
    lw, lh = log2_ceil(width), log2_ceil(height)
    size = min(lw, lh)
    base = size - 4 if size > 4 else 0
    packed = mip - base
    if packed < 3:
        x, y = (0, 16 >> packed) if lw > lh else (16 >> packed, 0)
    elif lw > lh:
        x, y = (1 << (lw - base)) >> (packed - 2), 0
    else:
        x, y = 0, (1 << (lh - base)) >> (packed - 2)
    return x // side, y // side


def levels(desc):
    """Every stored level of a 2D texture: [{level, width, height, at (byte
    in the GPU part), pitch (blocks), x, y (blocks)}], by Xenia's
    GetGuestTextureLayout: the base at the base address with the fetch's
    pitch; the mips from the mip address, a surface each (power-of-two
    sizes, rows and pitch aligned to 32 blocks, 4 KB apart) up to the packed
    level (log2 of the shorter side - 4), whose surface also holds every
    smaller level at its packed offset."""
    t = describe(desc)
    f = struct.unpack_from(">6I", desc, 0x48)
    max_level = (f[4] >> 6) & 0xF
    has_packed = bool(f[5] & 0x800)
    w, h, fmt = t["width"], t["height"], t["format"]
    side, nbytes = BLOCK[fmt]
    packed_level = max(log2_ceil(min(w, h)) - 4, 0) if has_packed else 99
    out = []

    def surface(level, at, pitch_texels, rows_texels):
        pitch = align(align(pitch_texels, side) // side, 32)
        rows = align(align(rows_texels, side) // side, 32)
        subs = range(level, max_level + 1) if level == packed_level else [level]
        for sub in subs:
            x, y = packed_offset(w, h, sub, side) if level == packed_level else (0, 0)
            out.append({"level": sub, "width": max(w >> sub, 1), "height": max(h >> sub, 1), "at": at,
                        "pitch": pitch, "x": x, "y": y})
        return align(pitch * rows * nbytes, 4096)

    end = t["base"] + surface(0, t["base"], t["pitch"], h)
    at = t["mips"]
    for level in range(1, min(max_level, packed_level) + 1):
        at += surface(level, at, max(next_pow2(w) >> level, 1), max(next_pow2(h) >> level, 1))
        end = max(end, at)
    out[0]["surfaces_end"] = end  # the GPU part's size: whole surfaces
    return out


def level_image(desc, pixels, lv):
    """One stored level as an RGBA image."""
    t = describe(desc)
    fmt = t["format"]
    side, nbytes = BLOCK[fmt]
    log = nbytes.bit_length() - 1
    bw, bh = (lv["width"] + side - 1) // side, (lv["height"] + side - 1) // side
    out = bytearray(bw * bh * nbytes)
    for by in range(bh):
        for bx in range(bw):
            o = lv["at"] + tiled_offset(bx + lv["x"], by + lv["y"], lv["pitch"], log)
            out[(by * bw + bx) * nbytes:(by * bw + bx + 1) * nbytes] = pixels[o:o + nbytes]
    if t["endian"] == 1:
        out = swap_pairs(bytes(out))
    if side == 4:
        four = {0x12: b"DXT1", 0x13: b"DXT3", 0x14: b"DXT5"}[fmt]
        dds = b"DDS " + struct.pack("<7I", 124, 0x1007 | 0x80000, bh * 4, bw * 4, len(out), 0, 1) + bytes(44) \
            + struct.pack("<2I4s5I", 32, 4, four, 0, 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
        img = Image.open(io.BytesIO(dds + bytes(out)))
        img.load()
        return img.crop((0, 0, lv["width"], lv["height"])).convert("RGBA")
    if nbytes == 1:
        return Image.frombytes("L", (bw, bh), bytes(out)).convert("RGBA")
    a, r, g, b = Image.frombytes("RGBA", (bw, bh), bytes(out)).split()
    return Image.merge("RGBA", (r, g, b, a))


W3 = {0x12: 0xD10, 0x13: 0xD10, 0x14: 0xD10, 0x06: 0xC14}  # fetch word 3 (swizzle) as the game's


def tex_desc(width, height, fmt, max_level):
    """A TEX description (0x60 bytes) and its pointer words {offset: kind}:
    width, height, 0, 0, ->own name, ->own D3D object (+0x2C), 0x4000, 0 x4,
    the D3D header 00200003 1 0 0 0 FFFF0000 FFFF0000, the fetch constant.
    Tiled, pitch = width rounded to 32, base 0, mips after the base surface
    (4 KB aligned), packed levels on when there are mips."""
    side, nbytes = BLOCK[fmt]
    pitch = align(width, 32 * side)  # 32 blocks
    base_bytes = align(align(align(pitch, side) // side, 32) * align(align(height, side) // side, 32) * nbytes, 4096)
    packed_in_base = log2_ceil(min(width, height)) <= 4  # every level in the base surface
    w5 = 0x200 | ((0x800 | (0 if packed_in_base else base_bytes)) if max_level else 0)
    endian = 2 if nbytes == 4 and side == 1 else 1  # 8888: 32-bit words; DXT: 16-bit pairs
    fetch = (0x80000000 | (pitch // 32) << 22 | 2, fmt | endian << 6, (height - 1) << 13 | (width - 1),
             W3[fmt], max_level << 6, w5)
    desc = struct.pack(">4I", width, height, 0, 0) + struct.pack(">II", 0, 0) + struct.pack(">5I", 0x4000, 0, 0, 0, 0)
    desc += struct.pack(">7I", 0x00200003, 1, 0, 0, 0, 0xFFFF0000, 0xFFFF0000) + struct.pack(">6I", *fetch)
    return desc, {0x10: "name", 0x14: ("self", 0x2C)}


def tex_gpu(desc, blocks):
    """The GPU part for linear block data of each level ({level: bytes, row by
    row, ceil(w/side) x ceil(h/side) blocks, already in the game's endian})."""
    t = describe(desc)
    side, nbytes = BLOCK[t["format"]]
    log = nbytes.bit_length() - 1
    lvs = levels(desc)
    out = bytearray(b"\xCD" * lvs[0]["surfaces_end"])  # unused texels: the exporter's unset bytes
    for lv in lvs:
        bw, bh = (lv["width"] + side - 1) // side, (lv["height"] + side - 1) // side
        src = blocks[lv["level"]]
        for by in range(bh):
            for bx in range(bw):
                o = lv["at"] + tiled_offset(bx + lv["x"], by + lv["y"], lv["pitch"], log)
                out[o:o + nbytes] = src[(by * bw + bx) * nbytes:(by * bw + bx + 1) * nbytes]
    return bytes(out)


def level_blocks(desc, pixels, lv):
    """A stored level's blocks, linear, as stored (no endian change)."""
    t = describe(desc)
    side, nbytes = BLOCK[t["format"]]
    log = nbytes.bit_length() - 1
    bw, bh = (lv["width"] + side - 1) // side, (lv["height"] + side - 1) // side
    out = bytearray()
    for by in range(bh):
        for bx in range(bw):
            o = lv["at"] + tiled_offset(bx + lv["x"], by + lv["y"], lv["pitch"], log)
            out += pixels[o:o + nbytes]
    return bytes(out)


def swap_pairs(b):
    a = bytearray(b)
    a[0::2], a[1::2] = b[1::2], b[0::2]
    return bytes(a)


def to_image(desc, pixels):
    t = describe(desc)
    w, h, fmt = t["width"], t["height"], t["format"]
    data = pixels[t["base"]:]
    if fmt in (0x12, 0x13, 0x14):
        block = 8 if fmt == 0x12 else 16
        log = 3 if block == 8 else 4
        across = max(t["pitch"] // 4, 1)
        bw, bh = (w + 3) // 4, (h + 3) // 4
        out = bytearray(bw * bh * block)
        for by in range(bh):
            for bx in range(bw):
                o = tiled_offset(bx, by, across, log) if t["tiled"] else (by * across + bx) * block
                out[(by * bw + bx) * block:(by * bw + bx + 1) * block] = data[o:o + block]
        if t["endian"] == 1:
            out = swap_pairs(bytes(out))
        four = {0x12: b"DXT1", 0x13: b"DXT3", 0x14: b"DXT5"}[fmt]
        # header: size, flags, height, width, linear size, depth, mips; pixel format; caps
        dds =b"DDS " + struct.pack("<7I", 124, 0x1007 | 0x80000, bh * 4, bw * 4, len(out), 0, 1) + bytes(44) \
            + struct.pack("<2I4s5I", 32, 4, four, 0, 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
        img = Image.open(io.BytesIO(dds + bytes(out)))
        img.load()
        return img.crop((0, 0, w, h)).convert("RGBA")
    if fmt in (0x06, 0x02):
        bpp = 4 if fmt == 0x06 else 1
        log = 2 if bpp == 4 else 0
        pitch = max(t["pitch"], 1)
        out = bytearray(w * h * bpp)
        for y in range(h):
            for x in range(w):
                o = tiled_offset(x, y, pitch, log) if t["tiled"] else (y * pitch + x) * bpp
                out[(y * w + x) * bpp:(y * w + x + 1) * bpp] = data[o:o + bpp]
        if bpp == 1:
            return Image.frombytes("L", (w, h), bytes(out)).convert("RGBA")
        # 8888 is stored A R G B (big-endian words)
        argb = Image.frombytes("RGBA", (w, h), bytes(out))
        a, r, g, b = argb.split()
        return Image.merge("RGBA", (r, g, b, a))
    raise ValueError(f"format {fmt:#x} not handled")


def textures(archive):
    for b in range(len(archive.blocks())):
        d = archive.block(b)
        if d[:4] != b"APKF":
            continue
        p = apkf.read(d)
        for f in p.files:
            if p.types[f["type"]]["name"] == "TEX" and f["gpu"] is not None:
                yield b, f["name"], p.cpu(f), p.gpu(f)


def main():
    archive = kapf.Archive(sys.argv[1])
    if sys.argv[2] == "--all":
        out = Path(sys.argv[3])
        out.mkdir(parents=True, exist_ok=True)
        for b, name, desc, pixels in textures(archive):
            t = describe(desc)
            try:
                to_image(desc, pixels).save(out / f"{name}.png")
                print(f"block {b:2} {name:30} {t['width']}x{t['height']} format {t['format']:#x}")
            except (ValueError, OSError) as e:
                print(f"block {b:2} {name:30} {t['width']}x{t['height']} format {t['format']:#x}: {e}")
        return
    for b, name, desc, pixels in textures(archive):
        if name == sys.argv[2]:
            to_image(desc, pixels).save(sys.argv[3])
            print(f"{sys.argv[3]}: {describe(desc)}")
            return
    print("not found")


if __name__ == "__main__":
    main()
