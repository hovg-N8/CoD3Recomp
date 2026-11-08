"""World at War's signal flare (weapons/mp/signal_flare_mp) for CoD3's port:
custom\\flare beside the game, made from the user's own World at War install.
The output is game data: keep it out of the repository.

  flare_glow.png        fxt_light_flare3_128: the glow of its burning
                        (misc/fx_flare_sky_white_10sec_mp's only image),
                        alpha from brightness
  flare_core.png        fxt_light_flare: the bright core
  hud_icon_flare.png    its kill icon (hud_flare)
  weapon_flare.mesh,    the flare stick as thrown and lying
  flare_world_c.png     (weapon_mp_flare_stick, its colour map)
  world_slots.txt       the colour map in the smoke can's other two slots
  viewmodel_flare.mesh  in the hand (viewmodel_mp_flare_stick)
  flare_toss.wav        the throw (flashbang_toss_b)
  flare_shock.wav,      the blinded ring and its fade (flareshock_eq,
  flare_shock_fade.wav  flareshock_fade_eq)
  flare_00..03.wav      its ignition (flare_exp: sfx/destruction/exp/flare)
  flare_burn.wav        the burning trail (flashbang_asplode_trail)
                        all 16-bit PCM (from MS ADPCM, or xWMA: the ignition and
                        the trail, by the port's FFmpeg, wawimport.xwma_to_pcm)

usage: python -I make_flare.py <WaW folder> <out folder> [zone cache]
  (<WaW folder>: ...\\Call of Duty World at War; the zone cache: a
  decompressed common_mp.ff, made there if missing)
"""
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wawimport as w  # noqa: E402
from make_flamethrower import glow  # noqa: E402

SOUNDS = (("flashbang_toss_b.wav", "flare_toss.wav"), ("flareshock_eq.wav", "flare_shock.wav"),
          ("flareshock_fade_eq.wav", "flare_shock_fade.wav"),
          ("flare/flare_00.wav", "flare_00.wav"), ("flare/flare_01.wav", "flare_01.wav"),
          ("flare/flare_02.wav", "flare_02.wav"), ("flare/flare_03.wav", "flare_03.wav"),
          ("flashbang_asplode_trail.wav", "flare_burn.wav"))


def main():
    waw, out = Path(sys.argv[1]), Path(sys.argv[2])
    cache = Path(sys.argv[3]) if len(sys.argv) > 3 else out / "common_mp.zone"
    out.mkdir(parents=True, exist_ok=True)
    if not cache.exists():
        cache.write_bytes(w.decompress_ff((waw / "zone" / "english" / "common_mp.ff").read_bytes()))
    zone = cache.read_bytes()
    main_dir = waw / "main"

    glow(w.decode_iwi(w.find_iwi(main_dir, "fxt_light_flare3_128"))).save(out / "flare_glow.png")
    glow(w.decode_iwi(w.find_iwi(main_dir, "fxt_light_flare"))).save(out / "flare_core.png")
    w.decode_iwi(w.find_iwi(main_dir, "hud_flare")).save(out / "hud_icon_flare.png")
    w.decode_iwi(w.find_iwi(main_dir, "weapon_mp_flare_stick_c")).save(out / "flare_world_c.png")
    # the smoke can's draws have three textures (slots 2, 3 and 1): the stick's
    # colour map in all three (the main one, slot 2, is the host's; test
    # flaremodel5, 2026-11-03: a flat normal and grey in 1 and 3 drew it grey)
    (out / "world_slots.txt").write_text("1=flare_world_c.png 3=flare_world_c.png\n")
    for name, mesh in (("weapon_mp_flare_stick", "weapon_flare.mesh"), ("viewmodel_mp_flare_stick", "viewmodel_flare.mesh")):
        model = w.read_xmodel(zone, name)
        w.write_mesh(out / mesh, model, ["mc/mtl_weapon_mp_flare_stick"])
        verts = sum(len(s["verts"]) for s in model["surfaces"])
        print(f"{mesh}: {name}, {verts} vertices, {len(model['surfaces'])} surfaces")

    for wav, name in SOUNDS:
        found, riff = w.find_wav(zone, wav)
        ch = w.riff_chunks(riff)
        fmt = ch[b"fmt "]
        tag = int.from_bytes(fmt[0:2], "little")
        if tag == 1:     # already PCM
            data = riff
            channels, rate = int.from_bytes(fmt[2:4], "little"), int.from_bytes(fmt[4:8], "little")
            frames = len(ch[b"data"]) // (2 * channels)
        elif tag == 0x161:   # xWMA: the port's FFmpeg beside the game (the out folder's grandparent)
            data, channels, rate, frames, kbits, want = w.xwma_to_pcm(riff, out.parent.parent)
            print(f"  ({name}: xWMA at {kbits} kbit/s, {frames} samples of its {want})")
        else:
            data, channels, rate, frames = w.msadpcm_to_pcm(fmt, ch[b"data"])
        (out / name).write_bytes(data)
        print(f"{name}: {found}, {channels} channel(s), {rate} Hz, {frames / rate:.2f} s")


if __name__ == "__main__":
    main()
