"""Texture detail upscaler for CoD3Recomp's texture packs (texture_pack.cpp).

Each dumped texture (textures\\dump\\<key>.png) is made larger (2x for 1024 and up, 4x
below) and given finer detail by SDXL through a running ComfyUI (img2img at a low
strength, in overlapping 1024 tiles); only SDXL's fine detail (a high pass) is kept
and added to a clean Lanczos enlargement, so the texture's colours, layout, UV island
edges and alpha stay the game's own. The result goes to textures\\replace\\<key>.png.

Only colour textures: never normal maps, specular or other data textures.

  python upscale-textures.py --dump <...\\textures\\dump> --out <...\\textures\\replace>
      --keys key1 key2 ...  (or --list file)  [--comfy http://127.0.0.1:8188]
      [--checkpoint sd_xl_base_1.0.safetensors] [--denoise 0.3] [--detail 1.0]

Needs ComfyUI running with an SDXL checkpoint; numpy and Pillow.
"""
import argparse, io, json, os, random, time, urllib.request, uuid
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

POSITIVE = ("high resolution video game texture, photographic, sharp fine surface detail, "
            "wood grain, worn painted metal, woven fabric, leather, skin pores, scratches")
NEGATIVE = "blurry, smooth, lowres, text, watermark, signature, cartoon, painting, jpeg artifacts"


def post(url, data, content_type="application/json"):
    req = urllib.request.Request(url, data=data, headers={"Content-Type": content_type})
    with urllib.request.urlopen(req, timeout=600) as r:
        return r.read()


def upload(comfy, image, name):
    boundary = uuid.uuid4().hex
    buf = io.BytesIO(); image.save(buf, format="PNG")
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"image\"; filename=\"{name}\"\r\n"
            f"Content-Type: image/png\r\n\r\n").encode() + buf.getvalue() + \
           f"\r\n--{boundary}\r\nContent-Disposition: form-data; name=\"overwrite\"\r\n\r\ntrue\r\n--{boundary}--\r\n".encode()
    return json.loads(post(comfy + "/upload/image", body, f"multipart/form-data; boundary={boundary}"))["name"]


def run_tile(comfy, checkpoint, tile, denoise, seed):
    name = upload(comfy, tile, f"cod3tile_{uuid.uuid4().hex[:8]}.png")
    graph = {
        "1": {"class_type": "CheckpointLoaderSimple", "inputs": {"ckpt_name": checkpoint}},
        "2": {"class_type": "LoadImage", "inputs": {"image": name}},
        "3": {"class_type": "VAEEncode", "inputs": {"pixels": ["2", 0], "vae": ["1", 2]}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"text": POSITIVE, "clip": ["1", 1]}},
        "5": {"class_type": "CLIPTextEncode", "inputs": {"text": NEGATIVE, "clip": ["1", 1]}},
        "6": {"class_type": "KSampler", "inputs": {"model": ["1", 0], "positive": ["4", 0], "negative": ["5", 0],
                                                   "latent_image": ["3", 0], "seed": seed, "steps": 24, "cfg": 5.0,
                                                   "sampler_name": "dpmpp_2m", "scheduler": "karras", "denoise": denoise}},
        "7": {"class_type": "VAEDecode", "inputs": {"samples": ["6", 0], "vae": ["1", 2]}},
        "8": {"class_type": "SaveImage", "inputs": {"images": ["7", 0], "filename_prefix": "cod3tex"}},
    }
    pid = json.loads(post(comfy + "/prompt", json.dumps({"prompt": graph}).encode()))["prompt_id"]
    while True:
        time.sleep(0.5)
        hist = json.loads(urllib.request.urlopen(f"{comfy}/history/{pid}", timeout=60).read())
        if pid in hist:
            entry = hist[pid]
            if entry.get("status", {}).get("status_str") == "error":
                raise RuntimeError(json.dumps(entry["status"])[:500])
            img = entry["outputs"]["8"]["images"][0]
            q = urllib.parse.urlencode({"filename": img["filename"], "subfolder": img["subfolder"], "type": img["type"]})
            return Image.open(io.BytesIO(urllib.request.urlopen(f"{comfy}/view?{q}", timeout=60).read())).convert("RGB")


def tiles(size, tile, overlap):
    if size <= tile:
        return [0]
    step = tile - overlap
    starts = list(range(0, size - tile, step)) + [size - tile]
    return sorted(set(starts))


def detail_upscale(comfy, checkpoint, src, denoise, detail, seed, two_from=1024):
    rgba = src.convert("RGBA")
    w, h = rgba.size
    scale = 2 if max(w, h) >= two_from else 4
    W, H = w * scale, h * scale
    up = rgba.resize((W, H), Image.LANCZOS)
    rgb = up.convert("RGB")
    T, O = 1024, 192
    # SDXL wants at least 1024 or so: smaller pictures go through padded by mirroring.
    pw, ph = max(W, T), max(H, T)
    base = np.asarray(rgb, dtype=np.float32)
    if (pw, ph) != (W, H):
        base_p = np.pad(base, ((0, ph - H), (0, pw - W), (0, 0)), mode="reflect")
    else:
        base_p = base
    acc = np.zeros_like(base_p); wsum = np.zeros(base_p.shape[:2] + (1,), dtype=np.float32)
    ramp = lambda n: np.minimum(1.0, np.minimum(np.arange(n) + 1, n - np.arange(n)) / O)
    for y in tiles(ph, T, O):
        for x in tiles(pw, T, O):
            tile = Image.fromarray(base_p[y:y + T, x:x + T].astype(np.uint8))
            out = np.asarray(run_tile(comfy, checkpoint, tile, denoise, seed), dtype=np.float32)
            wy, wx = ramp(out.shape[0]), ramp(out.shape[1])
            weight = (wy[:, None] * wx[None, :])[..., None]
            acc[y:y + out.shape[0], x:x + out.shape[1]] += out * weight
            wsum[y:y + out.shape[0], x:x + out.shape[1]] += weight
    sd = (acc / np.maximum(wsum, 1e-6))[:H, :W]
    # Only fine detail from SDXL, over the clean enlargement: its low pass
    # kept, and of the two high passes (a radius of a source texel or so)
    # the stronger at each pixel, so nothing comes out softer than the
    # enlargement and SDXL's grain, weave and scratches come in where it
    # drew more than was there.
    r = max(1.0, scale * 0.6)
    blur = lambda a: np.asarray(Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(r)), dtype=np.float32)
    low = blur(base)
    hpBase, hpSd = base - low, (sd - blur(sd)) * detail
    energy = lambda h: np.abs(h).sum(axis=2, keepdims=True)
    result = low + np.where(energy(hpSd) > energy(hpBase), hpSd, hpBase)
    out = Image.fromarray(np.clip(result + 0.5, 0, 255).astype(np.uint8)).convert("RGBA")
    out.putalpha(up.getchannel("A"))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True); ap.add_argument("--out", required=True)
    ap.add_argument("--keys", nargs="*", default=[]); ap.add_argument("--list")
    ap.add_argument("--comfy", default="http://127.0.0.1:8188")
    ap.add_argument("--checkpoint", default="sd_xl_base_1.0.safetensors")
    ap.add_argument("--denoise", type=float, default=0.3); ap.add_argument("--detail", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=1944)
    ap.add_argument("--two-from", type=int, default=1024, help="textures this wide or more enlarged 2x, smaller ones 4x")
    ap.add_argument("--rest-every", type=float, default=0, help="minutes of work before a rest (0: none)")
    ap.add_argument("--rest-for", type=float, default=15, help="minutes a rest lasts")
    ap.add_argument("--rest-clock", help="a file keeping when the last rest ended, so separate runs share the clock")
    a = ap.parse_args()
    clock = Path(a.rest_clock) if a.rest_clock else None
    def rest_check():
        if a.rest_every <= 0: return
        since = float(clock.read_text()) if clock and clock.exists() else rest_check.start
        if time.time() - since >= a.rest_every * 60:
            print(f"resting {a.rest_for:.0f} min from {time.strftime('%H:%M')}", flush=True)
            time.sleep(a.rest_for * 60)
            now = time.time(); rest_check.start = now
            if clock: clock.write_text(str(now))
    rest_check.start = time.time()
    if clock and not clock.exists(): clock.write_text(str(rest_check.start))
    keys = list(a.keys)
    if a.list:
        keys += [l.strip() for l in open(a.list) if l.strip() and not l.startswith("#")]
    os.makedirs(a.out, exist_ok=True)
    for key in keys:
        key = key[:-4] if key.endswith(".png") else key
        dest = Path(a.out) / f"{key}.png"
        if dest.exists():
            print(f"{key}: done already"); continue
        rest_check()
        t0 = time.time()
        img = detail_upscale(a.comfy, a.checkpoint, Image.open(Path(a.dump) / f"{key}.png"), a.denoise, a.detail, a.seed, a.two_from)
        img.save(dest)
        print(f"{key}: {img.size[0]}x{img.size[1]} in {time.time() - t0:.0f} s", flush=True)


if __name__ == "__main__":
    import urllib.parse
    main()
