"""World at War's Seelow mantle helpers (brushes of contents 0x01000000:
mount_mantle_over, mantle_on/over, kept out of the collision) as boxes for
the host's mantle (CoD3Host/movement_fixes.cpp, 0.5.729): within one of
these boxes a player's step up reaches WaW's mantle height, so the ledges
and walls WaW lets players climb can be climbed. Maps with no file: CoD3's
own movement, unchanged.

usage: python -I seelow_mantle.py <mp_seelow.zone> <out .txt>
  (the install's custom\\mantle\\seelow.txt)
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import seelow_clipmap   # noqa: E402


def main():
    z = open(sys.argv[1], "rb").read()
    seelow_clipmap.read(z)
    boxes = seelow_clipmap.MANTLE
    assert boxes, "no mantle helpers found"
    out = Path(sys.argv[2])
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", newline="\n") as f:
        f.write(f"CoD3 mantle 1 {len(boxes)}\n")
        for lo, hi in boxes:
            f.write(" ".join(f"{v:.1f}" for v in (*lo, *hi)) + "\n")
    heights = sorted(hi[2] - lo[2] for lo, hi in boxes)
    print(f"{len(boxes)} mantle boxes -> {out}; box heights {heights[0]:.0f}..{heights[-1]:.0f} "
          f"(median {heights[len(heights) // 2]:.0f})")


if __name__ == "__main__":
    main()
