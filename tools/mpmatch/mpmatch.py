# Finds the multiplayer twin of a single player function (0.5.477, Claude).
# Usage: python tools/mpmatch/mpmatch.py CoD3RecompLib 82420F48 [more SP addresses]
# Reads the generated CoD3RecompLib/ppc and ppc_mp sources (never commit those),
# masks call/branch/address constants and compares the functions line by line:
# an exact twin, or the three closest (0.87 for 82420F48 -> 82438DF0, the same
# code scheduled otherwise; the runner-up at 0.17 shows the match is sure).
import re,glob,sys,os
def funcs(d):
    out={}
    for f in glob.glob(os.path.join(d,'ppc_recomp.*.cpp')):
        cur=None;buf=[]
        for line in open(f,encoding='utf-8',errors='replace'):
            m=re.match(r'PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\)',line)
            if m:
                if cur: out[cur]=buf
                cur=m.group(1);buf=[];continue
            if cur: buf.append(line.strip())
        if cur: out[cur]=buf
    return out
def norm(lines):
    r=[]
    for l in lines:
        if l.startswith('//') or not l: continue
        l=re.sub(r'sub_[0-9A-F]{8}','SUB',l); l=re.sub(r'loc_[0-9A-F]+','LOC',l)
        l=re.sub(r'0x[0-9A-Fa-f]{8}','ADDR',l)
        l=re.sub(r'= -?2\d{9};','= HI;',l)
        r.append(l)
    return r
sp=funcs(os.path.join(sys.argv[1],'ppc')); mp=funcs(os.path.join(sys.argv[1],'ppc_mp'))
for want in sys.argv[2:]:
    a=norm(sp[want]); key=tuple(a)
    exact=[k for k,v in mp.items() if len(v)==len(sp[want]) and tuple(norm(v))==key]
    print(want,'lines',len(a),'exact MP matches:',exact)
    if not exact:
        import difflib
        cands=sorted(mp, key=lambda k: abs(len(mp[k])-len(sp[want])))[:400]
        best=sorted(((difflib.SequenceMatcher(None,a,norm(mp[k])).ratio(),k) for k in cands),reverse=True)[:3]
        print('  closest',best)
