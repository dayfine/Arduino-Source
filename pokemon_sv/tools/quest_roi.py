import re,os,glob,sys
from datetime import datetime
from collections import defaultdict
base=os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
ts=lambda l: datetime.strptime(l[:26],"%Y-%m-%d %H:%M:%S.%f")
route_pat=re.compile(r"\]: ((Quest: [^.\n]+)|(Catch: [^\n]+)|(Photo: [^\n]+))")
# era: camera off = R10 after 13:16 and later profiles
CAMOFF=datetime(2026,10,23,13,16)
st={"before":defaultdict(lambda:[0,0,0.0]),"after":defaultdict(lambda:[0,0,0.0])}
for prof in sorted(glob.glob(base+"/bbq-*")):
    f=prof+"/SerialPrograms.log"
    if not os.path.exists(f): continue
    cur=None;start=None
    for line in open(f,errors="replace"):
        if len(line)<27 or line[4]!='-': continue
        try: t=ts(line)
        except: continue
        m=route_pat.search(line)
        if m:
            r=re.sub(r"Auto Battle.*","Auto Battle",m.group(1).strip())
            if cur!=r or start is None: cur=r; start=t
        if cur and ("Quest completed" in line or "Quest did not complete" in line or "Current quest was not found" in line):
            era="after" if t>=CAMOFF else "before"
            s=st[era][cur]; s[0]+=1; s[2]+=(t-start).total_seconds()
            if "not complete" not in line: s[1]+=1
            cur=None; start=None
rows=set(st["before"])|set(st["after"])
def fmt(s):
    n,ok,sec=s
    if not n: return "-"
    return f"{ok}/{n} {sec/n:.0f}s/att"+(f" {sec/ok:.0f}s/ok" if ok else "")
for r in sorted(rows,key=lambda r:-(st['before'][r][0]+st['after'][r][0])):
    print(f"{r[:40]:40} | before: {fmt(st['before'][r]):28} | after: {fmt(st['after'][r])}")
