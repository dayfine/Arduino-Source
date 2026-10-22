# Per-quest-route success rate and attempt time across BBQ lab profile logs.
import glob, os, re, sys
from datetime import datetime
from collections import defaultdict
base=os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
ts=lambda l: datetime.strptime(l[:26],"%Y-%m-%d %H:%M:%S.%f")
stats=defaultdict(lambda: {"n":0,"ok":0,"secs":0.0,"prof":set()})
route_pat=re.compile(r"\]: ((Quest: [^.]+)|(Catch: \S+)|(Photo: \S+)|(Tera raid[^.]*)|(Hatch[^.]*))")
for prof in sorted(glob.glob(base+"/bbq-*")):
    f=prof+"/SerialPrograms.log"
    if not os.path.exists(f): continue
    p=os.path.basename(prof)
    start=None; route=None
    for line in open(f,errors="replace"):
        if len(line)<27 or line[4]!='-': continue
        if "Current quest exists on list. Doing quest." in line:
            start=ts(line); route=None; continue
        if start and route is None:
            m=route_pat.search(line)
            if m: route=re.sub(r"\d+/\d+","N",m.group(1)).strip()
        if start and ("Quest completed!" in line or "Quest did not complete" in line or "Current quest was not found" in line):
            r=route or "?"
            s=stats[r]; s["n"]+=1; s["prof"].add(p)
            if "completed!" in line or "not found" in line: s["ok"]+=1
            s["secs"]+=(ts(line)-start).total_seconds()
            start=None
rows=sorted(stats.items(), key=lambda kv:-kv[1]["n"])
print(f"{'route':42} {'att':>4} {'ok':>4} {'rate':>5} {'avg s':>6} {'s per success':>13}")
for r,s in rows:
    rate=s["ok"]/s["n"]; avg=s["secs"]/s["n"]
    sps=s["secs"]/s["ok"] if s["ok"] else float('inf')
    print(f"{r[:42]:42} {s['n']:4} {s['ok']:4} {rate:5.0%} {avg:6.0f} {sps:13.0f}")
