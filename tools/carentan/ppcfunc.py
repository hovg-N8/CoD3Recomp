"""print the PPC comments of the function containing an address: func.py <dir> <hex addr> [max lines]"""
import sys, re, glob
d, addr = sys.argv[1], int(sys.argv[2], 16)
lim = int(sys.argv[3]) if len(sys.argv) > 3 else 200
best = None
for f in glob.glob(d + "/ppc_recomp.*.cpp"):
    for m in re.finditer(r"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\)", open(f, encoding="latin-1").read()):
        a = int(m.group(1), 16)
        if a <= addr and (best is None or a > best[0]):
            best = (a, f)
a, f = best
src = open(f, encoding="latin-1").read()
start = src.index(f"PPC_FUNC_IMPL(__imp__sub_{a:08X})")
end = src.find("PPC_FUNC_IMPL(", start + 10)
body = src[start:end]
print(f"sub_{a:08X} in {f.split('/')[-1]}")
n = 0
for line in body.splitlines():
    s = line.strip()
    if s.startswith("// ") or s.startswith("loc_"):
        print(s[3:] if s.startswith("// ") else s)
        n += 1
        if n >= lim: break
