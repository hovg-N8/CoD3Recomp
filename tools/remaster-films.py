"""Remastered films for CoD3Recomp (film_hd.cpp): each movies\\<name>.wmv made again
at a higher resolution and 60 frames a second, into movies\\<name>.c3film beside it.

Run with ComfyUI's Python (it has torch with CUDA, PyAV and spandrel):

  D:\\ComfyUI\\venv\\Scripts\\python.exe remaster-films.py --movies <game>\\movies
      [--films forest nightd ...] [--size 1920x1080] [--upscaler <model.pth>] [--rife <rife.pth>]
      [--quality 19] [--comfy D:\\ComfyUI]

--upscaler: any image super-resolution model spandrel loads (Real-ESRGAN, the
            compact "realesr-general" ones, 2x/4x ESRGANs); without one, bicubic.
--rife:     a RIFE 4.x checkpoint (flownet.pkl or a .safetensors of it), run
            through ComfyUI's own IFNet; without one each frame is shown twice.

The game's own films are left as they are; delete a .c3film to go back.
The format is described at the top of film_hd.cpp.
"""
import argparse, os, struct, sys, time
from fractions import Fraction
from pathlib import Path

import av
import numpy as np
import torch
import torch.nn.functional as F

THUMB_W, THUMB_H = 32, 18
SOURCE_RATE = (30000, 1001)
OUT_RATE = (60000, 1001)


def luma_thumb(rgb):
    """The brightness at 32 by 18, a byte a pixel, as film_hd.cpp's thumbnails."""
    t = torch.from_numpy(rgb).float().permute(2, 0, 1)[None] / 255.0
    l = 0.299 * t[:, 0:1] + 0.587 * t[:, 1:2] + 0.114 * t[:, 2:3]
    l = F.interpolate(l, size=(THUMB_H, THUMB_W), mode="area")
    return (l[0, 0].clamp(0, 1) * 255.0 + 0.5).to(torch.uint8).numpy().tobytes()


class Upscaler:
    def __init__(self, path, device):
        self.model = None
        if path:
            import spandrel
            self.model = spandrel.ModelLoader().load_from_file(path).to(device).eval()
            if device.type == "cuda" and self.model.supports_half:
                self.model.model.half()
                self.half = True
            else:
                self.half = False
            print(f"upscaler: {self.model.architecture.name} x{self.model.scale}")

    @torch.no_grad()
    def __call__(self, x, size):
        """x: 1x3xHxW 0..1 on the device; the picture at `size` (height, width)."""
        if self.model is not None:
            y = self.model(x.half() if self.half else x).float()
        else:
            y = x
        return F.interpolate(y, size=size, mode="bicubic", antialias=True, align_corners=False).clamp(0, 1)


class Interpolator:
    def __init__(self, path, comfy, device):
        self.model = None
        if path:
            sys.path.insert(0, str(comfy))
            import comfy.utils
            from comfy_extras.frame_interpolation_models.ifnet import IFNet, detect_rife_config
            sd = comfy.utils.load_torch_file(path, safe_load=True)
            sd = comfy.utils.state_dict_prefix_replace(sd, {"module.": "", "flownet.": ""})
            key_map = {}
            for k in sd:
                for i in range(5):
                    if k.startswith(f"block{i}."):
                        key_map[k] = f"blocks.{i}.{k[len(f'block{i}.'):]}"
            sd = {key_map.get(k, k): v for k, v in sd.items()}
            sd = {k: v for k, v in sd.items() if not k.startswith(("teacher.", "caltime."))}
            head_ch, channels = detect_rife_config(sd)
            self.model = IFNet(head_ch=head_ch, channels=channels)
            self.model.load_state_dict(sd)
            self.model.eval().to(device)
            self.align = getattr(self.model, "pad_align", 64)
            print(f"interpolation: RIFE (head {head_ch}, channels {channels})")

    @torch.no_grad()
    def __call__(self, a, b):
        """The frame halfway between a and b (1x3xHxW 0..1); without a model, a again."""
        if self.model is None:
            return a
        H, W = a.shape[2], a.shape[3]
        ph, pw = (-H) % self.align, (-W) % self.align
        pa = F.pad(a, (0, pw, 0, ph), mode="reflect") if ph or pw else a
        pb = F.pad(b, (0, pw, 0, ph), mode="reflect") if ph or pw else b
        cache = {"img0": self.model.extract_features(pa), "img1": self.model.extract_features(pb)}
        ts = torch.full((1, 1, pa.shape[2], pa.shape[3]), 0.5, device=a.device, dtype=a.dtype)
        mid = self.model(pa, pb, timestep=ts, cache=cache)
        return mid[:, :, :H, :W].clamp(0, 1)


def to_yuv420p10(x):
    """1x3xHxW RGB 0..1 to BT.709 at its limited range, ten bits, as one array
    of PyAV's yuv420p10le layout (Y, then U, then V, each flattened)."""
    r, g, b = x[0, 0], x[0, 1], x[0, 2]
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    cb = (b - y) / 1.8556
    cr = (r - y) / 1.5748
    H, W = y.shape
    Y = (64.0 + 876.0 * y).round().clamp(0, 1023)
    cb = F.avg_pool2d(cb[None, None], 2)[0, 0]
    cr = F.avg_pool2d(cr[None, None], 2)[0, 0]
    U = (512.0 + 896.0 * cb).round().clamp(0, 1023)
    V = (512.0 + 896.0 * cr).round().clamp(0, 1023)
    out = torch.cat([Y.reshape(-1), U.reshape(-1), V.reshape(-1)]).to(torch.int32).cpu().numpy().astype(np.uint16)
    return out.reshape(H * 3 // 2, W)


def remaster(wmv, out_path, size, upscaler, interpolator, quality, device):
    W, H = size
    container = av.open(str(wmv))
    stream = container.streams.video[0]
    encoder = av.CodecContext.create("hevc_nvenc", "w")
    encoder.width, encoder.height = W, H
    encoder.pix_fmt = "p010le"
    encoder.time_base = Fraction(OUT_RATE[1], OUT_RATE[0])
    encoder.framerate = Fraction(OUT_RATE[0], OUT_RATE[1])
    encoder.options = {"preset": "p6", "tune": "hq", "rc": "vbr", "cq": str(quality), "b": "0", "bf": "0",
                       "g": "60", "profile": "main10", "spatial-aq": "1", "temporal-aq": "1"}
    encoder.open()

    thumbs, packets = [], []
    out_index = 0

    def encode(x):
        nonlocal out_index
        frame = av.VideoFrame.from_ndarray(to_yuv420p10(x), format="yuv420p10le").reformat(format="p010le")
        frame.pts = out_index
        out_index += 1
        for p in encoder.encode(frame):
            packets.append((bytes(p), 1 if p.is_keyframe else 0))

    previous = None
    count = 0
    started = time.time()
    for frame in container.decode(stream):
        rgb = frame.to_ndarray(format="rgb24")
        thumbs.append(luma_thumb(rgb))
        x = torch.from_numpy(rgb).to(device).permute(2, 0, 1)[None].float() / 255.0
        x = upscaler(x, (H, W))
        if previous is not None:
            encode(previous)
            encode(interpolator(previous, x))
        previous = x
        count += 1
        if count % 100 == 0:
            rate = count / (time.time() - started)
            print(f"  {wmv.name}: {count} frames, {rate:.1f} a second", flush=True)
    if previous is not None:
        encode(previous)
        encode(previous)
    for p in encoder.encode(None):
        packets.append((bytes(p), 1 if p.is_keyframe else 0))
    container.close()
    extra = bytes(encoder.extradata or b"")

    header = b"C3FILM1\0" + struct.pack("<12I", 0, W, H, OUT_RATE[0], OUT_RATE[1], len(packets),
                                       len(thumbs), SOURCE_RATE[0], SOURCE_RATE[1], THUMB_W, THUMB_H, len(extra))
    start = len(header) + len(extra) + len(thumbs) * THUMB_W * THUMB_H + len(packets) * 16
    index, where = [], start
    for data, key in packets:
        index.append(struct.pack("<QII", where, len(data), key))
        where += len(data)
    temp = out_path.with_suffix(".c3film.part")
    with open(temp, "wb") as f:
        f.write(header)
        f.write(extra)
        f.write(b"".join(thumbs))
        f.write(b"".join(index))
        for data, _ in packets:
            f.write(data)
    os.replace(temp, out_path)
    print(f"{out_path.name}: {count} frames in, {len(packets)} out, {where / 1048576:.1f} MB, {time.time() - started:.0f} s", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--movies", required=True)
    parser.add_argument("--films", nargs="*")
    parser.add_argument("--size", default="1920x1080")
    parser.add_argument("--upscaler")
    parser.add_argument("--rife")
    parser.add_argument("--quality", type=int, default=19)
    parser.add_argument("--comfy", default=r"D:\ComfyUI")
    parser.add_argument("--out", help="where the .c3film files go (default: beside the .wmv)")
    args = parser.parse_args()
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    size = tuple(int(v) for v in args.size.lower().split("x"))
    upscaler = Upscaler(args.upscaler, device)
    interpolator = Interpolator(args.rife, Path(args.comfy), device)
    movies = Path(args.movies)
    names = args.films or sorted(p.stem for p in movies.glob("*.wmv"))
    for name in names:
        wmv = movies / f"{name}.wmv"
        if not wmv.exists():
            print(f"{wmv} is not there")
            continue
        dest = Path(args.out) / f"{name}.c3film" if args.out else movies / f"{name}.c3film"
        if dest.exists():
            print(f"{dest.name}: done already"); continue
        remaster(wmv, dest, size, upscaler, interpolator, args.quality, device)


if __name__ == "__main__":
    main()
