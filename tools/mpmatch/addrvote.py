# Finds the multiplayer twin of a single player DATA address (0.5.478, Claude).
# Usage: python tools/mpmatch/addrvote.py CoD3RecompLib 82A8DF2C
# Every SP function that builds the address (lis + addi / load offset) and has
# exactly one MP twin with the same normalized code votes for the address its
# twin builds at the same place. 82A8DF2C -> 82C6B1FC: 25 of 25 votes.
import re,glob,os,sys,collections
def load(d):
    out={}
    for f in glob.glob(os.path.join(d,'ppc_recomp.*.cpp')):
        cur=None;buf=[]
        for line in open(f,encoding='utf-8',errors='replace'):
            m=re.match(r'PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\)',line)
            if m:
                if cur: out[cur]=buf
                cur=m.group(1);buf=[];continue
            if cur and not line.strip().startswith('//'): buf.append(line.strip())
        if cur: out[cur]=buf
    return out
def addrs(lines):
    hi={};res=[]
    for l in lines:
        m=re.match(r'ctx\.(r\d+)\.s64 = (-?\d+);$',l)
        if m: hi[m.group(1)]=int(m.group(2))&0xffffffff; continue
        m=re.search(r'ctx\.(r\d+)\.s64 = ctx\.(r\d+)\.s64 \+ (-?\d+);',l) or re.search(r'ctx\.(r\d+)\.u32 \+ (-?\d+)\)',l)
        if m:
            if len(m.groups())==3: r=m.group(2);lo=int(m.group(3))
            else: r=m.group(1);lo=int(m.group(2))
            if r in hi and 0x82000000<=hi[r]<0x83000000: res.append((hi[r]+lo)&0xffffffff)
    return res
def norm(lines):
    o=[]
    for l in lines:
        l=re.sub(r'sub_[0-9A-F]{8}','S',l);l=re.sub(r'loc_[0-9A-F]+','L',l);l=re.sub(r'0x[0-9A-Fa-f]{8}','A',l);l=re.sub(r'-?\d{3,}','N',l);o.append(l)
    return tuple(o)
d=sys.argv[1]; want=int(sys.argv[2],16)
sp=load(os.path.join(d,'ppc')); mp=load(os.path.join(d,'ppc_mp'))
mpidx=collections.defaultdict(list)
for k,v in mp.items(): mpidx[norm(v)].append(k)
votes=collections.Counter(); used=0
for k,v in sp.items():
    a=addrs(v)
    if want not in a: continue
    twins=mpidx.get(norm(v),[])
    if len(twins)!=1: continue
    b=addrs(mp[twins[0]])
    if len(a)!=len(b): continue
    for x,y in zip(a,b):
        if x==want: votes[y]+=1
    used+=1
print(hex(want),'functions matched',used,'votes',[(hex(a),n) for a,n in votes.most_common(4)])
