# Distribution of newly appearing quests, split into blue and red.
#
# A panel read is the run of "String Match Result" lines after "Reading quests."
# (it ends at the first non-OCR action line). A red quest sits in the top slot
# and pushes the three blues down, so a read with 4 matched slots has a red in
# slot 1; a read with 3 matches and an empty 4th slot has none.
#
# New blue quests are the multiset difference of consecutive reads' blue
# quests (by content, not by slot), so the red slot's shift does not count as
# a change. Red episodes are tracked from the read where a red appears to the
# read where it is gone.
import glob, os, re, sys
from collections import Counter
from datetime import datetime

base = os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
QUEST = re.compile(r"\): \(([a-z0-9-]+)\) \(log10p")
END = ("issue_mash_button", "OCR Text", "pbf_", "Pressing", "Current quest", "Detected")

def stamp(line):
    try: return datetime.fromisoformat(line[:26])
    except ValueError: return None

def reads_of(path):
    """Yields (time, slots) per panel read; None marks a program start."""
    cur = None; t = None
    for line in open(path, errors="replace"):
        if "Received Program Start Request" in line:
            if cur: yield t, cur
            cur = None; yield None, None; continue
        if "Reading quests." in line:
            if cur: yield t, cur
            cur = []; t = stamp(line); continue
        if cur is None: continue
        if "String Match Result" in line:
            m = QUEST.search(line)
            cur.append(m.group(1) if m else "?")
            if len(cur) == 4: yield t, cur; cur = None
        elif any(k in line for k in END):
            if cur: yield t, cur
            cur = None
    if cur: yield t, cur

def split(slots):
    """(red, blues) or None when the read is unreliable."""
    if len(slots) == 4 and "?" not in slots: return slots[0], slots[1:]
    if len(slots) >= 3 and "?" not in slots[:3] and (len(slots) == 3 or slots[3] == "?"):
        return None, slots[:3]
    return None

blue = Counter(); red = Counter(); reads = bad = 0
episodes = []   # (profile, quest, start, end or None)
for prof in sorted(glob.glob(base + "/bbq-*")):
    path = prof + "/SerialPrograms.log"
    if not os.path.exists(path): continue
    name = os.path.basename(prof)
    prev = None; open_ep = None
    for t, slots in reads_of(path):
        if slots is None:
            prev = None
            if open_ep: episodes.append((name, open_ep[0], open_ep[1], None)); open_ep = None
            continue
        s = split(slots)
        if s is None: bad += 1; continue
        reads += 1
        r, b = s
        if prev is not None:
            pr, pb = prev
            blue.update(Counter(b) - Counter(pb))
            if r and r != pr: red[r] += 1
        if open_ep and r != open_ep[0]:
            episodes.append((name, open_ep[0], open_ep[1], t)); open_ep = None
        if r and not open_ep: open_ep = (r, t)
        prev = s
    if open_ep: episodes.append((name, open_ep[0], open_ep[1], None))

def table(title, c):
    total = sum(c.values())
    print(f"\n{title}: {total} draws")
    for k, v in c.most_common(): print(f"  {k:22} {v:4} {v/total:6.1%}")

print(f"reads {reads}, skipped unreliable reads {bad}")
table("blue new quests", blue)
table("red new quests (appeared in top slot)", red)
print("\nred episodes (profile, quest, minutes on board; open = still there at program end)")
for name, q, a, b in episodes:
    mins = f"{(b - a).total_seconds() / 60:5.1f}" if a and b else " open"
    print(f"  {name:16} {q:22} {mins}  from {a:%m-%d %H:%M}" if a else f"  {name} {q}")
