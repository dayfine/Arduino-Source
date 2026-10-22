# Monte Carlo of 2-hour BBQ runs on roi.py's inputs: per-run BP/h spread for
# the current setup and with queued items stacked. Attempt times are drawn
# around each route's mean (gamma, CV 0.35); reds every ~1/RED_P completions.
import random, statistics, sys, os, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import roi
random.seed(1)
RUN_S, N, CV = 7200, 4000, 0.35
RED_P, RED_BP, RED_S = 2/30, 130, 400           # R5b: 2 reds in 30; ~400 s each (guess)

def sim(stats, dist, reroll):
    qs, ws = zip(*dist.items()); t = bp = done = 0
    while t < RUN_S:
        q = random.choices(qs, ws)[0]; st = stats[q]
        if st is None or q in reroll: t += roi.REROLL_S; bp -= roi.REROLL_BP; continue
        p, mean = st[0], st[1]
        for _ in range(roi.TRIES):
            t += random.gammavariate(1/CV**2, mean*CV**2)
            if random.random() < p:
                bp += st[2] + roi.BONUS; done += 1
                if random.random() < RED_P: t += RED_S; bp += RED_BP
                break
        else:
            t += roi.REROLL_S; bp -= roi.REROLL_BP
    return bp * 3600 / t

st_recent = roi.route_stats(roi.PROFILES)
st_all = roi.route_stats([os.path.basename(p) for p in glob.glob(roi.BASE + "/bbq-*")])
stats = roi.per_quest(st_recent, st_all); dist = roi.draws()
its = {n: f for n, f, *_ in roi.items(stats)}
scen = [("R6 now", []),
        ("+ C10", ["C10 Auto Battle: stop at board read"]),
        ("+ C10, C12 Kleavor", ["C10 Auto Battle: stop at board read", "C12 Kleavor look-around / Scyther"]),
        ("+ all queued C/E items", ["C10 Auto Battle: stop at board read", "C12 Kleavor look-around / Scyther", "C12+E4 catch-any at a dense spot", "E4 sneak-up at a better spot"]),
        ("+ M10 pickup-10, photo-fly", ["C10 Auto Battle: stop at board read", "C12 Kleavor look-around / Scyther", "C12+E4 catch-any at a dense spot", "E4 sneak-up at a better spot", "M10 pickup-10 doable", "M10 photo-fly doable"])]
print(f"{'scenario':30} {'median':>6} {'p10':>5} {'p90':>5}   (BP/h per 2-hour run, {N} sims)")
for name, keys in scen:
    st2 = dict(stats); rr = set(roi.REROLL)
    for k in keys: its[k](st2, rr)
    xs = sorted(sim(st2, dist, rr) for _ in range(N))
    print(f"{name:30} {statistics.median(xs):6.0f} {xs[N//10]:5.0f} {xs[9*N//10]:5.0f}")
