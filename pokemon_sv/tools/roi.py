# ROI of BBQ Farmer work items, from the lab logs.
#
# 1. Measures each quest route (success per attempt, seconds per attempt) from
#    the profile logs of the current build era (PROFILES), falling back to all
#    logs for routes with too few attempts.
# 2. Measures the new-quest draw distribution (content-based, as quest_draws.py).
# 3. Runs the renewal model (reroll_model.py): each draw is either rerolled
#    (10 BP + 25 s) or attempted up to TRIES times; every completion also earns
#    BONUS (the 10-completion bar, ~+110 per 10) plus RED (reds' share).
# 4. Applies each work item as a what-if change to the inputs and reports the
#    BP/hour gain, the cost, and gain per hour of work.
#
# Item inputs marked "guess" are assumptions to replace with measurements; see
# pokemon_sv/ROI.md for the ledger and how to update it.
import glob, os, re, sys
from collections import Counter, defaultdict
from datetime import datetime

BASE = os.path.expanduser("~/Library/Application Support/SerialPrograms/Profiles")
PROFILES = ["bbq-r4", "bbq-r5"]          # current build era (#21, #22, #24)
MIN_ATTEMPTS = 4
REROLL_S, REROLL_BP, TRIES = 25, 10, 2
BONUS = 11                                # crown bar: +110 per 10 completions (R3b)
RED = 9                                   # reds per completion: R5b 2 x 130 over 30

# quest slug -> (route regex in "Quest:/Catch:/Photo:" lines, BP reward)
QUESTS = {
    "travel-500": (r"Travel 500", 20), "make-tm": (r"Make TM", 20),
    "auto-10": (r"Auto Battle", 20), "tera-self-defeat": (r"Tera-self", 20),
    "photo-swim": (r"Photo: Swimming", 30), "photo-other": (r"Photo: (Coastal|Canyon|In-flight|Polar)", 30),
    "sneak-up": (r"Sneak up", 20), "catch-any": (r"Catch: Any", 20),
    "catch-typed": (r"Catch: (?!Any)", 20), "sandwich": (r"sandwich", 20),
    "photo-savanna": (r"Photo: Normal", 30), "pickup-10": (None, 20), "photo-fly": (None, 30),
}
# current reroll list (bbq-r6 = policy C' + C15 for impossible quests)
REROLL = {"sneak-up", "photo-savanna", "pickup-10", "photo-fly"}
IMPOSSIBLE = {"pickup-10", "photo-fly"}

ts = lambda l: datetime.strptime(l[:26], "%Y-%m-%d %H:%M:%S.%f")
ROUTE = re.compile(r"\]: ((Quest: [^.]+)|(Catch: [^\n]+)|(Photo: [^\n]+))")

def route_stats(profiles):
    st = defaultdict(lambda: [0, 0, 0.0])      # attempts, ok, seconds
    for prof in profiles:
        f = f"{BASE}/{prof}/SerialPrograms.log"
        if not os.path.exists(f): continue
        start = route = None
        for line in open(f, errors="replace"):
            if len(line) < 27 or line[4] != "-": continue
            if "Doing quest." in line or "Quest did not complete." in line and start is None:
                start, route = ts(line), None; continue
            if start and route is None:
                m = ROUTE.search(line)
                if m: route = m.group(1)
            done = "Current quest was not found" in line
            if start and (done or "Quest did not complete" in line):
                s = st[route or "?"]; s[0] += 1; s[1] += done; s[2] += (ts(line) - start).total_seconds()
                start = None
                if not done: start, route = ts(line), None   # a retry follows
    return st

def per_quest(st_recent, st_all):
    out = {}
    for q, (pat, bp) in QUESTS.items():
        if pat is None: out[q] = None; continue
        def agg(st):
            a = [v for r, v in st.items() if re.search(pat, r)]
            return [sum(x[i] for x in a) for i in range(3)]
        n, ok, s = agg(st_recent)
        src = "recent"
        if n < MIN_ATTEMPTS:
            n, ok, s = agg(st_all); src = "all"
        out[q] = (ok / n if n else 0.5, s / n if n else 150, bp, n, src) if n else (0.5, 150, bp, 0, "guess")
    return out

def draws():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import io, contextlib
    with contextlib.redirect_stdout(io.StringIO()):
        import quest_draws as qd                  # reuses its parsing
    c = Counter(qd.blue)
    m = Counter()
    for k, v in c.items():
        if k in QUESTS: m[k] += v
        elif k.startswith("catch-"): m["catch-any" if k == "catch-any" else "catch-typed"] += v
        elif k.startswith("photo-"): m["photo-other"] += v
        elif "sandwich" in k: m["sandwich"] += v
    return {k: v for k, v in m.items() if k in QUESTS}

def bp_per_hour(stats, dist, reroll, bonus):
    n = sum(dist.values()); pc = bp = s = 0.0
    for q, w in dist.items():
        f = w / n; st = stats[q]
        if st is None or q in reroll:
            s += f * REROLL_S; bp -= f * REROLL_BP; continue
        p, t, r = st[:3]
        fail = (1 - p) ** TRIES; tries = sum((1 - p) ** k for k in range(TRIES))
        pc += f * (1 - fail); bp += f * ((1 - fail) * (r + bonus) - fail * REROLL_BP)
        s += f * (tries * t + fail * REROLL_S)
    return 3600 * bp / s

# Work items: (name, what-if on (stats, reroll set), work hours incl. review, Switch hours, basis)
def items(stats):
    def upd(q, **kw):
        def f(st, rr):
            p, t, r, n, src = st[q]
            st[q] = (kw.get("p", p), t + kw.get("dt", 0), r, n, src)
            if kw.get("keep"): rr.discard(q)
        return f
    def enable(q, p, t, bp):
        def f(st, rr): st[q] = (p, t, bp, 0, "guess"); rr.discard(q)
        return f
    return [
        ("C10 Auto Battle: stop at board read", upd("auto-10", dt=-70), 1.5, 1, "1 pass (~70 s) saved; R4 passes ~70 s"),
        ("C12 Kleavor look-around / Scyther", upd("tera-self-defeat", p=0.85), 2.0, 1.5, "guess: misses 45% -> 15%"),
        ("C12+E4 catch-any at a dense spot", upd("catch-any", p=0.6, keep=True), 3.0, 2, "guess: 14% -> 60% at Chargestone court"),
        ("E4 sneak-up at a better spot", upd("sneak-up", p=0.5, keep=True), 2.0, 2, "guess: 16% -> 50%"),
        ("M10 pickup-10 doable", enable("pickup-10", 0.8, 150, 20), 4.0, 2, "guess: unknown route; 12% of draws"),
        ("M10 photo-fly doable", enable("photo-fly", 0.6, 120, 30), 4.0, 2, "guess"),
        ("Typed catches: denser spots", upd("catch-typed", p=0.9), 4.0, 3, "guess: 75% -> 90%"),
    ]

if __name__ == "__main__":
    st_recent = route_stats(PROFILES)
    st_all = route_stats([os.path.basename(p) for p in glob.glob(BASE + "/bbq-*")])
    stats = per_quest(st_recent, st_all)
    dist = draws()
    bonus = BONUS + RED
    base = bp_per_hour(stats, dist, set(REROLL), bonus)
    print(f"route inputs ({'+'.join(PROFILES)}; 'all' = all logs, too few recent attempts)")
    for q, v in stats.items():
        print(f"  {q:18} " + ("impossible" if v is None else f"p={v[0]:.2f} t={v[1]:5.0f}s n={v[3]:3} {v[4]}") + f"  draws {dist.get(q, 0)}")
    print(f"\nbaseline model: {base:.0f} BP/h (bonus {bonus}/completion); measured R5b ~600-680")
    rows = []
    for name, f, work, switch, basis in items(stats):
        st2 = dict(stats); rr = set(REROLL); f(st2, rr)
        gain = bp_per_hour(st2, dist, rr, bonus) - base
        rows.append((gain / (work + switch), name, gain, work, switch, basis))
    print(f"\n{'item':40} {'gain BP/h':>9} {'work h':>6} {'switch h':>8} {'gain/h':>6}  basis")
    for r, name, gain, work, switch, basis in sorted(rows, reverse=True):
        print(f"{name:40} {gain:+9.0f} {work:6.1f} {switch:8.1f} {r:6.1f}  {basis}")
