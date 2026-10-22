# BBQ Farmer: ROI ledger

The user (2026-10-22): keep an explicit record of each work item's cost and
payoff, and use it to justify and order the queue. This file is that record.
`pokemon_sv/tools/roi.py` recomputes the estimates from the lab logs.

## How estimates are made

`python3 pokemon_sv/tools/roi.py` (needs `quest_draws.py` beside it):

1. **Route inputs** from the current build era's logs (`PROFILES`, now bbq-r4 +
   bbq-r5): success per attempt and seconds per attempt per quest route. Routes
   with fewer than 4 recent attempts fall back to all logs.
2. **Draw distribution** of new blue quests, content-based (`quest_draws.py`).
3. **Renewal model:** each draw is rerolled (10 BP + 25 s) or attempted up to 2
   times, then rerolled. A completion pays its reward plus a bonus: +11 for the
   10-completion bar (measured R3b) and +9 for reds (R5b: 2 reds x 130 BP in 30
   completions).
4. **Each item is a what-if** on those inputs; gain = model BP/h after minus
   before. Gain per hour = gain / (work hours + Switch hours).

**Calibration:** the model gives 657 BP/h for the bbq-r6 setup; R5b measured
~600-680 BP/h. One 2-hour run varies about +-20% with the board (R5a 259 vs
R5b ~620 on one build), so a single run can't confirm gains under ~100 BP/h;
log several runs before calling a small item measured.

## Ledger (2026-10-22)

Gains are model estimates; "guess" inputs are assumptions to replace with
measurements. Work hours include an agent's coding and review; Switch hours are
the live runs needed to validate.

| Item | Gain BP/h | Work h | Switch h | Gain/h | Basis | Status |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| C10 Auto Battle: stop at board read | +50 | 1.5 | 1 | 20 | 1 pass (~70 s) saved per Auto Battle 10 | queued |
| C12 Kleavor look-around / Scyther fallback | +55 | 2 | 1.5 | 16 | guess: tera-self misses 45% -> 15% | queued, frames in R4/R5 |
| M10 pickup-10 doable | +71 | 4 | 2 | 12 | guess; 12% of draws, now always rerolled | needs feasibility check |
| C12+E4 catch-any at a dense spot | +42 | 3 | 2 | 8 | guess: 14% -> 60% | queued |
| E4 sneak-up at a better spot | +32 | 2 | 2 | 8 | guess: 16% -> 50% | queued |
| M10 photo-fly doable | +40 | 4 | 2 | 7 | guess | needs feasibility check |
| Typed catches at denser spots | +17 | 4 | 3 | 2 | guess: 75% -> 90% | not queued |

**Not modeled as BP/h (risk or correctness; ordered by judgment):**

| Item | Why | Work h | Status |
| --- | --- | ---: | --- |
| C15 follow-up: reroll only if BP covers it | regression for low-BP users before upstream | 0.5 | queued |
| #17 kill count off by ~1, pass totals | wrong logs; C10 depends on the count | 1 | found R3b |
| C5/M8a restore party after failed hatch | wrong lead slows battle routes ~2.5x | Codex | uncommitted |
| M13 navigation recovery bounds | one return took 8m50s | 1 | queued |
| #23 catch-any on return | built; needs a catch-any draw live | 0 | waiting on R6 |

## Measured results (update after each run)

| Item | Estimate | Measured | Runs | Notes |
| --- | --- | --- | --- | --- |
| #21 heal below 50% | ~2-5 min per Auto Battle quest | Auto Battle 10: 337 -> 230 s; 0 heals in 24 passes | R4 | confirmed |
| #22 panel-read reuse | fewer reads | 4.0 -> 2.9 reads per completion | R4 | confirmed |
| #17 stop at target | ~30 s per Auto Battle | stop fired every run | R3b, R4 | confirmed |
| #24 (C14) plaza arrival check | prevents R3's fatal stop | 61/61 landings confirmed, 0 false retries | R5 | confirmed, no BP/h change |
| Exclusions for impossible quests (C15 workaround) | unblock 2 slots | R5a 259 -> R5b ~620 BP/h (2 reds done) | R5 | confirmed; C15 in R6 |
| #26 (M15 step 1b) no-trip chaining | was estimated ~1.5 min/quest | agent measured ~5-20 s/quest | none | estimate was wrong; low priority |

## Updating

After each run: add a row under "Measured results"; move `PROFILES` in roi.py
to the newest build era when its logs have enough attempts; re-run roi.py and
refresh the ledger table; when a guess is measured, put the number into
`items()` and drop "guess". New queue items get a row here before work starts,
with the estimate that justifies them.
