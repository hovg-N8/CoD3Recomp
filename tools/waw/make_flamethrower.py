"""The M2 flamethrower's files for CoD3's port (custom\\flamethrower beside
the game), made from the user's own World at War install. The output is game
data: keep it out of the repository.

  viewmodel_flamethrower.mesh  WaW's viewmodel_usa_flamethrower (cod2import's
                               C2MS mesh), the hose's UVs moved onto a grey
                               band under the body's texture
  flamethrower_c.png           usa_flamer_m2flamethrower_c (512x512) with that
                               band below (512x576)
  weapon_flamethrower.mesh,    the world model (weapon_usa_flamethrower) and its
  flamethrower_world_c.png     colour map
  flame_fire.png               flamethrower_fire (8x8 frames: two 32-frame
                               fireballs), alpha from brightness
  flame_lick.png               fxt_fire_flame_lick (4x4), the same
  hud_m2_flamethrower.png      the kill icon (2:1)
  flm_ignite.wav, flm_loop.wav, flm_loop_far.wav, flm_cooldown.wav
                               the sounds, 16-bit PCM

usage: python -I make_flamethrower.py <WaW folder> <out folder> [zone cache]
  (<WaW folder>: ...\\Call of Duty World at War; the zone cache: a
  decompressed common_mp.ff, made there if missing)
"""
import struct
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wawimport as w  # noqa: E402

BAND = 64          # the grey band's height under the 512x512 body texture
GREY = (92, 92, 88, 255)   # WaW's hose: mp_w_skin_grey50_c under its detail maps, darkened as lit


def glow(img):
    """Black-backed fire as straight alpha: alpha from the brightest channel,
    the colour divided by it, so alpha blending looks as the additive did."""
    img = img.convert("RGB")
    out = Image.new("RGBA", img.size)
    px = []
    for r, g, b in img.get_flattened_data() if hasattr(img, "get_flattened_data") else img.getdata():
        a = max(r, g, b)
        if a == 0:
            px.append((0, 0, 0, 0))
        else:
            px.append((min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a), a))
    out.putdata(px)
    return out


def main():
    waw, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    cache = Path(sys.argv[3]) if len(sys.argv) > 3 else out / "common_mp.zone.tmp"
    if not cache.exists():
        cache.write_bytes(w.decompress_ff((waw / "zone" / "english" / "common_mp.ff").read_bytes()))
    zone = cache.read_bytes()
    main_dir = waw / "main"

    # the view model, the hose's UVs onto the band
    model = w.read_xmodel(zone, "viewmodel_usa_flamethrower")
    scale_v = 512 / (512 + BAND)
    band_v = (512 + BAND / 2) / (512 + BAND)
    for i, s in enumerate(model["surfaces"]):
        hose = i < len(model["materials"]) and model["materials"][i].endswith("_hose")
        s["verts"] = [(p, n, (0.5, band_v) if hose else (uv[0], uv[1] * scale_v)) for p, n, uv in s["verts"]]
    w.write_mesh(out / "viewmodel_flamethrower.mesh", model)

    # the world model (what others see in its holder's hands): one surface,
    # usa_flamethrower_world_c
    w.write_mesh(out / "weapon_flamethrower.mesh", w.read_xmodel(zone, "weapon_usa_flamethrower"), ["mc/mtl_weapon_flamethrower"])
    w.decode_iwi(w.find_iwi(main_dir, "usa_flamethrower_world_c")).save(out / "flamethrower_world_c.png")

    body = w.decode_iwi(w.find_iwi(main_dir, "usa_flamer_m2flamethrower_c"))
    tex = Image.new("RGBA", (512, 512 + BAND), GREY)
    tex.paste(body.resize((512, 512)), (0, 0))
    tex.save(out / "flamethrower_c.png")

    # the Thompson's other texture slots (its reflection cube, normal and
    # specular maps would show through the M2's UVs): neutral stand-ins, as
    # the Webley's (CoD3's normal maps keep x in alpha, y in green: flat_nm)
    Image.new("RGBA", (16, 16), (36, 36, 40, 255)).save(out / "dark_cube.png")
    Image.new("RGBA", (4, 4), (128, 128, 128, 128)).save(out / "flat_nm.png")
    Image.new("RGBA", (4, 4), (128, 128, 128, 255)).save(out / "grey.png")
    # (the Thompson's view draws: 4 the colour map, ours; 5 the specular; 3 the
    # normal map; 2 the reflection cube)
    (out / "slots.txt").write_text("2=dark_cube.png 3=flat_nm.png 5=grey.png")
    (out / "world_slots.txt").write_text("2=dark_cube.png 3=flat_nm.png 5=grey.png")

    glow(w.decode_iwi(w.find_iwi(main_dir, "flamethrower_fire"))).save(out / "flame_fire.png")
    glow(w.decode_iwi(w.find_iwi(main_dir, "fxt_fire_flame_lick"))).save(out / "flame_lick.png")
    w.decode_iwi(w.find_iwi(main_dir, "hud_m2_flamethrower")).save(out / "hud_m2_flamethrower.png")

    for wav, name in (("wpn_flm_ignite.wav", "flm_ignite.wav"), ("wpn_flmthwr_st_flame.wav", "flm_loop.wav"),
                      ("wpn_flmthwr_mn_flame.wav", "flm_loop_far.wav"), ("wpn_flm_cooldown.wav", "flm_cooldown.wav")):
        _n, riff = w.find_wav(zone, wav)
        ch = w.riff_chunks(riff)
        pcm, channels, rate, frames = w.msadpcm_to_pcm(ch[b"fmt "], ch[b"data"])
        (out / name).write_bytes(pcm)
        print(f"{out / name}: {channels} channel(s), {rate} Hz, {frames / rate:.2f} s")
    if cache.name.endswith(".tmp"):
        cache.unlink()


if __name__ == "__main__":
    main()
