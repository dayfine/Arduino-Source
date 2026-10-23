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

**Quest value from logs (2026-10-22; re-run `python3 pokemon_sv/tools/quest_stats.py`).**
All lab profile logs; BP rewards from R3. A reroll costs 10 BP + ~25 s
(~90 s of quest time at the run's pace), so reroll quests whose seconds per
success are well above ~250 s.

| Route | Att | Success | s/attempt | s/success | BP | BP/min |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Make TM | 12 | 100% | 74 | 74 | 20 | 16 |
| Travel 500 m | 15 | 100% | 89 | 89 | 20 | 13 |
| Photo Swimming/Water/Polar | 10 | 70% | 70 | 101 | 30 | 18 |
| Auto Battle 10/30 | 13 | 100% | 464 | 464 | 20 | 2.6 |
| Bath | 5 | 40% | 267 | 667 | ? | ~2 |
| Sneak up (Pride Rock) | 19 | 16% | 109 | 689 | ~20 | 1.7 |
| Catch any/Normal/Fire (Pride Rock) | 14 | 14% | 148 | 1038 | ~20 | 1.2 |
| Photo Normal/Fire (Pride Rock) | 13 | ~0% | 96 | very high | 30 | ~0 |

Levels (user, 2026-10-22): (1) measure from all logs (this table); (2) hard-
coded decisions: reroll sneak-up, catch any/normal/fire, photo normal/fire,
bath via the exclusions table now; Auto Battle borderline, keep for M9a tests;
C13 later = adaptive reroll from running per-route stats; (3) fix ROI =
frequency x time lost: Pride Rock routes first (C11, C12, E4), then Auto
Battle (M9a, #21, C10), then bath.

**Reroll policy with chained rerolls and red quests (2026-10-22).** A reroll
draws a random new quest, which may also be bad. New-quest distribution from
148 slot changes in all logs (`pokemon_sv/tools/quest_draws.py`): travel-500
14%, pickup-10 11%, auto-10 9%, tera-self-defeat 9%, sneak-up 9%, photo-swim 8%,
make-tm 8%, photo-savanna 5%, catch-any 5%, photo-fly 4%, rest <2% each.
Renewal model (each draw: attempt up to 2 tries, reroll on failure; or reroll
at 10 BP + 25 s), long-run BP/hour, by bonus BP per completion (red reward /
completions per red; unmeasured):

| Policy | bad draws | draws/quest | +0 | +5 | +10 | +20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A: reroll only impossible | 15% | 1.52 | 226 | 294 | 362 | 498 |
| B: current (+ sneak-up) | 24% | 1.59 | 232 | 305 | 377 | 522 |
| C: B + Pride Rock catch/photo + bath | 37% | 1.67 | 267 | 354 | 442 | 618 |
| D: C + Auto Battle | 47% | 1.98 | 277 | 389 | 501 | 724 |
| F: D + all typed catches | 55% | 2.26 | 247 | | | |

Chained rerolls are priced in: D costs ~0.4 more rerolls per completed quest;
P(5 bad draws in a row) is 0.47^5 = 2%. F over-rerolls. Red quests raise the
value of a completion, which favours fast quests and rerolling slow ones.
Red quests (corrected 2026-10-22): a red ("strong") quest appears in the
TOP slot and pushes the blue quests to slots 2-4, so a 4-entry board means a
red is present and slot 1 is the red. (An earlier note here read slot 4 as red
and blamed Photo Savanna; that was wrong: Photo Savanna was a blue quest.)
Red episodes in all logs: lab salty-sandwich 8 min/0 completions; m3 tera-raid;
m5 wash-pokemon 25 min (23 reads) and 54 min (40 reads, 1 completion);
m5 hatch-egg x2; m5 sandwich-three; m7b auto-30 x2; R3 tera-raid, done,
+150 BP. Serebii: a strong quest comes after 10 completed quests, pays
130-200 BP, and costs 50 BP to reroll (10 for a blue). So a completion is
worth ~+15 BP extra (use the +10..+20 columns). Red policy (to decide with
more data): attempt reds the program does reliably (tera raid, auto-30,
hatch); reroll (50 BP) reds it can't (wash at 40%/267 s held the slot for
54 min).

**Revised with red quests separated (2026-10-22, `pokemon_sv/tools/reroll_model.py`).**
`quest_draws.py` now compares reads by content and splits the red top slot from
the blues; the old 148 count was inflated by the red shift. 502 reads, 138 blue
draws: travel-500 13%, pickup-10 12%, auto-10 12%, tera-self-defeat 10%,
make-tm 9%, sneak-up 8%, photo-swim 8%, catch-any 5%, photo-savanna 4%,
photo-fly 4%, typed catches 17% combined. 8 red draws: tera-raid 2, wash 2,
salty-sandwich, hatch-egg, sandwich-three, auto-30 (1 each). Red episodes:
lab salty-sandwich 12 min, m5 wash 27 and 58 min, hatch 3 min, sandwich-three
6 min, m7b-reuse auto-30 18 min, R3 tera-raid 11 min.

Model: 2 tries then reroll; blue reroll 10 BP + 25 s; a red after every 10 blue
completions pays 150 BP, red reroll 50 BP draws another red. Red times:
tera-raid 518 s, auto-30 ~1070 s, wash 40% x 267 s, hatch 191 s, sandwich reds
unmeasured (guess 50% x 250 s). BP/hour:

| Blue policy | blue only | + attempt all reds | + reroll wash/sandwich reds | ignore reds | draws/quest |
| --- | ---: | ---: | ---: | ---: | ---: |
| A: impossible only | 201 | 315 | 271 | 201 | 1.56 |
| B: + sneak-up | 204 | 322 | 276 | 204 | 1.62 |
| C: + Pride Rock catch/photo, savanna | 228 | 356 | 303 | 228 | 1.66 |
| C': C keeping catch-any (R4, tests #23) | 218 | 341 | 291 | 218 | 1.63 |
| D: C + auto-10 | 218 | 370 | 309 | 218 | 2.03 |
| F: D + typed catches | 171 | 369 | 295 | 171 | 2.54 |

Findings: (1) reds are worth ~+55% BP/hour; finishing them matters more than
any blue reroll choice. (2) Never reroll a red: the reroll costs 50 BP and draws
another red, so even wash at 40% beats it. What hurts is a red the program
keeps failing without the 2-try cap (m5 wash held 58 min): make sure
OUT_OF_RETRIES applies to the red slot, and fix wash (C-item). (3) With reds,
rerolling auto-10 (D) edges out C (370 vs 356) because completions carry the
red bonus; without reds it loses. Keep C for R4 (C' to test #23), revisit D
when C10 shortens Auto Battle. (4) Typed catches stay (F gains nothing).

14. C14 (added 2026-10-22 from R3, high priority: it ended R3): return to
   Central Plaza can land elsewhere and the program then dies. R3 08:17:
   return_to_plaza() found the Pokecenter icon 130 px off, got no "Fly" menu
   item, fell back to "zoom to max map level" and flew to the closest point,
   which was near Chargestone. Nothing checked the arrival; every central_to_*
   offset was then wrong; 3 x 2 min fly retries, then "Unable to fly to
   Chargestone Cavern!" ended the program (08:25). Existing code (not changed
   by the lab stack); bbq-m5 hit the same no-Fly error twice. Fix: (a) after
   return_to_plaza(), verify the arrival (the place-name banner "中心廣場" /
   "Central Plaza" shown on landing, or the map label) and retry the return
   if wrong; (b) make a failed quest fly a failed attempt (return to plaza
   from the map corner) instead of a fatal program error. N1 (#20) helps.
15. C15 (added 2026-10-22 from R5a): impossible quests sit on the board.
   `plan_quests()` only skips quests in `not_possible_quests` (pickup-10,
   photo-flying, catch-flying, UnableToDetect) and rerolls one only when nothing
   else is doable, then the *first* such slot, which is the red top slot (50 BP)
   when a red is impossible. R5a 12:55-13:08: red photo-flying + blue
   catch-flying held 2 of 4 slots for 20+ min; 1 completion in 10 min; no red
   income while the red slot is blocked. Fix: reroll an impossible blue (10 BP)
   on sight; reroll an impossible red (50 BP; a red pays 100-150 and blocks the
   next one); in the fallback pick the cheapest impossible slot. Data-driven
   tests in PokemonSV_Tests.cpp (BBQuestPlan group). Workaround now: list them
   in QUEST_EXCLUSIONS with action reroll (done for bbq-r5 at 13:19).

16. M15 step 1 (added 2026-10-22 by the user, from R5b; next after C15):
   batch quests on one trip. Today every quest is its own trip (fly, attempt,
   return, date change, panel read: ~1.5-2 min overhead). R5b 13:33-13:45 made
   5 trips for catch-electric, photo-swim, photo-coastal, sandwich. Step 1, no
   route planner needed:
   (a) same-target pairs: photo-X + catch-X (e.g. photo-ghost + catch-ghost on
       the board at 13:45), photo + sneak-up/catch-any: photograph first, then
       catch/sneak in the same encounter;
   (b) no-trip quests back to back (make-tm, sandwiches, hatch): skip the
       return / date change / panel read between them;
   (c) later: same-area pairs once each route records its destination plaza
       (photo-swim + photo-coastal at Coastal).
   Accidental multi-completions so far: R3b catch-any + sneak-up (ball from
   behind). Saves ~1.5 min per batched quest. Queue after it: C10, C12.

17. C16 (added 2026-10-22 by the user): a third quest category, "incidental":
   not worth a dedicated trip but may progress while doing other quests
   (candidate: pickup-10). Policy: keep (skip) while other slots are doable,
   reroll if it blocks the board or sits past a time cap. Evidence so far: none.
   A first log scan suggested pickup-10 left the board 19 times without a
   reroll, but every R4/R5 case had BP -10 (an unlogged reroll path), and m5 had
   no BP readings, so that claim is retracted. Test in R7 (no code): set
   pickup-10's exclusion row to skip (rows override C15); incidental if it ever
   leaves the board with BP +20 and no reroll; back to reroll after ~30 min
   with no progress. Code only if the test says so (`incidental_quests` set in
   plan_quests(), a variation on C15). Also: find the reroll path that doesn't
   log "Reroll selected" so log scans can see it.


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

## Run log

**R3b (2026-10-22 08:29-10:30, `lab/r3`, profile bbq-r3, stopped by rule at 120 min).**
No errors, no stuck steps. 23 quests done (the program's counter says 24), 2 saves.
BP 7234 -> 7994 in game: earned 870, spent 110 (rerolls), net +760, 398 BP/hour.
- **Auto Battle 10, 7 runs, stop-at-target (#17) fired every time.** Passes: 4, 5,
  5, 4, 3, 2, 4; time to target 349, 451, 446, 345, 256, 174, 340 s (mean 337 s,
  vs 464 s per quest before). Per-pass kills 1-5, once 9 in a 78 s pass.
  - The run of 5 passes at 09:13 counted 9/10 kills, but the board read found the
    quest done: the kill count undercounts by about 1 (same as R3).
  - Pass totals don't always add up: "1 kills, 4/10" after 2/10 (08:49) and
    "1 kills, 9/10" after 7/10 (08:52). The per-pass and running counters
    disagree; look at #17's counting.
- **10-completion bonus, measured.** The 10th completion (sandwich, 09:18) and the
  20th (travel-500, 10:04) each paid +130 BP instead of 20; the board had no red
  quest either time. So the in-game crown bar pays about +110 extra per 10
  completions (+11 BP per completion). This is not the red-quest mechanic; red
  quests (e.g. R3's tera raid, +150) come some other way. Use the model's
  "+10" column for the bonus and keep reds separate.
- **One attempt can complete two quests:** catch-any at 09:23 also completed
  sneak-up (a catch thrown from behind), +40 BP.
- **Rerolls:** excluded quests (photo-fly, pickup-10, sneak-up) on sight; tera-
  self-defeat (twice) and catch-any failed both tries and were rerolled under
  OUT_OF_RETRIES.
- **Panel reads:** 93 reads for 23 completions (4.0 per completion); several
  bursts of 3-4 reads in ~20 s around rerolls. This is C9; #22 (in R4) fixes it.
- **Ordering (A1):** at 10:14 the planner chose auto-10 with travel-500 on the
  board.
- Log noise: 18,320 `SpectrogramMatcher (Shiny Sound) spectrum timestamps are not
  continuous` stdout lines; harmless so far.

**R4 (started 2026-10-22 10:31, `lab/r4` = `lab/r3` + #21 + #22 + #23, profile
bbq-r4).** Exclusions: policy C' (C keeping catch-any, to test #23). Notes in
`~/Projects/Arduino-Source-r4/build-prep/R4_BUILD.md`. Ran 10:31-12:32, stopped by
rule at 120 min; no errors, no stuck steps.

| | R3b | R4 |
| --- | ---: | ---: |
| Quests completed | 23 | 38 |
| BP net (earned / spent) | +760 (870 / 110) | +910 (1160 / 250) |
| BP/hour net | 398 | 459 (peak ~540) |
| Quest-panel reads per completion | 4.0 | 2.9 (111 reads, 43 reused) |
| Auto Battle 10, time to target | 337 s mean, 7 runs | 230 s mean (145-344), 7 runs |
| Passes to target | 4, 5, 5, 4, 3, 2, 4 | 4, 3, 3, 2, 3, 4, 5 |
| Heals between passes | after every short pass | 0 of 24 passes (HP 96-100%) |

- **#21 confirmed:** HP never fell below 50%, so no heals; each pass is ~70 s
  instead of ~100 s. The remaining Auto Battle time is spawn density (1-9 kills
  per pass) and the extra pass at 9 counted kills (C10).
- **#22 confirmed:** reads per completion 4.0 -> 2.9.
- **#23 untested:** no catch quest was drawn in 38 quests. Carried to R5.
- 25 rerolls (policy C' plus OUT_OF_RETRIES), 1 failed-and-rerolled quest.
- **Red quest measured:** wild-tera (top slot) completed in ~2.5 min, +100 BP.
- **Kleavor (tera-self-defeat):** 14 attempts, 6 misses ("Did not enter
  battle"), 8 battles. Frames in the session scratchpad (`kleavor/<time>/sheet.png`): the
  glide always lands on the 黑奇石 field; on a hit Kleavor stands straight
  ahead, on a miss the field in view is empty. No clear time-of-day pattern
  (hit at night, misses at day and dusk). Older logs: Oct 20 00:12-01:08 had 14
  misses in a row on upstream main, each after a date change. Feeds C12: pan
  the camera with ZL lock-on before giving up; else glide on to the Scyther
  grassland. The main app log (`SerialPrograms*.log`, Oct 17-21: 76 completions)
  is not yet in quest_stats.py / quest_draws.py.

**R5 (started 2026-10-22 12:33, `lab/r5` = `lab/r4` + C14 #24, profile bbq-r5,
same settings as R4).** Tests the Central Plaza arrival check and, if a catch
quest is drawn, #23. Notes in `~/Projects/Arduino-Source-r5/build-prep/R5_BUILD.md`.
Ran in two parts on one build. **R5a 12:33-13:18:** 12 quests, net +190
(259 BP/h): a red photo-flying and blue catch-flying (both "not possible")
held 2 of 4 slots for 20+ min (C15). Stopped, added both to the reroll rows.
**R5b 13:19-15:19:** 34 quests, net +1,150 (604 BP/h; 9154 -> 10304 in game).
- **#24 plaza check:** 73/73 returns (R5a 21 + R5b 52) confirmed on the first
  landing, 0 retries, 0 false mismatches; one read came out "中心場" and still
  matched.
- **Reds:** Auto Battle 30 done in 9 passes / 682 s, 0 heals, +130 (m7b: ~1070
  s); sandwich-three done by a singleplayer sandwich, +130.
- **Auto Battle 10:** 201, 199, 276, 206, 268 s; 0 heals in 26 passes.
- **#23:** heal skip after a no-battle catch attempt fired once (catch-electric);
  no catch-any drawn, so catch-on-return is still untested.
- **Kleavor:** 8 tera-self attempts, 3 misses.
- Panel reads 102 for 34 completions (3.0), 29 reused. Rerolls 27.

**R6 (started 2026-10-22 15:19, `lab/r6` = `lab/r5` + C15 #25, profile bbq-r6
without the flying reroll rows, so C15 does it).** Notes in
`~/Projects/Arduino-Source-r6/build-prep/R6_BUILD.md`.
Restarted twice in the first 11 min for settings: OUT_OF_EGGS was
"keep-going", which skips hatch quests and blocks the red slot (the app warns
of this); the user had 400+ eggs, so box 13 (30 eggs) was selected and
NUM_EGGS = 30, OUT_OF_EGGS = reroll. **Final 15:30-17:30:** 35 quests, net
+1,240 (644 BP/h; in game 10334 -> 11574). No errors or stalls.
- **C15:** impossible quests rerolled on sight; no "enough BP" stop (balance
  ~10k). 14 rerolls total.
- **Hatch red:** done in the first 4 min, +220 BP (largest single payout).
- **#24 plaza check:** 47/47 confirmed first try (R5+R6: 120/120).
- **#23:** first catch-any draws. 16:26 attempt 1: no battle -> heal skipped;
  16:28 attempt 2: battle entered and handled (no early flee), ball thrown,
  quest completed. Catch-on-return (battle on the way back) still not seen.
- **Auto Battle 10:** 323, 138, 276, 269, 267 s; 0 heals in 22 passes.
- **Kleavor:** 8 attempts, 3 misses.
- Panel reads 92 for 35 completions (2.6), 22 reused.

**R7 (2026-10-22 18:10-18:49, `lab/r7` = `lab/r6` + #25 BP check, #26-#32,
profile bbq-r7 with pickup-10 set to skip).** Stopped on an error after 40 min:
`OperationFailedExceptionWithScreenshot: No more egg hatch after 10 minutes.`
8 quests, net +200 before that. Working live: #31 per-pass kills add up
(3+5+2=10); #32 read the board at 8/10, saw the quest, went again; #30 one
failed fly recovered on attempt 2; #27 reroll lines name slot and quest; #24
10/10 landings; #28 put Make TM first. No pickup-10 drawn (C16 no data).

**C17 (added 2026-10-22, high priority): hatch picked a hatched Pokémon as the
egg.** 18:39:36.417 "Cursor moved" to box 13 slot (0,0); 18:39:36.432 "Found
egg" 15 ms later; the slot held a Litten that R6's hatch had put back there.
The detector most likely read the summary panel before it updated (stale frame
from the previous slot). The Litten rode for 10 min, the program threw, and the
lead (Primarina) was left in the box (restored by hand at 19:05). Fix: (1) let
the panel settle and require the egg reading on two frames ~300 ms apart;
(2) after the swap, confirm party slot 1 is an egg, else swap back and skip;
(3) on any hatch failure, swap the lead back (overlaps M8a, Codex's
uncommitted work). Until fixed: NUM_EGGS = 0 in bbq-r6/bbq-r7.
