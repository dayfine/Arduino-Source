# Monte Carlo of 2-hour BBQ runs on the current build's measured inputs
# (reroll_threshold.py: R11-R12b, Camera Support off), with the reroll policies and
# queued items as scenarios. Attempt times are gamma around each mean (CV 0.35).
# A red quest arrives after every 10 completions. OVERHEAD_S per completion (board
# reads, returns to the plaza, saves every 10) is fitted so the current policy
# matches the measured R11/R12/R12b runs (660/725/676 BP/h).
import random, statistics, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "reroll_threshold.py")).read()
ns = {}; exec(src.split("base = {q for q")[0], ns)
BLUE, DRAWS, TRIES, C, TAU, BAR = ns["BLUE"], ns["DRAWS"], ns["TRIES"], ns["C"], ns["TAU"], ns["BAR"]
random.seed(1)
RUN_S, N, CV = 7200, 3000, 0.35
OVERHEAD_S = 33
# red quests on the current build: (p, s, BP); None = rerolled (50 BP)
RED = {"tera-raid": (1.0, 233, 150), "hatch-egg": (1.0, 190, 200), "sandwich-three": (1.0, 198, 130),
       "auto-30": (1.0, 720, 150), "salty-sandwich": (0.5, 250, 150), "wash": None}
RED_DRAWS = {"tera-raid": 2, "wash": 2, "salty-sandwich": 1, "hatch-egg": 1, "sandwich-three": 1, "auto-30": 1}

def gamma(m): return random.gammavariate(1 / CV**2, m * CV**2)

def attempt(st, reroll_cost):
    """One draw: returns (seconds, BP, completed)."""
    p, t, r = st; s = 0.0
    for _ in range(TRIES):
        s += gamma(t)
        if random.random() < p: return s, r, True
    return s + TAU, -reroll_cost, False

def sim(blue, reroll):
    qs, ws = zip(*DRAWS.items()); rq, rw = zip(*RED_DRAWS.items())
    t = bp = done = 0
    while t < RUN_S:
        q = random.choices(qs, ws)[0]; st = blue[q]
        if st is None or q in reroll:
            t += TAU; bp -= C; continue
        s, b, ok = attempt(st, C); t += s; bp += b
        if ok:
            done += 1; bp += BAR; t += OVERHEAD_S
            if done % 10 == 0:
                while True:
                    r = random.choices(rq, rw)[0]
                    if RED[r] is None: t += TAU; bp -= 50; continue
                    s, b, ok2 = attempt(RED[r], 50); t += s; bp += b
                    if ok2: break
    return bp * 3600 / t

def run(name, blue, reroll):
    xs = sorted(sim(blue, reroll) for _ in range(N))
    print(f"{name:44} {statistics.median(xs):6.0f} {xs[N//10]:5.0f} {xs[9*N//10]:5.0f}")

IMPOSSIBLE = {q for q, st in BLUE.items() if st is None}
print(f"{'scenario':44} {'median':>6} {'p10':>5} {'p90':>5}  (BP/h per 2-hour run, {N} sims)")
run("current: wash + impossible rerolled", BLUE, IMPOSSIBLE)
run("+ Poison/Dark rerolled", BLUE, IMPOSSIBLE | {"catch-poison-dark"})
opt = IMPOSSIBLE | {"catch-poison-dark", "catch-any", "auto-10"}
run("+ optimal set (also catch-any, auto-10)", BLUE, opt)
b2 = dict(BLUE); b2["make-tm"] = (0.95, 95, 20)
run("  + Make TM look-ahead (120 -> 95 s)", b2, opt)
b3 = dict(b2); b3["photo-flying"] = (0.5, 120, 30)
run("  + photo-flying doable (guess 50%, 120 s)", b3, opt)
b4 = dict(b3); b4["catch-poison-dark"] = (0.6, 170, 40)
run("  + Poison/Dark route fixed (guess 60%)", b4, opt - {"catch-poison-dark"})
