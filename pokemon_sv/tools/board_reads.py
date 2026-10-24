import re,os,sys
from datetime import datetime
base=os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
ts=lambda l: datetime.strptime(l[:26],"%Y-%m-%d %H:%M:%S.%f")
for prof in sys.argv[1:]:
    reads=[];start=None;lastm=None;comp=0;why={}
    prev=None
    for line in open(f"{base}/{prof}/SerialPrograms.log",errors="replace"):
        if len(line)<27 or line[4]!='-': continue
        try: t=ts(line)
        except: continue
        if re.search(r"Reading quest panel|Reading quests|read_quests\(\)",line):
            if start and lastm: reads.append((lastm-start).total_seconds())
            start=t; lastm=None
            m=re.search(r"Reading quest panel ?([^.]*)",line); k=(m.group(1) if m else "other") or "plain"; why[k]=why.get(k,0)+1
        elif start and "String Match Result" in line and "log10p" in line and "herba" not in line and "ball" not in line:
            lastm=t
        if "Quest completed successfully" in line: comp+=1
    if start and lastm: reads.append((lastm-start).total_seconds())
    rs=[r for r in reads if r<60]
    print(f"{prof}: reads={len(rs)} completions={comp} reads/completion={len(rs)/max(1,comp):.2f} mean={sum(rs)/max(1,len(rs)):.1f}s total={sum(rs)/60:.1f}min  kinds={why}")
