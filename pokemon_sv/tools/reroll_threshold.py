# Which blue quests to reroll: renewal-reward model with the optimal rate as the
# threshold (Dinkelbach iteration), plus each quest's marginal effect.
#
# Each slot draw is quest i with probability q_i. Attempting i up to TRIES times
# earns (R_i + BAR) on success; after the last failure the slot is rerolled.
# Rerolling costs C BP and TAU seconds and gives a fresh draw. Long-run rate:
#   lambda(S) = sum_i q_i * B_i(S) / sum_i q_i * T_i(S)
# where S is the reroll set, B_i = expected BP of a draw of i, T_i its time.
# Rerolling i beats attempting it iff  B_i - lambda* T_i  <  -(C + lambda* TAU),
# with lambda* the rate of the optimal policy, so the threshold moves as S grows:
# iterate until S stops changing.
TRIES = 2; C = 10; TAU = 25; BAR = 11   # +110 BP per 10 completions

# current build (R11-R12b, Camera Support off): (p per attempt, s per attempt, BP)
BLUE = {
    "travel-500": (1.00, 87, 20), "make-tm": (0.95, 120, 20),
    "auto-10": (1.00, 240, 20), "tera-self-defeat": (0.67, 141, 20),
    "photo-swim": (0.94, 62, 30), "photo-other": (0.80, 70, 30),
    "sneak-up": (0.85, 112, 20), "catch-any": (0.50, 169, 20),
    "catch-normal-fire": (0.70, 186, 40), "photo-savanna": (0.70, 83, 30),
    "catch-typed": (0.70, 160, 40), "catch-poison-dark": (0.00, 163, 40),
    "pickup-10": None, "photo-flying": None,
}
# blue draws from quest_draws.py (n = 138); typed catches split: ~2 of 23 poison/dark
DRAWS = {
    "travel-500": 18, "pickup-10": 17, "auto-10": 16, "tera-self-defeat": 14,
    "make-tm": 12, "sneak-up": 11, "photo-swim": 11, "catch-any": 7,
    "photo-savanna": 6, "photo-flying": 5, "catch-normal-fire": 3,
    "photo-other": 5, "catch-typed": 21, "catch-poison-dark": 2,
}

def draw_value(q, reroll):
    """Expected (BP, seconds, completions) of one draw of q."""
    st = BLUE[q]
    if st is None or q in reroll:
        return -C, TAU, 0.0
    p, t, r = st
    fail = (1 - p) ** TRIES
    tries = sum((1 - p) ** k for k in range(TRIES))
    return (1 - fail) * (r + BAR) - fail * C, tries * t + fail * TAU, 1 - fail

def rate(reroll):
    n = sum(DRAWS.values()); bp = s = c = 0.0
    for q, w in DRAWS.items():
        b, t, k = draw_value(q, reroll)
        bp += w / n * b; s += w / n * t; c += w / n * k
    return 60 * bp / s, c   # BP/min, completions per draw

def optimal():
    reroll = {q for q, st in BLUE.items() if st is None}
    while True:
        lam = rate(reroll)[0] / 60
        new = {q for q, st in BLUE.items() if st is None}
        for q in BLUE:
            if BLUE[q] is None: continue
            b, t, _ = draw_value(q, set())
            if b - lam * t < -(C + lam * TAU):
                new.add(q)
        if new == reroll: return reroll, lam
        reroll = new

base = {q for q, st in BLUE.items() if st is None}
r0, c0 = rate(base)
n = sum(DRAWS.values())
print(f"baseline (reroll only impossible): {r0:.2f} BP/min = {60*r0:.0f} BP/h of quest time")
print(f"{'add to reroll set':20} {'q':>5} {'BP/min':>7} {'delta':>6}  {'value - lambda*t':>16}")
lam0 = r0 / 60
for q in sorted(BLUE, key=lambda q: draw_value(q, set())[0] - lam0 * draw_value(q, set())[1]):
    if BLUE[q] is None: continue
    b, t, _ = draw_value(q, set())
    r1, _ = rate(base | {q})
    print(f"{q:20} {DRAWS[q]/n:5.2f} {r1:7.2f} {r1-r0:+6.2f}  {b - lam0*t:+10.1f} vs {-(C+lam0*TAU):+.1f}")
S, lam = optimal()
QR = sum(DRAWS[q] for q in S) / n
print(f"\noptimal reroll set: {sorted(S - base)} (+ impossible)")
print(f"rate {60*lam:.2f} BP/min; P(draw rerolled) = {QR:.2f}; rerolls per kept quest = {QR/(1-QR):.2f}")
