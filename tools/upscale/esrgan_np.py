# Real-ESRGAN x4plus (RRDBNet) in plain numpy: no torch in this workspace.
# Usage: python3 esrgan_np.py model.pth in.png out.png [tile]
import sys, zipfile, pickle, io, time
import numpy as np
import cv2

def load_pth(path):
    z = zipfile.ZipFile(path)
    names = z.namelist()
    root = names[0].split('/')[0]
    class Storage:
        def __init__(self, key, dtype): self.key, self.dtype = key, dtype
    def rebuild(storage, offset, size, stride, *a):
        raw = z.read(f'{root}/data/{storage.key}')
        arr = np.frombuffer(raw, dtype=storage.dtype)
        n = int(np.prod(size)) if size else 1
        itemsize = arr.itemsize
        return np.lib.stride_tricks.as_strided(arr[offset:], shape=tuple(size), strides=tuple(s * itemsize for s in stride)).copy() if n else np.zeros(size, storage.dtype)
    class U(pickle.Unpickler):
        def find_class(self, mod, name):
            if name == '_rebuild_tensor_v2': return rebuild
            if name == 'OrderedDict': import collections; return collections.OrderedDict
            if name.endswith('Storage'):
                return {'FloatStorage': np.float32, 'HalfStorage': np.float16, 'LongStorage': np.int64}.get(name, np.float32)
            return super().find_class(mod, name)
        def persistent_load(self, pid):
            _, dtype, key, loc, n = pid
            return Storage(key, dtype)
    d = U(io.BytesIO(z.read(f'{root}/data.pkl'))).load()
    if 'params_ema' in d: d = d['params_ema']
    elif 'params' in d: d = d['params']
    return {k: np.ascontiguousarray(v, dtype=np.float32) for k, v in d.items()}

class Net:
    def __init__(self, w):
        self.w = {}
        for k in w:
            if k.endswith('.weight'):
                base = k[:-7]; W = w[k]; co, ci, kh, kw = W.shape
                # (kh*kw*ci, co) matching the im2col order below
                self.w[base] = (W.transpose(2, 3, 1, 0).reshape(kh * kw * ci, co).copy(), w[base + '.bias'])
        self.blocks = 1 + max(int(k.split('.')[1]) for k in w if k.startswith('body.'))

    def conv(self, name, x, act=False):
        W, b = self.w[name]
        h, wd, c = x.shape
        p = np.pad(x, ((1, 1), (1, 1), (0, 0)), mode='constant')
        cols = np.empty((h, wd, 9, c), np.float32)
        i = 0
        for dy in range(3):
            for dx in range(3):
                cols[:, :, i, :] = p[dy:dy + h, dx:dx + wd, :]; i += 1
        y = cols.reshape(h * wd, 9 * c) @ W + b
        y = y.reshape(h, wd, -1)
        if act: np.maximum(y, 0.2 * y, out=y)
        return y

    def rdb(self, pre, x):
        x1 = self.conv(pre + '.conv1', x, True)
        x2 = self.conv(pre + '.conv2', np.concatenate([x, x1], 2), True)
        x3 = self.conv(pre + '.conv3', np.concatenate([x, x1, x2], 2), True)
        x4 = self.conv(pre + '.conv4', np.concatenate([x, x1, x2, x3], 2), True)
        x5 = self.conv(pre + '.conv5', np.concatenate([x, x1, x2, x3, x4], 2))
        return x5 * 0.2 + x

    def forward(self, img):   # HxWx3 float RGB 0..1
        f = self.conv('conv_first', img)
        x = f
        for n in range(self.blocks):
            y = self.rdb(f'body.{n}.rdb1', x); y = self.rdb(f'body.{n}.rdb2', y); y = self.rdb(f'body.{n}.rdb3', y)
            x = y * 0.2 + x
        f = f + self.conv('conv_body', x)
        f = self.conv('conv_up1', f.repeat(2, 0).repeat(2, 1), True)
        f = self.conv('conv_up2', f.repeat(2, 0).repeat(2, 1), True)
        f = self.conv('conv_hr', f, True)
        return self.conv('conv_last', f)

def upscale(net, rgb, tile=96, pad=10):
    h, w, _ = rgb.shape
    out = np.zeros((h * 4, w * 4, 3), np.float32)
    for y0 in range(0, h, tile):
        for x0 in range(0, w, tile):
            y1, x1 = min(y0 + tile, h), min(x0 + tile, w)
            ya, xa = max(0, y0 - pad), max(0, x0 - pad)
            yb, xb = min(h, y1 + pad), min(w, x1 + pad)
            r = net.forward(rgb[ya:yb, xa:xb])
            out[y0 * 4:y1 * 4, x0 * 4:x1 * 4] = r[(y0 - ya) * 4:(y0 - ya + y1 - y0) * 4, (x0 - xa) * 4:(x0 - xa + x1 - x0) * 4]
    return np.clip(out, 0, 1)

def main():
    model, src, dst = sys.argv[1:4]
    tile = int(sys.argv[4]) if len(sys.argv) > 4 else 96
    t = time.time()
    net = Net(load_pth(model))
    im = cv2.imread(src, cv2.IMREAD_UNCHANGED)
    if im.ndim == 2: im = cv2.cvtColor(im, cv2.COLOR_GRAY2BGRA)
    if im.shape[2] == 3: im = cv2.cvtColor(im, cv2.COLOR_BGR2BGRA)
    bgr = im[:, :, :3].astype(np.float32) / 255.0
    alpha = im[:, :, 3]
    # Colour under fully clear pixels spread from their neighbours first, so
    # the upscaler doesn't pull black fringes into edges.
    if alpha.min() < 255 and alpha.max() > 0:
        mask = (alpha == 0).astype(np.uint8)
        bgr = cv2.inpaint((bgr * 255).astype(np.uint8), mask, 3, cv2.INPAINT_TELEA).astype(np.float32) / 255.0
    rgb = bgr[:, :, ::-1].copy()
    up = upscale(net, rgb, tile)[:, :, ::-1]
    h, w = alpha.shape
    a = cv2.resize(alpha, (w * 4, h * 4), interpolation=cv2.INTER_CUBIC)
    outim = np.dstack([(up * 255 + 0.5).astype(np.uint8), a])
    cv2.imwrite(dst, outim)
    print(f'{src} -> {dst} {w}x{h} -> {w*4}x{h*4} in {time.time()-t:.0f}s')

if __name__ == '__main__':
    main()
