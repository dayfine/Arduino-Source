# Stage test runs

Each stage in [PR_PLAN.md](PR_PLAN.md#stages-what-each-group-of-prs-buys) is checked on the Switch
with the exact code that will go upstream, before its PRs are handed to the user. Results go in
the table at the end and, for upstream readers, on ComputerControl#263.

## Why stage 0 and stage 1 get separate runs

Stage 0 goes upstream first, on top of `main`'s round loop. Stage 1 replaces that loop, and the
BP check from stage 0 is re-wired into stage 1's game-actions seam (each verified reroll gets its
own BP window). The two builds run different BP-tracking code, so each needs its own run.

## Runs

| Run | Build | What it contains | Settings | Length |
|---|---|---|---|---|
| S0 | `test/stage0` 23c6bdea5 | `main` + U#1628 (BP OCR) + #16 (BP stats) + #52 (BP check) + #50 (error report fix) | copy of the R23 profile (`bbq-r19`), new name `bbq-s0` | 30 min |
| S1 | `test/stage01` c0025b745 | stage 1 (#37 … #25) + the stage 0 PRs above, BP wiring moved into the seam | copy of `bbq-r19` as `bbq-s1`; Out of retries: **Reroll** | 30 min |

Binaries (built 2026-10-27, BBQ tests 24/24 and 76/76) are copied with `Resources` and
`Firmware` to the session scratchpad under `builds/stage0` and `builds/stage01`, then signed with
the lab identity ([lab/MACOS_LOCAL_SIGNING.md](lab/MACOS_LOCAL_SIGNING.md): bundle ID
`com.dayfine.pokemonautomation.lab`, certificate "PokemonAutomation Local Development") so the
camera grant carries over. SHA-256 prefix of the signed executable: S0 `c9ea73dda5c39198`, S1
`807bbf61b7b45290`. Rebuild from the branch if the scratchpad is gone.

Both start from the same save at Central Plaza, with at least 500 BP so that rerolls never run
out. Prepare and launch as in [lab/CONTROL.md](lab/CONTROL.md): quit the app cleanly before editing
a profile, keep `settings-at-start.json`, and don't press controller keys while a program runs.
Set **Number of quests to 8** (about 30 minutes at R23's rate) so the farmer ends by itself: the
`BP check` line is logged only when the program finishes, not when it is stopped by hand. A
watcher presses Stop only as a safety cap at 50 minutes.

## Pass criteria

| Check | S0 | S1 | Why |
|---|---|---|---|
| Stats show BP earned, spent, net and BP/h | ✔ | ✔ | stage 0 claim |
| End-of-run line `BP check: OK` | ✔ | ✔ | stage 0 claim: every reroll's BP drop matched its cost |
| No crash or program error | ✔ | ✔ | stage 0 claim (#50) |
| No stretch over 15 min without a completed quest | note only | ✔ | stage 1 claim; `main` can still stall in S0 |
| No stop by the program | note only | ✔ | stage 1 stops only when stuck for good |
| Net BP/h not below the R23 profile's range for the board | note only | note only | a 30-min run varies too much to prove a gain |

A 30-minute run is a smoke test: it shows the code works on the real game and catches
regressions. The BP/h effect of stage 1 shows only in runs that hit a dead slot or a stall, so it
needs the 2-hour runs described in PR_PLAN.md.

## Summarizing a run

```
python3 pokemon_sv/tools/run_summary.py bbq-s0 --stage 0 --label S0
python3 pokemon_sv/tools/run_summary.py bbq-s1 --stage 1 --label S1
```

It reads the profile's `SerialPrograms.log`, takes the last BBQ Farmer launch (`--run N` picks
another), and prints a markdown block: duration, final stats, wall-clock net BP/h, attempts,
failures and rerolls, the longest stretch without a completion, the BP check line, how the run
ended, errors, and a checklist of the stage's pass criteria. Paste the block under Results, add
anything new to [KNOWN_ISSUES.md](KNOWN_ISSUES.md), and add the run to [ROI.md](ROI.md).

Checked against R23 (`bbq-r19`, run 10): 34 quests, 1,040 net BP, 564 BP/h in the stats and 519
wall-clock, longest gap 10.5 min. That build has no BP check, so that line fails as expected.

## Results

| Run | When | Result | Note |
|---|---|---|---|
| S0 | 2026-10-27 12:47–13:09 | invalid | 8 quests in 21 min (617 BP/h wall), then KI-14: an egg quest with no egg left the game in the box, the next panel read failed and the program stopped, so no BP check line. The error report was saved without a screenshot and the app did not crash (#50's case). |
| S1 | 2026-10-27 13:21–13:23 | invalid | KI-14 on the first quest. |
| S0b | 2026-10-27 13:25–13:49 | **pass** | 8 quests in 24 min, 533 BP/h wall; BP check OK (2/2 reroll windows) |
| S1b | 2026-10-27 13:49–14:21 | **pass** | 8 quests in 32 min, 650 BP/h wall; BP check OK (3/3 windows, every reroll confirmed); longest gap 10.9 min |
| S1c | 2026-10-27 14:31–14:56 | **pass** | targeted run for #4 and #25: save every 2 quests, 6 quests, flying rows removed from the exclusions table. Saves at 2, 4 and 7 completions (7 crosses the milestone at 6 without landing on it); 3 quests not possible rerolled on sight, each confirmed; BP check OK (4/4). A first try at 14:30 failed in 10 s on a dimmed screen (KI-15). |

Both were re-run with Number of eggs 0 and Out of eggs: Reroll (the boxes have no eggs).

### S0b: stage 0, 2026-10-27 13:25 → 13:49 (24 min), PASS

- Log: `/Users/difan/Library/Application Support/SerialPrograms/Profiles/bbq-s0/SerialPrograms.log` (run 2 of 2)
- Stats: Quests Completed 8 · Saves 0 · BP Earned 270 · BP Spent 60 · Net BP 210 · BP/hour 543
- Wall-clock net BP/h: 529
- Attempts 10, failed 2, rerolls 2 (+0 whole-panel)
- Longest stretch without a completion: 4.6 min; stretches over 15 min: 0
- BP check: OK. Balance 13484 -> 13694 (change 210), stats net 210. Rerolls expected to cost 60 BP; 2/2 reroll windows matched the reads.
- Ended by: the program (finished or stopped itself)

- [x] stats show BP
- [x] BP check OK
- [x] no crash or program error

### S1b: stage 1, 2026-10-27 13:49 → 14:21 (32 min), PASS

- Log: `/Users/difan/Library/Application Support/SerialPrograms/Profiles/bbq-s1/SerialPrograms.log` (run 2 of 2)
- Stats: Quests Completed 8 · Saves 0 · BP Earned 380 · BP Spent 30 · Net BP 350 · BP/hour 656
- Wall-clock net BP/h: 650
- Attempts 11, failed 3, rerolls 3 (3 confirmed) (+0 whole-panel)
- Longest stretch without a completion: 10.9 min; stretches over 15 min: 0
- BP check: OK. Balance 13694 -> 14044 (change 350), stats net 350. Rerolls expected to cost 30 BP; 3/3 reroll windows matched the reads.
- Ended by: the program (finished or stopped itself)

- [x] stats show BP
- [x] BP check OK
- [x] no crash or program error
- [x] no stretch over 15 min without a completion
- [x] no stop by the program

### S1c: stage 1, 2026-10-27 14:31 → 14:56 (25 min), PASS

- Log: `/Users/difan/Library/Application Support/SerialPrograms/Profiles/bbq-s1/SerialPrograms.log` (run 4 of 4)
- Stats: Quests Completed 7 · Saves 3 · BP Earned 270 · BP Spent 40 · Net BP 230 · BP/hour 566
- Wall-clock net BP/h: 556
- Attempts 8, failed 1, rerolls 4 (4 confirmed) (+0 whole-panel)
- Longest stretch without a completion: 6.2 min; stretches over 15 min: 0
- BP check: OK. Balance 14044 -> 14274 (change 230), stats net 230. Rerolls expected to cost 40 BP; 4/4 reroll windows matched the reads.
- Ended by: the program (finished or stopped itself)

- [x] stats show BP
- [x] BP check OK
- [x] no crash or program error
- [x] no stretch over 15 min without a completion
- [x] no stop by the program

One 8-quest run says nothing about BP/h: S1b drew an easier board. Both runs show the stage
works on the real game and the BP accounting closes exactly.

Not seen on the Switch: #5 (give up when rerolls change nothing; needs an empty BP balance) and
#6's "failed quest still on the panel" (with Out of retries: Reroll every failure is rerolled
first). Both are covered by the BBQFarmerRounds unit tests, including the stuck cases #53 pins.
