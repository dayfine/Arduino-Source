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

Both were re-run with Number of eggs 0 and Out of eggs: Reroll (the boxes have no eggs).
