"""LZO1X decompression (pure Python), after the Linux kernel's
lzo1x_decompress_safe.c. Used by kapf.py for CoD3's .cod archives."""


def decompress(src, out_size=None):
    ip, n = 0, len(src)
    out = bytearray()
    state = 0

    def literals(count):
        nonlocal ip
        if ip + count > n:
            raise ValueError("lzo: literal run past the input")
        out.extend(src[ip:ip + count])
        ip += count

    def extended(t, base):
        nonlocal ip
        while src[ip] == 0:
            t += 255
            ip += 1
        t += base + src[ip]
        ip += 1
        return t

    def copy_match(dist_back, length):
        start = len(out) - dist_back
        if start < 0:
            raise ValueError("lzo: match before the output's start")
        if dist_back >= length:
            out.extend(out[start:start + length])
        else:
            for k in range(length):
                out.append(out[start + k])

    t = src[0]
    nxt = 0
    if t > 17:
        ip = 1
        t -= 17
        if t < 4:
            nxt = t
            literals(nxt)
            state = nxt
        else:
            literals(t)
            state = 4
    while True:
        t = src[ip]
        ip += 1
        if t < 16:
            if state == 0:
                if t == 0:
                    t = extended(t, 15)
                literals(t + 3)
                state = 4
                continue
            elif state != 4:
                nxt = t & 3
                dist = 1 + (t >> 2) + (src[ip] << 2)
                ip += 1
                copy_match(dist, 2)
            else:
                nxt = t & 3
                dist = 1 + 0x0800 + (t >> 2) + (src[ip] << 2)
                ip += 1
                copy_match(dist, 3)
        elif t >= 64:
            nxt = t & 3
            dist = 1 + ((t >> 2) & 7) + (src[ip] << 3)
            ip += 1
            copy_match(dist, (t >> 5) + 1)
        elif t >= 32:
            t = (t & 31) + 2
            if t == 2:
                t = extended(t, 31)
            word = src[ip] | (src[ip + 1] << 8)
            ip += 2
            copy_match(1 + (word >> 2), t)
            nxt = word & 3
        else:
            back = (t & 8) << 11
            t = (t & 7) + 2
            if t == 2:
                t = extended(t, 7)
            word = src[ip] | (src[ip + 1] << 8)
            ip += 2
            back += word >> 2
            nxt = word & 3
            if back == 0:
                break   # the end marker
            copy_match(back + 0x4000, t)
        state = nxt
        if nxt:
            literals(nxt)
    if out_size is not None and len(out) != out_size:
        raise ValueError(f"lzo: {len(out)} bytes out, {out_size} expected")
    return bytes(out)
