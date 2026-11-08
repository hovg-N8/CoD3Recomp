"""Call of Duty 2's placed models (misc_model, and the intact S&D target
vehicles) as extra drawn surfaces of the level, for cod2cells.py: CoD2's
compiler leaves them out of the drawn surfaces (the game draws them as
models), so in CoD3 they would be missing while CoD2's clip brushes around
them (clip, clip_nosight, clip_player: in the collision already) stand as
invisible walls. Each placement becomes surfaces in CoD2's own 68-byte
vertex layout (xyz, normal, u32 colour, uv, lightmap uv, tangent,
binormal), so the rest of the converter treats them as world surfaces.

Placement: CoD2's entity origin, angles (pitch yaw roll, Quake's
AnglesToAxis: forward, left, up) and modelscale; the model's top detail
level (cod2import.read_xmodel / read_parts / read_surfaces: bones applied).
Light: CoD2 lights models from its light grid, not lightmaps; here each
placement takes one world lightmap texel: of the drawn, lightmapped
surfaces under its origin and under eight points on a ring at its radius,
the one with the most sun, then the most ambient (a tree's own shade under
it would darken its sunlit crown).
"""
import math
import re
import struct
from collections import defaultdict

import cod2import

LM_PLANE = 512 * 512 * 4
LM_SIZE = 3 * LM_PLANE + 1024 * 1024


def axis(angles):
    p, y, r = (math.radians(a) for a in angles)
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    forward = (cp * cy, cp * sy, -sp)
    right = (-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp)
    up = (cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp)
    return forward, tuple(-c for c in right), up


def placements(entity_text):
    out = []
    for e in re.findall(r"\{[^{}]*\}", entity_text):
        kv = dict(re.findall(r'"([^"]*)" "([^"]*)"', e))
        cls, model = kv.get("classname"), kv.get("model", "")
        keep = cls == "misc_model" or (cls == "script_model" and kv.get("script_gameobjectname") == "bombzone"
                                       and model.endswith("_static") and "_d_" not in model and not model.endswith("_d_static"))
        if keep and model.startswith("xmodel/"):
            out.append((model, tuple(float(v) for v in kv["origin"].split()),
                        tuple(float(v) for v in kv.get("angles", "0 0 0").split()), float(kv.get("modelscale", "1"))))
    return out


class GroundLight:
    """The drawn, lightmapped surfaces in 128-unit columns: (lightmap, uv) of the
    highest one under a point."""

    def __init__(self, soups, verts, indices, lm):
        self.verts, self.lm, self.cols = verts, lm, defaultdict(list)
        for mat, lmi, fv, _vc, ic, fi in soups:
            if lmi >= 31:
                continue
            for t in range(fi, fi + ic, 3):
                ids = [fv + indices[t + k] for k in range(3)]
                P = [struct.unpack_from("<3f", verts, 68 * i) for i in ids]
                for cx in range(int(min(p[0] for p in P) // 128), int(max(p[0] for p in P) // 128) + 1):
                    for cy in range(int(min(p[1] for p in P) // 128), int(max(p[1] for p in P) // 128) + 1):
                        self.cols[(cx, cy)].append((lmi, ids, P))

    def under(self, x, y, z):
        best = None
        for lmi, ids, (A, B, C) in self.cols.get((int(x // 128), int(y // 128)), ()):
            den = (B[1] - C[1]) * (A[0] - C[0]) + (C[0] - B[0]) * (A[1] - C[1])
            if abs(den) < 1e-9:
                continue
            l1 = ((B[1] - C[1]) * (x - C[0]) + (C[0] - B[0]) * (y - C[1])) / den
            l2 = ((C[1] - A[1]) * (x - C[0]) + (A[0] - C[0]) * (y - C[1])) / den
            l3 = 1 - l1 - l2
            if min(l1, l2, l3) < -1e-6:
                continue
            h = l1 * A[2] + l2 * B[2] + l3 * C[2]
            if h > z or (best and h <= best[0]):
                continue
            uv = [sum(w * struct.unpack_from("<2f", self.verts, 68 * i + 36)[k] for w, i in zip((l1, l2, l3), ids))
                  for k in range(2)]
            best = (h, lmi, uv)
        return best

    def light(self, lmi, uv):
        x, y = min(511, max(0, int(uv[0] * 512))), min(511, max(0, int(uv[1] * 512)))
        i = y * 512 + x
        base = lmi * LM_SIZE
        ambient = sum(sum(self.lm[base + c * LM_PLANE + 4 * i:base + c * LM_PLANE + 4 * i + 4]) for c in range(3))
        sun = self.lm[base + 3 * LM_PLANE + (2 * y) * 1024 + 2 * x]
        return sun, ambient


def tangents(positions, uvs, tris):
    """Per-vertex tangent and binormal from the uv derivatives (Lengyel)."""
    tan = [[0.0] * 3 for _ in positions]
    bit = [[0.0] * 3 for _ in positions]
    for a, b, c in tris:
        p0, p1, p2 = positions[a], positions[b], positions[c]
        u0, u1, u2 = uvs[a], uvs[b], uvs[c]
        e1 = [p1[k] - p0[k] for k in range(3)]
        e2 = [p2[k] - p0[k] for k in range(3)]
        s1, t1, s2, t2 = u1[0] - u0[0], u1[1] - u0[1], u2[0] - u0[0], u2[1] - u0[1]
        det = s1 * t2 - s2 * t1
        if abs(det) < 1e-12:
            continue
        f = 1.0 / det
        sd = [f * (t2 * e1[k] - t1 * e2[k]) for k in range(3)]
        td = [f * (s1 * e2[k] - s2 * e1[k]) for k in range(3)]
        for v in (a, b, c):
            for k in range(3):
                tan[v][k] += sd[k]
                bit[v][k] += td[k]

    def unit(v):
        n = math.sqrt(sum(x * x for x in v)) or 1.0
        return tuple(x / n for x in v)
    return [unit(t) for t in tan], [unit(b) for b in bit]


def add_models(arch, entity_text, mats, soups, verts, indices, lm, log=print):
    """(mats, soups, verts, indices) with the placed models' surfaces added;
    their materials appended to mats as (name, 0, 0)."""
    mats, soups, verts, indices = list(mats), list(soups), bytearray(verts), list(indices)
    ground = GroundLight(soups, verts, indices, lm)
    mat_index = {m[0]: k for k, m in enumerate(mats)}
    cache = {}
    placed, tri_total, unlit = 0, 0, 0
    for model, origin, angles, scale in placements(entity_text):
        if model not in cache:
            lods = cod2import.read_xmodel(arch.read(model))
            bones = cod2import.read_parts(arch.read("xmodelparts/" + lods[0]["name"]))
            surfaces = cod2import.read_surfaces(arch.read("xmodelsurfs/" + lods[0]["name"]), bones)
            radius = max((math.hypot(v[0][0], v[0][1]) for s in surfaces for v in s[0]), default=16.0)
            cache[model] = (surfaces, lods[0]["materials"], radius)
        surfaces, materials, radius = cache[model]
        fwd, left, up = axis(angles)
        # light: the brightest lightmap texel among the ground under the origin and a ring around it
        best = None
        for k in range(9):
            if k == 0:
                x, y = origin[0], origin[1]
            else:
                a = (k - 1) * math.pi / 4
                x, y = origin[0] + (radius * scale + 32) * math.cos(a), origin[1] + (radius * scale + 32) * math.sin(a)
            g = ground.under(x, y, origin[2] + 24)
            if g:
                key = ground.light(g[1], g[2])
                if best is None or key > best[0]:
                    best = (key, g[1], g[2])
        lmi, lmuv = (best[1], best[2]) if best else (31, (0.0, 0.0))
        unlit += best is None
        for s, (vs, tris) in enumerate(surfaces):
            name = materials[s] if s < len(materials) else materials[-1]
            if name not in mat_index:
                mat_index[name] = len(mats)
                mats.append((name, 0, 0))
            pos, nrm, uvs = [], [], []
            for p, n, uv, _bone in vs:
                w = [origin[k] + scale * (p[0] * fwd[k] + p[1] * left[k] + p[2] * up[k]) for k in range(3)]
                q = [n[0] * fwd[k] + n[1] * left[k] + n[2] * up[k] for k in range(3)]
                pos.append(w)
                nrm.append(q)
                uvs.append(uv)
            tan, bit = tangents(pos, uvs, tris)
            first_vertex = len(verts) // 68
            for k in range(len(vs)):
                verts += struct.pack("<3f3fI2f2f3f3f", *pos[k], *nrm[k], 0xFFFFFFFF, *uvs[k], *lmuv, *tan[k], *bit[k])
            first_index = len(indices)
            for t in tris:
                indices.extend(t)
            soups.append((mat_index[name], lmi, first_vertex, len(vs), 3 * len(tris), first_index))
            tri_total += len(tris)
        placed += 1
    log(f"placed models: {placed} placements of {len(cache)} models, {tri_total} triangles, "
        f"{len(mats)} materials in all, {unlit} with no ground light")
    return mats, soups, bytes(verts), indices
