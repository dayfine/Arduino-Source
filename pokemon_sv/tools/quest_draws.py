# Distribution of newly appearing quests: every time a board slot's quest
# changes between consecutive panel reads (completion or reroll), count the
# new quest. Uses the OCR string-match lines of each read.
import glob, os, re
from collections import Counter
base=os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
draws=Counter(); reads=0
for prof in sorted(glob.glob(base+"/bbq-*")):
    f=prof+"/SerialPrograms.log"
    if not os.path.exists(f): continue
    prev=None; cur=None
    for line in open(f,errors="replace"):
        if "Reading quests." in line:
            if cur is not None and len(cur)>=3:
                if prev is not None:
                    for i in range(min(3,len(cur),len(prev))):
                        if cur[i]!=prev[i] and cur[i]!="?": draws[cur[i]]+=1
                prev=cur; reads+=1
            cur=[]
            continue
        if cur is not None and "String Match Result" in line:
            m=re.search(r"\): \(([a-z0-9-]+)\)",line)
            cur.append(m.group(1) if m else "?")
            if "no matches" in line: cur[-1]="?"
        if "Received Program Start Request" in line: prev=None; cur=None
total=sum(draws.values())
print("reads",reads,"new-quest draws",total)
for k,v in draws.most_common(): print(f"{k:22} {v:4} {v/total:5.1%}")
