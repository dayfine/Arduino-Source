import re,os,sys
from datetime import datetime
from collections import defaultdict
base=os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
ts=lambda l: datetime.strptime(l[:26],"%Y-%m-%d %H:%M:%S.%f")
d=defaultdict(list); comp=0; span=0
for prof in sys.argv[1:]:
    start=None; dest=None; first=last=None
    for line in open(f"{base}/{prof}/SerialPrograms.log",errors="replace"):
        if len(line)<27 or line[4]!='-': continue
        try: t=ts(line)
        except: continue
        first=first or t; last=t
        m=re.search(r"Attempting to fly to (.*)\.",line)
        if m: start=t; dest=m.group(1); continue
        if "Map opened" in line or "open_map_from_overworld" in line:
            if start is None: start=t; dest="Central Plaza (return)"
        if start and "Fly successful" in line:
            s=(t-start).total_seconds()
            if s<90: d[dest].append(s)
            start=None
        if "Quest completed successfully" in line: comp+=1
    span+=(last-first).total_seconds()
tot=0
for k,v in sorted(d.items(),key=lambda kv:-sum(kv[1])):
    tot+=sum(v); print(f"{k:28} n={len(v):3} mean={sum(v)/len(v):5.1f}s total={sum(v)/60:5.1f}min")
print(f"all flights {tot/60:.1f} min of {span/60:.0f} min run = {100*tot/span:.0f}%; per completion {tot/max(1,comp):.0f}s")
