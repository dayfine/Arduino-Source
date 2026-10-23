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
| C16 pickup-10 as incidental (skip, don't reroll) | +0 to +100 | 0 (settings) | 2 | high if it works | test in R7: unknown whether it completes passively | queued (R7 settings) |
| Make TM: remember last craftable TM | +0 to +5 | 0.5 | 0.5 | low | Make TM 79 s / 91% today; matters only if early TMs run out | not queued |
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
| R5b overall (exclusions + #24) | model 657 | 604 BP/h, 34 quests | R5b | inside MC band |
| C15 reroll impossible on sight + eggs enabled | model 657 | 644 BP/h, 35 quests, hatch red +220 | R6 | inside MC band; #23 catch path seen working |
| #26 (M15 step 1b) no-trip chaining | was estimated ~1.5 min/quest | agent measured ~5-20 s/quest | none | estimate was wrong; low priority |
| C17 hatch fix (stats view, lead check, restore) + start-up fly-spot check | model 657 | 509 BP/h over 58 min, 13 quests, hatch red +200 | R8 | short run, inside MC p10-p90 (451-698); hatch works end to end |
| R10 build (look-arounds, catch-any court, catch-flying, catch options) | model 657 | 507 BP/h over 2h10, 32 quests | R10 | below band: pickup-10 run as Auto Battle never completes (~26 min lost, 4 rerolls), Tera-self glide 1/6 (17 min, s/success 1023 vs 247 in R6). Make TM 6/7, sneak-up 3/5, travel 6/6, photos 5/5. Without those two losses ~690 BP/h. Camera Support ON gave 0 kills in Auto Battle. |

## Projection (Monte Carlo, `pokemon_sv/tools/roi_mc.py`)

4000 simulated 2-hour runs per scenario on roi.py's inputs (attempt times
gamma-distributed around each route's mean, CV 0.35; reds 2 per 30 completions
at 130 BP, ~400 s each, guess). BP/h per run, 2026-10-22:

| Scenario | Median | p10 | p90 |
| --- | ---: | ---: | ---: |
| R6 now | 574 | 451 | 698 |
| + C10 | 614 | 484 | 740 |
| + C10, C12 Kleavor | 660 | 544 | 783 |
| + all queued C/E items | 714 | 596 | 829 |
| + M10 pickup-10, photo-fly | 768 | 657 | 879 |

Measured runs sit inside the "now" band (R4 459, R5b 604); R5a (259) is
outside because blocked slots aren't modeled (C15 removes that case). The
p10-p90 spread (~250 BP/h) is larger than any single item's gain, so one run
can't confirm an item; compare medians over several runs.

## Updating

After each run: add a row under "Measured results"; move `PROFILES` in roi.py
to the newest build era when its logs have enough attempts; re-run roi.py and
refresh the ledger table; when a guess is measured, put the number into
`items()` and drop "guess". New queue items get a row here before work starts,
with the estimate that justifies them.
