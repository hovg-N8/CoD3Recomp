"""CoD2's own first-person view of an animation, drawn from the user's CoD2
install: the hands model and the gun model posed at a frame and seen from
tag_view (CoD2's eye: x forward, y left, z up), flat shaded, the gun with its
colour map. For comparing a port's version with CoD2's (0.5.677: the
Webley's reload, its left hand and fingers).

usage:
  python -I cod2view.py <cod2 main> <anim> <hands model> <gun model> <out.png>
                        <frames, comma separated> [fov degrees across, 80]
e.g. cod2view.py ".../Call of Duty 2/main" viewmodel_webley_reload
     viewmodel_hands_cloth viewmodel_webley view.png 18,36,54,72,90,108
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cod2import as c  # noqa: E402
import xanim  # noqa: E402


def model_parts(arch, model):
    lods = c.read_xmodel(arch.read("xmodel/" + model))
    part = lods[0]["name"]
    return part, lods[0]["materials"]


def posed_triangles(arch, model, world):
    """The model's triangles, its vertices placed by the bones' world
    transforms (world: {name: (pos, quat)}), with each vertex's normal."""
    part, _materials = model_parts(arch, model)
    bones = c.read_parts(arch.read("xmodelparts/" + part))
    for b in bones:
        hit = world.get(b["name"])
        if hit:
            b["wpos"], b["wrot"] = hit
    tris = []
    for verts, faces in c.read_surfaces(arch.read("xmodelsurfs/" + part), bones):
        for a, b_, cc in faces:
            tris.append([verts[a], verts[b_], verts[cc]])
    return tris


def main():
    from PIL import Image, ImageDraw
    main_dir, anim_name, hands, gun, out = sys.argv[1:6]
    frames = [int(x) for x in sys.argv[6].split(",")]
    fov = float(sys.argv[7]) if len(sys.argv) > 7 else 80.0
    arch = c.Archives(main_dir)
    anim = xanim.read_xanim(arch.read("xanim/" + anim_name))
    skel = xanim.skeleton(arch, hands, gun)
    w, h = 480, 270
    focal = (w / 2) / math.tan(math.radians(fov) / 2)
    img = Image.new("RGB", (w * min(4, len(frames)), h * ((len(frames) + 3) // 4)), (40, 44, 52))
    d = ImageDraw.Draw(img)
    light = (0.4, 0.3, 0.85)
    for k, f in enumerate(frames):
        world = xanim.pose(skel, anim, f)
        cam_p, cam_q = world["tag_view"]
        ox, oy = (k % 4) * w, (k // 4) * h
        items = []
        for model, colour in ((hands, (196, 150, 120)), (gun, (120, 125, 140))):
            for tri in posed_triangles(arch, model, {n: (p, q) for n, (p, q) in world.items()}):
                pts, ok = [], True
                for p, _n, _uv, _b in tri:
                    v = c.quat_rotate((p[0] - cam_p[0], p[1] - cam_p[1], p[2] - cam_p[2]), (cam_q[0], -cam_q[1], -cam_q[2], -cam_q[3]))
                    if v[0] < 0.5:
                        ok = False
                        break
                    pts.append((ox + w / 2 - v[1] / v[0] * focal, oy + h / 2 - v[2] / v[0] * focal, v[0]))
                if not ok:
                    continue
                n = tri[0][1]
                shade = 0.35 + 0.65 * abs(n[0] * light[0] + n[1] * light[1] + n[2] * light[2])
                items.append((sum(p[2] for p in pts) / 3, [(p[0], p[1]) for p in pts], tuple(int(x * shade) for x in colour)))
        for _depth, pts, col in sorted(items, key=lambda t: -t[0]):
            d.polygon(pts, fill=col)
        d.text((ox + 6, oy + 4), f"CoD2 frame {f}", fill=(230, 230, 230))
    img.save(out)
    print(f"{out}: {len(frames)} frames of {anim_name} from CoD2's eye ({fov:.0f} degrees across)")


if __name__ == "__main__":
    main()
