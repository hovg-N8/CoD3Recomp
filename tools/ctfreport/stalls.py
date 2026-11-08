"""Bots stalled while going somewhere: two ctftrace samples 3 s apart, both 'to goal', under 30 apart."""
import sys, re, math, collections as C
prev = {}; total = 0; stuck = 0; cells = C.Counter(); roles = C.Counter()
for l in open(sys.argv[1], errors='replace'):
    if 'ctftrace' not in l: continue
    m = re.search(r'slot (\d+) side \d at (-?\d+) (-?\d+), .*?away: (.*?)( \(.*?\))?; think (.*?);', l)
    if not m: continue
    s = m.group(1); x, y = int(m.group(2)), int(m.group(3)); th = m.group(6)
    p = prev.get(s); prev[s] = (x, y, th)
    if not p or not th.startswith('to goal') or not p[2].startswith('to goal'): continue
    total += 1
    if math.hypot(x - p[0], y - p[1]) < 30:
        stuck += 1; cells[(x // 200 * 200, y // 200 * 200)] += 1; roles[m.group(4)] += 1
print(f'moving samples {total}, stalled {stuck} ({100 * stuck / max(1, total):.1f}%)')
print('roles', roles.most_common(5)); print('places', cells.most_common(8))
