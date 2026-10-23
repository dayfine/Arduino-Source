# Pokémon SV BBQ Farmer: lab notes

This folder (fork `dayfine/Arduino-Source`, orphan branch `agents`, never merged)
holds everything about the 2026-10 effort to make the BBQ Farmer
(`PokemonSV:BBQSoloFarmer`) reliable and fast: how to change and test it, how
we judged what to work on, what each run measured, and what we thought of but
didn't do. Code lives in PRs; this folder is the record around them.

**Where it ended up (2026-10-22):** on a Switch 1 (Traditional Chinese), the
farmer went from ~400 net BP/hour with multi-hour stalls (`main`, Oct 20) to
~600-700 net BP/hour with no stalls or fatal stops over ~10 hours of lab runs.
User-facing summary: PokemonAutomation/ComputerControl#263.

## Map of this folder

| File | What |
| --- | --- |
| `ROADMAP.md` | Execution queue (C1-C16, E1-E4, A1-A2, M-items), per-run results (R3b onward), quest-value and reroll-policy analysis |
| `ROI.md` | Ledger of each work item's estimated payoff, cost and measured result; Monte Carlo projection |
| `PR_WORKFLOW.md` | Where PRs live (fork vs upstream), banners, stacking, moving PRs upstream |
| `VALIDATION_WORKFLOW.md` | The older Codex validation loop (milestones M1-M15) |
| `lab/CONTROL.md` | Driving the lab app and the Switch: build flags, keys, manual map/fly control |
| `lab/builds/R*_BUILD.md` | Exact build recipe and test results per lab build; `bbq-native-runner.cpp` + `link-native-tests.py` run the native tests |
| `lab/STACK.md`, `M*.md`, `fixtures/` | Earlier milestone notes and log excerpts |
| `tools/` | `quest_stats.py`, `quest_draws.py`, `reroll_model.py`, `roi.py`, `roi_mc.py` |

## Updating the program

1. **Branch.** New work is a draft PR on the fork, based on its parent's branch
   so the diff shows only the change (`PR_WORKFLOW.md`). Upstream only gets PRs
   with no unmerged parent. Every PR body starts with a "Depends on:" banner;
   refactors add a "How to review" section.
2. **Test without the Switch.** BBQ logic is behind seams (`BBQGameActions`,
   Auto Battle actions, `plan_quests()`), so decisions are tested with scripted
   fakes in `PokemonSV_Tests.cpp` (data-driven, designated initializers). Build
   the native test runner per `lab/builds/R6_BUILD.md`. Source files are CRLF,
   no tabs.
3. **Lab build.** Stack the PRs under test on the previous lab branch
   (`lab/rN` = `lab/rN-1` + PRs; local branches, recorded in `R*_BUILD.md`), build
   the app (Ninja, arm64, RelWithDebInfo; opencv@4 pkg-config, Qt 6.8.3, AGL
   from the 15.4 SDK), copy the previous profile to `bbq-rN`.
4. **Live run.** Launch with `--profile bbq-rN`; start from Central Plaza
   outdoors. Monitor the profile log every ~10 min (quests, BP, errors, stuck
   steps); never press keys while a program is RUNNING; stop at a "Reading quest
   panel" line. 30-45 min is enough to see a feature work; ~2 h runs are only
   needed to compare BP/hour.
5. **Record.** Add the run to `ROADMAP.md` ("Run log") and a "Measured results"
   row to `ROI.md`; re-run `tools/roi.py`.

Settings that matter (profile `UserSettings/SerialPrograms-Settings.json`, UTF-8
BOM): `QUEST_EXCLUSIONS` (reroll list), `NUM_RETRIES` = 1, `OUT_OF_RETRIES` =
reroll, `NUM_EGGS` / `OUT_OF_EGGS` (hatch quests use the box the box menu opens
on; we use box 13 with 30 eggs, reroll when out — "keep going" blocks red
quests), `SAVE_NUM_QUESTS` = 10, language `chi_tra`.

## Methodology

- **Measure from logs, not impressions.** Each route's success per attempt and
  seconds per attempt come from the profile logs (`tools/quest_stats.py`); the
  draw distribution of new quests is counted by content, red slot separate
  (`tools/quest_draws.py`). A red ("strong") quest sits in the top slot and
  pushes the blues down: a 4-entry board means a red is present.
- **Model BP/hour, then decide.** `tools/roi.py` runs a renewal model (draw ->
  reroll or attempt up to 2 tries -> reroll on failure), with a completion
  bonus for the 10-completion bar (+110 per 10, measured) and reds (~+9 per
  completion). Each work item is a what-if on its inputs; items are ranked by
  BP/hour gained per hour of work. Calibrated: model 657 vs measured ~600-680.
- **Expect noise.** `tools/roi_mc.py` simulates 2-hour runs: the p10-p90 spread
  is ~250 BP/hour, bigger than any single item's gain. One run shows whether a
  feature works, not whether a +5% item paid off.
- **Check before claiming.** Two estimates were wrong and are kept as lessons
  in `ROI.md`/`ROADMAP.md`: batching was estimated at ~1.5 min/quest (actual
  ~5-20 s), and pickup-10 "completing passively" turned out to be unlogged
  rerolls (BP -10 each time).

## Records

| Run | Build | Net BP/h | Highlights |
| --- | --- | ---: | --- |
| Oct 20 overnight | upstream `main` | ~400, then 0 for 3.2 h | stalled on impossible / single-spawn quests |
| R3b | + Auto Battle stop-at-target, bounds, BP status | 398 | 23 quests; 10-completion bonus measured |
| R4 | + heal only below 50% HP, panel-read reuse | 459 | Auto Battle 10: 337 -> 230 s; 0 heals in 24 passes; reads 4.0 -> 2.9 per completion |
| R5a / R5b | + Central Plaza landing check | 259 / 604 | 73/73 landings confirmed; R5a lost 2 slots to impossible quests, fixed by rerolling them |
| R7 | + BP-check, ordering, Make TM resume, return bounds, kill count, C10 board read, no-trip chaining | 424 (40 min) | stopped by the hatch bug (C17); every new feature seen working |
| R6 | + reroll impossible quests on sight (C15), eggs enabled | 644 | 35 quests; hatch red +220; 120/120 landings over R5+R6; catch-any completed after the #23 fix |
| R8 | + hatch fix (C17), start-up fly-spot check | 509 (58 min) | hatch red +200 end to end; failed-hatch recovery restored the lead; 3 false starts fixed |
| R9 | + C18 egg icon scan | ~440 (37 min) | 10 quests, net +270; no hatch quest drawn |
| R10 | + Kleavor/Pride Rock look-around (#35), catch-any court (#36), catch-flying canyon, catch options; Camera Support turned off in-game | 507 (2h10) | 32 quests, net +1,100. Losses: pickup-10 via auto battle 0/10 after 40+ kills (now never run), Tera-self glide 1/6 (moved to the court). 12:44-13:03 ran with Camera Support ON: 0 kills in 6 Auto Battle passes. |

Per-run details, Kleavor frame studies and the red-quest data are in
`ROADMAP.md` ("Run log"). Upstream PRs: PokemonAutomation/Arduino-Source#1628,
#1631, #1632, #1636, #1640 (BBQ), #1663 (FRLG); fork PRs #2-#26 follow them
(map in `PR_WORKFLOW.md` and `ROADMAP.md`).

## Ideas we had but didn't do

Rough value from `ROI.md` where modeled. In descending order of what we'd do next.

| Idea | Why | Value |
| --- | --- | --- |
| C12 Kleavor look-around, else glide to the Scyther field | glide lands right but Kleavor is absent ~40% of the time; any wild Pokémon counts | +55 BP/h |
| C10 read the board near the Auto Battle target | game finishes ~1 kill before the program counts it; saves a pass | +50 BP/h |
| C16 "incidental" quests (keep, don't reroll) | if pickup-10 progresses during other quests | 0 to +100 BP/h, test first |
| M10 make pickup-10 / photo-fly doable | pickup-10 is 14% of draws, always rerolled | +40-70 BP/h if a route under ~4 min exists |
| E4 denser spots for catch-any / sneak-up; no day skip for random spawns | Pride Rock success 14-16% | +30-40 BP/h each |
| M15 (a) photo + catch of the same target in one outing | e.g. photo-ghost + catch-ghost; routes differ today | needs new paths |
| M15 (c) same-area batching | needs each route to record its plaza; N1 (#20) fly-from-anywhere | small |
| C13 adaptive reroll from live success rates | replaces the hand-tuned exclusion list | small, robustness |
| C18 find eggs by the slot icon (Egg Hatcher's method) | view-independent, one frame, up to ~18 s faster per hatch | small, robustness |
| Make TM: detect craftable TMs on screen / OCR materials | today it presses A down the list | small |
| M11a skip the rinse after a failed scrub (wash) | wash is rerolled today | small |
| M8b select the egg box at startup | today the user picks the box | convenience |
| Radar-ball detection fails after every landing | falls back to a hard-coded spot; not investigated | unknown |
| Read in-game time; log what species a battle was against | to explain spawn misses | diagnostics |
| Item Printer: log BP before/after | turns BP/hour into items/hour | measurement |

Known open bugs: the plaza search may leave the map zoomed out after a fallback (C19, suspected); fork PRs #21-#32 fail Windows Clang's `-Wmissing-designated-field-initializers` (fix before upstreaming); Auto Battle kill count runs ~1 short and per-pass totals don't
always add up (#17); a failed hatch can leave the egg as party lead (M8a, Codex's
uncommitted work in `~/Projects/Arduino-Source-m8a`).
