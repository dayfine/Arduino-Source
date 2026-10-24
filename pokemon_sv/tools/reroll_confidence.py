# Run-or-reroll decisions with the uncertainty of small samples.
#
# Each quest's success rate per attempt is a Beta(1 + successes, 1 + failures)
# posterior (uniform prior). For each quest we sample the rate, recompute the
# optimal policy rate lambda* (reroll_threshold.py's model, other quests at their
# posterior means) and the quest's margin  B - lambda* T + (C + lambda* TAU):
# positive means attempting beats rerolling. Reported: P(attempt is better) and a
# 10-90% range of the margin. A quest with P near 0.5 is undecided at its sample
# size; one more attempt or two tells more than the point estimate.
import os, random
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "reroll_threshold.py")).read()
ns = {}; exec(src.split("base = {q for q")[0], ns)
BLUE, DRAWS, C, TAU = ns["BLUE"], ns["DRAWS"], ns["C"], ns["TAU"]
random.seed(2)

# attempts and successes on the current build (R11-R14, Camera Support off)
COUNTS = {
    "travel-500": (15, 15), "make-tm": (17, 12), "auto-10": (9, 9), "tera-self-defeat": (13, 6),
    "photo-swim": (16, 15), "photo-other": (10, 8), "sneak-up": (9, 9), "catch-any": (4, 1),
    "catch-normal-fire": (2, 2), "photo-savanna": (2, 2), "catch-typed": (24, 16), "catch-poison-dark": (5, 0),
}

def with_rate(q, p):
    b = dict(BLUE); _, t, r = b[q]; b[q] = (p, t, r); return b

def margin(blue, q):
    ns["BLUE"] = blue
    S, lam = ns["optimal"]()
    b, t, _ = ns["draw_value"](q, set())
    ns["BLUE"] = BLUE
    return b - lam * t + (C + lam * TAU)

print(f"{'quest':20} {'n':>3} {'ok':>3} {'mean':>5}  {'P(run better)':>13}  margin 10-90%")
for q, (n, ok) in COUNTS.items():
    xs = sorted(margin(with_rate(q, random.betavariate(1 + ok, 1 + n - ok)), q) for _ in range(400))
    p = sum(x > 0 for x in xs) / len(xs)
    verdict = "run" if p > 0.8 else "reroll" if p < 0.2 else "undecided"
    print(f"{q:20} {n:3} {ok:3} {(1+ok)/(2+n):5.2f}  {p:13.2f}  {xs[40]:+6.1f} .. {xs[360]:+6.1f}   {verdict}")
