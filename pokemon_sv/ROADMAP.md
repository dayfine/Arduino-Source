# BBQ roadmap: payoff, sharing and order

Status: **approved by the user, 2026-10-21.** It reorders the milestones in
[MILESTONE_STATE.md](MILESTONE_STATE.md) by payoff and dependency.

## Decisions (user, 2026-10-21)

- **The order in section 5 is approved,** with one change: the M9a cave
  experiments (E1, a cheaper spawn reset; E2, more kills per visit) get Switch
  time **before M8a**. Run them with hatching disabled and the intended lead
  checked at the start, so the M8 party bug can't confound them.
- **M9a PR 3's heal threshold defaults to 50%.** That makes PR 3 a behavior
  change, so its title says so and maintainers decide.
- **Who implements is open.** The goal is one automated dev loop that
  implements, tests and validates, then hands over to the PR process.
  - Codex could run it, but its quota is lower.
  - Claude can run it instead, by orchestrating agents through a workflow
    when the user asks for one.
  - Either way, the roles in AGENTS.md still hold: one owner per branch, and
    PRs only upstream when ready. Paths are
relative to `SerialPrograms/Source/VideoGames/PokemonSV`.

## 1. Battle efficiency: BBQ Auto Battle vs the Material Farmer

| | Kills per minute | Share of a trip spent battling | Kills per trip | Heals |
| --- | ---: | ---: | ---: | --- |
| Material Farmer, tonight (sandwich active, North Province) | **5.3** (92 kills in 17.5 min) | ~75–80% (140–280 s clearing in 185–340 s trips) | 11–43, about 20 typical | none in 17 min |
| BBQ Auto Battle 10, correct lead | ~2.5 (10 kills in 227–249 s) | ~26% | 0–4, typically 1–2 | every pass |
| BBQ Auto Battle 30, correct lead | ~1.9 (34 kills in 17.5 min) | ~26% | ~3 | every pass |
| BBQ Auto Battle 10, wrong lead (M8 party bug) | ~0.9 | | | every pass |

The BBQ route kills two to three times slower. Four reasons, from the code and
the logs:

1. **One clear per trip.** The Material Farmer walks forward and clears again
   several times per trip (`run_lets_go_iteration()`); BBQ clears once, then
   leaves.
2. **A heal and a re-fly every pass**, about 28 s; see [M9a_PLAN.md](M9a_PLAN.md).
3. **No sandwich.** Encounter power raises spawn density for the farmer.
4. **Different areas.** Chargestone's court may simply have fewer spawns. This
   is unmeasured, and part of M9a's E3.

## 2. Doing more quests in an area before returning to Central Plaza

Today every quest starts and ends at Central Plaza:

- each route flies there with `central_to_*()`, a fixed map-cursor offset from
  the Central Plaza fly point (`Programs/PokemonSV_Terarium.cpp`);
- each route then calls `return_to_plaza()`.

Several quests share a spot (`quest_photo_navi()` and `quest_catch_navi()` in
`Programs/Farming/PokemonSV_BlueberryCatchPhoto.cpp`):

| Area | Photo quests | Catch quests |
| --- | --- | --- |
| Coastal Plaza (Exeggutor-A and others) | coastal, grass, dragon, poison, dark, steel | grass, dragon, steel, poison, dark |
| Canyon Plaza (Golett and others) | canyon, ghost, ground, bug, rock, fighting | ghost, ground, fighting, bug |
| Savanna Plaza (Pride Rock) | savanna, normal, fire, flying | any, normal, fire, flying |
| Polar Rest Area / Polar Classroom 1 | flying, psychic; swim, water, polar | psychic, water, ice |

A board with, say, photo-coastal and catch-grass could do both on one trip:
photo first, then catch the same target. That saves one return and one flight,
about 45 s per pair. Pairs are common because boards draw from these lists, but
their frequency hasn't been measured.

**What it needs (new milestone N1, below):**
- **Fly from anywhere.** `central_to_*()` only works from Central Plaza.
  `return_to_plaza()` already shows the robust pattern: move the map cursor to
  a corner first, then to the target, then snap to the nearest fly point. A
  general `fly_to(TerariumFlyPoint)` built on it removes the "start at Central
  Plaza" assumption. Checking the fly point's name before flying also covers
  issue #263 item 7.
- **Planner ordering.** `plan_quests()` (#1640) groups to-do quests by area.
- **A start-and-end contract per route.** Each route declares where it starts
  and ends. The round loop calls `return_to_plaza()` only when the next quest
  is in a different area.

## 3. What to reuse from the Item Printer and Material Farmer code

| Code | Where | Reuse in BBQ |
| --- | --- | --- |
| `LetsGoHpWatcher` plus `check_hp()` threshold healing | `Inference/Overworld/PokemonSV_LetsGoHpReader.h`, `Programs/Farming/PokemonSV_MaterialFarmerTools.cpp` | **Yes**, M9a PR 3. `check_hp()` takes `MaterialFarmerOptions`; split out a version taking only the threshold. |
| `auto_heal_from_menu_or_overworld()` (heal from the menu) | `Programs/Battles/PokemonSV_Battles.cpp` | **Maybe.** It heals anywhere, with no flight to the nurse, but uses items. Option: heal from the menu when HP is low and the next quest is in the same area. |
| `run_lets_go_iteration()` (several forward clears per trip) | MaterialFarmerTools | **Yes, as a pattern**, for M9a E2. The path is area-specific; the loop is shared. |
| `run_from_battles_and_back_to_pokecenter()` | MaterialFarmerTools | **Partly.** BBQ's `return_to_plaza()` already handles battles while flying back; reuse the "run from incidental battles" part for E2 walks. |
| Sandwich making with encounter power | `make_sandwich_material_farm()`, `SandwichRoutines` | **Experiment.** A type-encounter sandwich could make typed catch and photo targets appear (M12a), and raise Auto Battle spawns. It costs about 3 min and ingredients, and lasts 30 min. |
| Consecutive-failure cap with game reset | `run_material_farmer()` | **The pattern**, for M13 and M9a PR 4 bounds. |

## 4. Other shared code for navigation and finding targets

- **Fly points on the map:** `fly_to_closest_pokecenter_on_map()`,
  `FastTravelDetector`, `MapPokeCenterIconDetector`, and AutoStory's
  `move_cursor_towards_flypoint_and_go_there(..., FlyPoint)`. Together they
  give "move near, then snap to a detected fly point", the basis for N1.
- **Facing and realigning:** `DirectionDetector` (the minimap compass) and
  AutoStory's `realign_player()` and `realign_player_from_landmark()`, which
  turn the player to a heading from the minimap. BBQ routes use fixed glides
  and camera presses; this would make arrival less sensitive to timing (M12a).
- **Object-guided walking:** AutoStory's YOLO helpers
  (`move_player_until_yolo_object_detected()` and similar) walk until a trained
  object is on screen. The models are trained on story landmarks, not on the
  Terarium's wild Pokémon, so there's no direct reuse without new training
  data. The helpers' shape (walk, detect, bound the rounds) is still the right
  one for a target search.
- **Reacting to unexpected battles:** AutoStory's
  `do_action_and_monitor_for_battles()` and `handle_unexpected_battles()`
  overlap with the Material Farmer's wrapper. Pick one for BBQ walks.

## 5. Milestones by payoff and dependency

Payoff is measured where we have data. On the M7 run with a correct lead, Catch
Any and Sneak Up spent 28.75 minutes, 48% of the hour, failing to find a
target.

| Order | Milestone | Payoff | Depends on | Coding (agent) | Live test |
| --- | --- | --- | --- | --- | --- |
| 1 | **M8a** restore the party after a failed hatch | Correctness gate. A wrong lead slows every battle route about 2.5× and confounds all measurements. | none | ~1 h | 30 min, focused |
| 2 | **M14** BP income/expense/net in the status | Lets every later change be judged in BP/hour | #1628 BP reader | ~30 min | with any run |
| 3 | **M9a PRs 1, 2, 4** (seam and pass log; stop at target; bounds) | ~30 s per Auto Battle quest; no endless loop | none (independent of the stacks) | ~1.5 h | one Auto Battle 10 and one 30 |
| 4 | **M12a step 1**: target-acquisition diagnostics | Finds the cause behind 48% of an hour | none | ~1 h (logs and frame captures per attempt) | recorded runs |
| 5 | **N1** fly from anywhere, with the name check | Enables area batching; fixes issue item 7 | none | ~1.5 h | fly to each Terarium point once |
| 6 | **M12a step 2**: target search or alternate target | The largest single payoff, if step 1 finds a fixable cause | step 4, maybe N1 | ~2 h | several runs |
| 7 | **M15 (new)**: area batching | ~45 s per same-area pair | N1, #1640 planner | ~1.5 h | several runs |
| 8 | **M9a PR 3** heal only when HP is low | ~2–5 min per Auto Battle quest | M9a PR 1; maintainers' default decision | ~45 min | one Auto Battle 30 |
| 9 | **M11a** no rinse after a failed scrub | ~1 min per failed wash | none | ~1 h | wash attempts |
| 10 | **M13** navigation recovery bounds | Prevents rare long stalls (one return took 8m50s) | none | ~1 h | injected failures |
| 11 | **M7 follow-ups** | Accounting accuracy | M7 | ~45 min | with any run |
| 12 | **M8b** startup egg box | Convenience | M8a | ~1.5 h | one startup |
| 13 | **M10** Pickup; Flying catch and photo | Low and unknown | investigation | unknown | unknown |
| exp. | **Sandwich encounter power** for typed catch, photo and Auto Battle | Possibly large for M12a's typed targets | M12a step 1 | ~1 h (the sandwich code exists) | A/B runs |

**What changed from the old order:**
- **M14 moved up**, since BP/hour is how everything else gets judged.
- **M12a's diagnostics moved ahead of** M11a, M13 and M8b: they target the
  largest measured loss.
- **N1 and area batching (M15) are new**, from your point 2.
- **M9a is split**: its cheap, default-preserving PRs go early, and the
  behavior-changing heal policy waits for maintainers to decide the default.

**Effort:** agreed, the coding adds up to about a day: roughly 12–15 agent-hours
for items 1–12. The real cost is per-PR testing and review. Each PR needs:
- its own native tests;
- a 30–60 minute live run on the lab profile;
- a fork PR, then an upstream PR once its parents merge.

So the pace is set by Switch time (one run at a time) and maintainers' reviews,
not by coding. Items 1–3 can be coded together and then validated in one
combined lab run, because they touch different code. Each still gets its own PR.

## Execution queue (approved by the user, 2026-10-21)

Coding and live runs are two separate queues. Coding needs only a checkout and
runs in parallel. Live runs need the Switch, one at a time.

**Code (in order):**

1. C1: M9a PR 1, the Auto Battle seam and per-pass log.
2. C2: M9a PR 2, stop at the target.
3. C3: M9a PR 4, attempt limits.
4. C4: M14, the BP status.
5. C5: M8a, restore the party.
6. C6: M12a step 1, diagnostics.
7. C7: N1, fly from anywhere.
8. C8: M9a PR 3, heal at 50%.
9. C9 (added 2026-10-22 by the user, ahead of M15): M7 follow-ups for panel
   reads. R3 showed 3 avoidable reads (~12–15 s) after each failed quest that
   leads to rerolls: (a) the out-of-retries reroll's verify read is not handed
   to scheduling; (b) M7a only reuses 4-slot verify reads, so boards without a
   bonus quest never reuse; (c) the before-dispatch read runs right after a
   fresh scheduling read. Fix (a) and (c) first; (b) needs care because a
   3-slot read can be a missed 4th slot.
10. C10 (added 2026-10-22 by the user): stop Auto Battle when the game says
   the quest is done. R3's Auto Battle 10: the program heard 9 kills after
   pass 4, but the game had already completed the quest (the user saw a new
   quest appear on the board during pass 4), so pass 5 and its heal (~100 s)
   were wasted. Decided (user): no pop-up detector; optimize when the board
   is read instead. Read the board (~5 s, existing reader) after a pass, before
   the heal, once the heard count is within reach of the target (e.g. heard
   >= target - typical kills per pass); if the quest is gone, end the attempt
   as completed and skip heal/re-fly. Fold this into the same read-timing
   policy as C9, so a read taken here also serves the next scheduling read.
   Also from R3: the stop check runs only after the 5 s ZR mash, so the stop
   lags up to 5 s.

11. C11 (added 2026-10-22 from R3): catch-route fixes in quest_catch()
   (Programs/Farming/PokemonSV_BlueberryCatchPhoto.cpp).
   (a) Don't flee from the target: the 25 s battle wait can expire just before
   the battle starts; return_to_plaza() then sees a battle and runs. R3
   08:00:21 (battle 0.4 s after the timeout) and bbq-lab 19:12 did this.
   Fix: after the timeout, keep watching for the battle menu for a short
   grace period (a few s) and handle it as the catch; consider a longer wait.
   Pride Rock route (catch any/normal/fire) over 26 logged attempts: 22 no
   battle, 2 in-time battles, 2+ battles just after the timeout.
   (b) Skip the nurse heal when the attempt had no battle (keep the day skip,
   which respawns the fixed encounter): ~15-20 s per failed attempt.

12. C12 (added 2026-10-22 from R3, after C11): catch-any route and day skips.
   (a) Catch any accepts any Pokémon, but its route goes to Pride Rock, where
   22 of 26 logged attempts saw no battle in 25 s. Route catch-any to a dense
   spawn area instead (e.g. the Chargestone battle court: ~6 Pokémon in front
   of the player on landing, 2026-10-22 07:27) and engage the nearest one.
   (b) Pride Rock is not labelled a fixed spawn in the code (unlike the
   Duosion and Horsea routes); 52poke lists Litleo as a random Savanna spawn
   and a fixed Pyroar only after the Zero storyline, with no location. If the
   target is a random spawn, the ~15 s Home-menu day skip after each attempt
   doesn't help (leaving the area already resets random spawns): keep it only
   for fixed-spawn routes.

**E4 (run, added 2026-10-22):** find a better spot for sneak-up and catch-any.
Sneak-up uses the same Pride Rock route as catch-any and completed 3 of 33
logged attempts (bbq-lab 0/12, bbq-m5 2/18, bbq-m3 0/2, bbq-m4 1/1). Try about
10 attempts each at Pride Rock (baseline), the Chargestone court, Coastal
Plaza and Canyon Plaza; count battles and sneak-up successes. Feeds C12; if a
spot works, sneak-up can leave the reroll list.

**To analyze from run logs (added 2026-10-22 by the user):**

- A1, quest order within a round. The planner keeps board order; nothing
  ranks quests by cost or success rate. R3 07:54: board was Make TM, Catch
  any, Travel 500 m (+ an unread 4th); it ran TM, then Catch any (the target
  was in view, but the player fell and the attempt failed), with travel-500
  last. Questions for the logs: how often does travel-500 complete for free
  while doing other quests (the pre-dispatch read catches this), vs its cost
  when run alone; per-type success rate and time, to order cheap/reliable
  quests first; whether a failed quest should push cheap ones ahead. Output:
  an ordering rule (e.g. by expected BP per minute) with evidence.
- A2, Catch any failure where the target was visible but the player fell
  (R3, ~07:57-07:59): feeds M12a; check what the route does on uneven
  ground.

**Run (in order):**

1. R1: E1, fly Chargestone → Chargestone. This needs no code: it can be done
   by hand through the keyboard control in lab/CONTROL.md.
2. R2: E2/E3, a recorded cave visit (needs C1).
3. R3: a combined lab run of C1–C4.
4. R4: M8a (C5).
5. R5: M12a diagnostics (C6).
6. R6: the N1 fly check (C7).
7. R7: the heal policy (C8).

**Status (2026-10-22 morning):** all code items except C5 are draft fork PRs
with passing native tests; none has run on the Switch yet.

| Item | Fork PR | Branch | Base |
| --- | --- | --- | --- |
| C1 M9a PR 1 seam + pass log | #15 | `bbq/auto-battle-seam` | `main` |
| C2 M9a PR 2 stop at target | #17 | `bbq/auto-battle-stop-at-target` | #15 |
| C3 M9a PR 4 attempt limits | #18 | `bbq/auto-battle-bounds` | #17 |
| C4 M14 BP status | #16 | `bbq/bp-status` | `bbq/bp-reader-crop` |
| C5 M8a restore party | — | Codex's uncommitted work in `~/Projects/Arduino-Source-m8a` | |
| C6 M12a diagnostics | #19 | `bbq/target-diagnostics` | `main` |
| C7 N1 fly from anywhere | #20 | `bbq/fly-from-anywhere` | `main` |
| C8 M9a PR 3 heal below 50% | #21 | `bbq/auto-battle-heal-threshold` | #18 |

R3 build is ready (not launched): local branch `lab/r3` = lab stack + #15–#18 +
#16 ported onto the lab round loop, app at
`~/Projects/Arduino-Source-r3/build-prep/app`, notes in `build-prep/R3_BUILD.md`.
The first launch needs the user (camera/microphone prompts). R1 needs the player
outdoors (no flying indoors), so do it during R3 from Central Plaza.

**Start, once the Item Printer run ends and the game is saved:** begin C1–C4,
and do R1 while they build. Code ahead of the Switch, so each run's build is
ready when the Switch is free.
