# Renewal model of BBQ reroll policies, blue and red separately.
#
# A slot draws a quest from the measured distribution (quest_draws.py). Policy
# either rerolls it (cost + 25 s, draw again) or attempts it up to TRIES times
# (rerolling after the last failure). A red quest arrives after every 10 blue
# completions and is handled the same way with red prices.
# Route stats are from quest_stats.py over all lab logs (2026-10-22).
REROLL_S = 25; TRIES = 2

# quest: (p success per attempt, seconds per attempt, BP), None = impossible
BLUE = {
    "travel-500": (1.0, 90, 20), "make-tm": (1.0, 74, 20),
    "auto-10": (1.0, 461, 20), "tera-self-defeat": (1.0, 166, 20),
    "photo-swim": (0.64, 71, 30), "photo-other": (0.75, 80, 30),
    "sneak-up": (0.16, 109, 20), "catch-any": (0.14, 148, 20),
    "catch-normal-fire": (0.14, 148, 20), "photo-savanna": (0.0, 96, 30),
    "catch-typed": (0.40, 157, 20), "pickup-10": None, "photo-fly": None,
}
# blue draws, n = 138 (quest_draws.py, content-based)
BLUE_DRAWS = {
    "travel-500": 18, "pickup-10": 17, "auto-10": 16, "tera-self-defeat": 14,
    "make-tm": 12, "sneak-up": 11, "photo-swim": 11, "catch-any": 7,
    "photo-savanna": 6, "photo-fly": 5, "catch-normal-fire": 3,
    "photo-other": 5, "catch-typed": 23,
}
# red: 150 BP (R3 tera raid; Serebii 130-200). Times: tera-raid 518 s (1 run),
# auto-30 ~1070 s (m7b-reuse episode), wash 40% x 267 s, hatch 191 s,
# sandwiches unmeasured as reds (blue singleplayer sandwich 200 s, 2/2;
# lab salty-sandwich held 12 min with no completion) -> guess 50% x 250 s.
RED = {
    "tera-raid": (1.0, 518, 150), "wash-pokemon": (0.4, 267, 150),
    "salty-sandwich": (0.5, 250, 150), "hatch-egg": (1.0, 191, 150),
    "sandwich-three": (0.5, 250, 150), "auto-30": (1.0, 1070, 150),
}
RED_DRAWS = {"tera-raid": 2, "wash-pokemon": 2, "salty-sandwich": 1,
             "hatch-egg": 1, "sandwich-three": 1, "auto-30": 1}

def cycle(stats, draws, reroll, cost):
    """Expected (BP, seconds) to complete one quest from a fresh draw."""
    n = sum(draws.values())
    # per draw: P(complete), E[BP], E[s]; a non-completed draw is followed by a reroll
    pc = bp = s = 0.0
    for q, w in draws.items():
        f = w / n; st = stats[q]
        if st is None or q in reroll:
            s += f * REROLL_S; bp -= f * cost; continue
        p, t, r = st
        fail = (1 - p) ** TRIES
        tries = sum((1 - p) ** k for k in range(TRIES))
        pc += f * (1 - fail); bp += f * ((1 - fail) * r - fail * cost)
        s += f * (tries * t + fail * REROLL_S)
    return bp / pc, s / pc, 1 / pc   # per completion: BP, s, draws

ALWAYS = set()
POL = {
    "A impossible only": ALWAYS,
    "B +sneak-up": {"sneak-up"},
    "C +Pride Rock +savanna": {"sneak-up", "catch-any", "catch-normal-fire", "photo-savanna"},
    "C' C but keep catch-any (R4)": {"sneak-up", "catch-normal-fire", "photo-savanna"},
    "D C + auto-10": {"sneak-up", "catch-any", "catch-normal-fire", "photo-savanna", "auto-10"},
    "F D + typed catches": {"sneak-up", "catch-any", "catch-normal-fire", "photo-savanna", "auto-10", "catch-typed"},
}
RPOL = {
    "attempt all reds": set(),
    "reroll wash+sandwich": {"wash-pokemon", "salty-sandwich", "sandwich-three"},
    "+ reroll auto-30": {"wash-pokemon", "salty-sandwich", "sandwich-three", "auto-30"},
}
red = {k: cycle(RED, RED_DRAWS, v, 50) for k, v in RPOL.items()}
# leaving the red on the board (it holds the top slot; blues go on)
RPOL["ignore reds"] = None; red["ignore reds"] = (0.0, 0.0, 0.0)
print("red policies: BP, s, draws per red completion")
for k, (b, s, d) in red.items(): print(f"  {k:24} {b:6.0f} {s:6.0f} {d:4.2f}")
print("\nBP/hour (blue only | with reds every 10 blue completions, per red policy)")
print(f"{'blue policy':30} {'blue':>6} " + " ".join(f"{k[:14]:>14}" for k in RPOL))
for name, rr in POL.items():
    b, s, d = cycle(BLUE, BLUE_DRAWS, rr, 10)
    row = [3600 * b / s]
    for k in RPOL:
        rb, rs, _ = red[k]
        row.append(3600 * (10 * b + rb) / (10 * s + rs))
    print(f"{name:30} {row[0]:6.0f} " + " ".join(f"{x:14.0f}" for x in row[1:]) + f"   draws/quest {d:.2f}")
