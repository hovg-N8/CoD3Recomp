"""One CTF self-test summary: version, ending, attacker exits and reach, progress,
kills by role, route remakes, takes and captures. Usage: python -I ctfreport.py <log>"""
import re, sys, math, collections as C

path = sys.argv[1]
lines = open(path, errors='replace').readlines()
F = {'1': (-1792, -91), '2': (2112, 462)}
for l in lines:
    m = re.search(r'ctf sides .*side 1 owns the flag at (-?\d+)', l)
    if m:
        if int(m.group(1)) > 0: F = {'1': (2112, 462), '2': (-1792, -91)}
        break
ver = next((l.split('version')[-1].strip() for l in lines[:5] if 'static recompilation' in l), '?')
print('version', ver)

# lives
life = {}; out = []
def close(s, why):
    if s in life: o = life.pop(s); o['end'] = why; out.append(o)
role = {}
kills = C.Counter(); pair = C.Counter()
def rl(s):
    e = re.sub(r' \(.*', '', role.get(s, '?'))
    return {'goes for the enemy flag': 'attacker', 'waits for his squad': 'attacker', 'holds the middle': 'middle',
            'holds cover by his flag': 'cover', 'hunts the enemy carrying his flag': 'hunter'}.get(e, e)
prog = []; prev = {}
for l in lines:
    m = re.search(r'ctf: slot (\d+) \(side \d\) spawned at|slot (\d+) put at .* taken as a spawn', l)
    if m:
        s = next(g for g in m.groups() if g); close(s, 'respawn'); life[s] = {'n': 0, 'exit': None, 'side': None, 'min': 1e9}; continue
    m = re.search(r'death of bot (\d+) \(at once\)', l)
    if m: close(m.group(1), 'died'); continue
    m = re.search(r'player dies \(sub_82587268\): bot (\d+).s entity, killed by bot (\d+)', l)
    if m: pair[(rl(m.group(2)), rl(m.group(1)))] += 1
    m = re.search(r'ctftrace: slot (\d+) side (\d) at (-?\d+) (-?\d+), enemy flag (\d+) away: (.*?); think (.*?);', l)
    if m:
        s = m.group(1); role[s] = m.group(6)
        att = m.group(6).startswith('goes for the enemy flag') or m.group(6).startswith('waits') or m.group(6).startswith('regroups')
        if att and s in life:
            o = life[s]; o['n'] += 1; o['side'] = m.group(2); d = int(m.group(5)); o['min'] = min(o['min'], d)
            own = F[m.group(2)]; dh = math.hypot(int(m.group(3)) - own[0], int(m.group(4)) - own[1])
            if o['exit'] is None and dh > 1000: o['exit'] = o['n']
        if m.group(6).startswith('goes for the enemy flag') and m.group(7).startswith('to goal'):
            d = int(m.group(5))
            if s in prev and abs(prev[s] - d) < 1500 and prev[s] > 300: prog.append(prev[s] - d)
        prev[s] = int(m.group(5))
for s in list(life): close(s, 'end')
lives = [o for o in out if o['n'] >= 2]
for sd in '12':
    ls = [o for o in lives if o['side'] == sd]; ex = [o for o in ls if o['exit']]
    print(f'side {sd}: attacker lives {len(ls)}, left base {len(ex)}')
b = C.Counter(('<300' if o['min'] < 300 else '300-700' if o['min'] < 700 else '700-1500' if o['min'] < 1500 else '1500+') for o in lives)
print('closest to enemy flag per life:', sorted(b.items()))
if prog:
    p = sorted(prog); print(f'progress per 3 s: median {p[len(p)//2]}, stalled {sum(1 for x in p if abs(x) < 50)} of {len(p)}')
print('killer -> victim:', pair.most_common(6))
rr = [l for l in lines if 'routes worked out again' in l]
if rr: print(rr[-1].strip()[6:])
state = {}; caps = rets = 0
for l in lines:
    m = re.search(r'flagwatch: flag (\d) (\(.*?\) TAKEN|dropped|home)', l)
    if not m: continue
    k, what = m.group(1), m.group(2)
    if what.endswith('TAKEN'): state[k] = 'carried'
    elif what == 'dropped': state[k] = 'dropped'
    else:
        if state.get(k) == 'carried': caps += 1
        elif state.get(k) == 'dropped': rets += 1
        state[k] = 'home'
print('takes:', sum(1 for l in lines if 'TAKEN by slot' in l and ', bot,' in l), 'by bots,',
      sum(1 for l in lines if 'TAKEN by slot 0 ' in l), 'by the host;',
      'captures:', caps, '(flag home straight from carried),', rets, 'returns')
for l in lines:
    if 'TAKEN by' in l or 'has the flag' in l or 'stuck' in l and 'carrier' in l: print('  ', l.strip()[:140])
crash = [l for l in lines if 'watchdog: no swap' in l or 'crash' in l.lower() and 'guest' in l.lower()]
print('watchdog/crash lines:', len(crash))
