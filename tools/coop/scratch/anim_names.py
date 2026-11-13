import re, sys, glob, os
root = sys.argv[1]
want = {0x92A23EE1: 'STAND_IDLE', 0x444ED990: 'WALK_F', 0xF4C043BB: 'WALK_B', 0xF4C5D0F5: 'WALK_L', 0x055BB472: 'WALK_R', 0x4DE531AF: 'RUN_F',
        0x7CDAACCD: 'CROUCH_IDLE', 0x6F035656: 'CROUCH_F', 0xE92CB727: 'PRONE_IDLE', 0x022B240F: 'node6', 0x392E06C1: 'node189', 0x2EED460B: 'node222',
        0x0037A2AE: 'node2', 0x87618FBA: 'node1', 0x00406A24: 'root'}
def h(t):
    x = 0
    for c in t.lower().encode('latin1'):
        x = (x * 33 + c) & 0xFFFFFFFF
    return x
names = set()
files = [os.path.join(root, 'anim.cod')] + glob.glob(os.path.join(root, 'forest', '*.cod')) + [os.path.join(root, 'global.cod')]
for f in files:
    try: data = open(f, 'rb').read()
    except OSError: continue
    found = set(m.group().decode('latin1') for m in re.finditer(rb'[A-Za-z0-9_]{3,64}', data))
    print(os.path.basename(f), len(data), 'bytes,', len(found), 'words')
    names |= found
hit = {}
for n in names:
    v = h(n)
    if v in want: hit[want[v]] = n
print('matches:', hit)
out = sys.argv[2]
with open(out, 'w') as o:
    for n in sorted(names, key=str.lower):
        o.write('%08X %s\n' % (h(n), n))
print(len(names), 'words written')
