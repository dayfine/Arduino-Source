# BBQ: measure changes before stacking them (plan, 2026-10-25)

Written at the user's request after R16. Freeze behaviour changes on
`lab/r10` until they can be measured one at a time.

## Why

R16 ran on 29 commits more than R12 and nobody can say which helped or hurt.
Full 2-hour runs are noisy (spawn drought, time of day, the board's quest mix,
start position), and "R12 = 742 BP/h" was one 66-minute run: the next run on
almost the same build (R12b) made 537. R11-R12b average ~630 (ROI.md).

## Reproducible run records (from now on)

Every run record states: worktree, branch, commit, binary SHA-256 (the
executable inside the .app), signing identity, profile name and its non-default
settings, start position, game state (time of day; game restarted or not),
start and stop times, and why it stopped. A run is not reported without them.

### Baseline build ("R12")

- R12: profile `bbq-r12`, 2026-10-23 17:20:31-18:37:39, app at
  `~/Projects/Arduino-Source-r10/build-prep/app/SerialPrograms.app`, branch
  `lab/r10`. Its row ("+ picnic retry, TM 800 ms wait") and the launch time
  match commit `365adbb31` (committed 17:20:30). **Inferred, not verified:** the
  binary SHA was not recorded and the app has been rebuilt since.
- R12b: one commit later, `efa64d507` (TM message closed by sight).
- Settings: R12 had NUM_RETRIES 1, OUT_OF_RETRIES reroll, exclusions = only
  wash-pokemon reroll, no direct flights (option did not exist). R16 used
  code defaults at `381936da9` etc. **The profiles differ, so the runs are not
  directly comparable.**
- To use it as a baseline: rebuild `365adbb31` in its own worktree
  (`git worktree add ../Arduino-Source-r12 365adbb31`), record the SHA, run
  with a copy of the `bbq-r12` profile.

### R16 segments (all `lab/r10`, worktree Arduino-Source-r10, profile bbq-r16, lab identity com.dayfine.pokemonautomation.lab)

| Segment | Commit | Time | Result / stop |
|---|---|---|---|
| R16a | a6106b22f (app rebuilt 02:08) | 02:11-02:29 | +40 net; start-up at Coastal flew wrong 7 min; stopped: out-of-retries=skip |
| R16b | 808c5ae2e | 02:32-03:33 | 225 BP/h; spawn drought (17/20 field attempts no spawn) until game restart |
| R16c | f763ac715 | 03:37-04:26 | ~374 BP/h, 10 ok/18 fail; stopped: fainted-lead prompt not detected |
| R16d | 4cd5656a9 | 04:46-04:52 | same stop (restarted without healing) |
| R16e | 2d9b21c1d | 05:29-05:33 | stopped: start-away change broke starts at Central |
| R16f | 381936da9 | 06:08:24-07:39:06 | 400 BP/h (net 600 in 91 min; earned 760, reroll spend 160), 20 done; stopped early by user: spawn drought from ~07:25 |

Binary SHAs for R16a-e were not recorded (gap; fixed going forward). R16f
(`381936da9`): `abbc8b8e92f0ba13c83005e32f541822fbd33f841c27a79fb812f2d5e6dafee6`.

### R16f per quest (attempt = quest label to panel re-read; seconds)

| Quest | ok/tries | min | per try |
|---|---|---|---|
| Sneak up | 0/6 | 13.1 | 120-166 |
| Tera-self | 3/5 | 10.1 | 98-145 |
| Catch bug | 1/1 | 7.5 | 448 |
| Travel 500 m | 5/5 | 7.2 | 86 |
| Auto Battle | 1/1 | 6.4 | 385 |
| Catch water/ice | 1/2 | 5.4 | 108, 218 |
| Photo swim/water/polar | 4/5 | 4.4 | 33-97 |
| Photo normal/fire | 0/4 | 4.2 | 56-75 |
| Make TM | 1/1 | 4.1 | 244 |
| Catch any | 2/2 | 4.0 | 85, 155 |
| Catch ghost/ground | 0/2 | 3.1 | 92-96 |
| Photo in-flight/psychic | 0/4 | 3.1 | 34-52 |
| Catch psychic | 0/2 | 2.5 | 69-79 |
| Photo canyon | 1/2 | 2.1 | 60-64 |
| Photo electric | 1/1 | 0.8 | 49 |

Rerolls: sneak-up 3, photo-fly 2, photo-savanna 2, one each catch-flying,
catch-ghost, catch-psychic, tera-self. Biggest sinks: sneak-up (13 min, 0
wins), photo normal/fire and in-flight (0/8). Bench these first. Script:
scratchpad `r16f_table.py` (copy into lab tools when the bench lands).

## Routine bench (built 2026-10-25)

- Program "BBQ Routine Bench" (`PokemonSV:BBQRoutineBench`), commit
  `304fbdaaa` on `lab/r10` (= `381936da9` + bench only). Routines: sneak-up,
  tera-self. Judge: a battle menu showed during the routine (all six R16f
  sneak-up failures were "no battle"); tera-self also needs no error.
- Baseline build: worktree `Arduino-Source-r12`, branch `lab/r12-bench` =
  `365adbb31` + bench (`4238f34e1`) + the three signing commits. Same lab
  bundle ID and certificate. Needs `PKG_CONFIG_PATH=/opt/homebrew/opt/opencv@4/lib/pkgconfig`
  and `-DWrapOpenGL_AGL=<MacOSX15.4.sdk>/.../AGL.framework/AGL.tbd` to configure/link.
- Executable SHA-256: A (r10, `304fbdaaa`) `c0608718611716b48a1495d505d98ded78e63ff796be8d9a2e19b80b94c5fefd`;
  B (r12-bench, `cb2537751`) `2cb1841c028c6a01167de0c50aefd0f3f4e438637b6156e0e6934a3777036103`.
- Profile `bbq-bench` (copy of bbq-r16's console setup; bench options at code
  defaults, chi_tra, not inverted). Both builds use it.
- Log lines: `BENCH,start,...,exe_sha256=...`, `BENCH,rep,i,pass=,seconds=,reason=`,
  `BENCH,summary,...` in the profile's SerialPrograms.log.
- START_AT_CENTRAL off (default) runs reps back to back like the farmer, so the
  new build's direct flights are included; on isolates the route itself.
- Protocol: restart the game first (no drought), start at Central, daytime.
  Alternate builds in blocks of 5 reps: A5 B5 A5 B5.

## Routine bench (design)

A lab-only program, "BBQ Routine Bench", in the r10 lab app:

- **Input:** routine, N (default 10), optional build label.
- **Each rep:** start on the Central fly point; set a fixed time of day; day
  skip (fresh spawns); run only the routine (not via the quest board); judge;
  return to Central; heal when a battle happened.
- **Evaluators (pass/fail + reason):**
  - Tera-self: battle entered, Tera used, battle won.
  - Sneak-up: battle started from behind (the sneak-attack message).
  - Typed catch: battle entered, species read, type matches, caught.
  - Photo routes: photo taken with a Pokemon in frame (screen check on the
    photo result).
  - Auto Battle: kills per minute and passes per heal.
  - Flights (central_to_*, direct, refly): landing banner names the target.
  - Reroll: slot read back as a new quest; BP charged once.
- **Output:** one log line per rep (build commit, binary SHA, routine, pass,
  reason, seconds) and a summary table (pass rate, median time).
- **A/B:** two builds' apps alternate on the same routine (ABAB...) so drift
  hits both; 10 reps each catches large effects (80% vs 40%), more for small.
- **Bisect:** for a routine that regressed, list only the commits since
  `365adbb31` that touch its code and bisect those with the bench.

## Order

1. Stop R16f at 08:09; per-quest table (success, seconds) vs R11-R12b; record
   it with the fields above.
2. Build the bench with Tera-self and Sneak-up first (both dropped most).
3. Bench `365adbb31` vs `381936da9` on those two; bisect if they differ.
4. Then the held changes, one A/B each: skip a day after no-battle attempts;
   set daytime at start; Coastal Flying route (needs the glide over the plaza,
   user's drawing); retry from where the player stands (R16f 07:30-07:32: two
   catch-ghost tries each spent ~33 s re-anchoring the map on Central to fly
   Canyon Plaza -> Canyon Plaza; closest-fly-point refly would be ~10 s).
   Also bench the Golett spot (both tries: no spawn, daytime).
   Split: same-quest retry = closest fly point; new quest = fly_from_anywhere.
   Drought: R16f 07:25-07:36 every field attempt empty although the date was
   skipped after each (sneak-up and catch-ghost alike). Candidate: after K (3)
   empty attempts in a row, save and restart the game (fixed R16b).
5. Only then a confirming 2-hour run; report per-quest, not only BP/h.

Also pending, unchanged: Codex isolated-hunt takeover (Charcadet, Chespin,
Chargestone), AZP without sandwich, AutoStory refactor PRs (fork #42-#47).
Notes on `agents` are committed locally; Codex's unpushed commit 82d2e1140
(1.8 MB evidence) blocks the push.

## Bench results

| Block | Build (exe SHA prefix) | Routine | Start | Result | Notes |
|---|---|---|---|---|---|
| A1 | `304fbdaaa` (c0608718) | sneak-up ×10, back to back | 07:56, game restarted 07:54, night | 10/10, median 74 s | reps 3, 9 (14 s) = wild attack right after the date-skip resume, not a sneak: real 8/10 |
| A2 | `3acb69ae0` (92109740; bench now ends at Central) | sneak-up ×30 | 08:10-08:42, same session, night (Switch clock ~04:xx) | 29/30, median 76 s | 7 passes were 14 s ambushes; route-started battles 22/23. No drought in ~50 min of session |

Reading: direct flights did not break sneak-up; R16f's 0/6 was session decay
(drought). A mass-outbreak notice appeared at Pride Rock at ~08:09; outbreaks
may also change the spawn pool. Judge gap: a wild Pokemon attacking the player
counts as a pass; tighten for sneak-up (battle must start after the route's
throw).

A2 rules out session length alone. Next hypothesis: time of day. Bench ran at
night (Switch clock 03:53 at 07:53 local; date skips keep the hour); R16f was
in-game daytime. A3: same build, Switch clock set to 12:00, sneak-up ×10.

| A3 | `3acb69ae0` (92109740) | sneak-up ×30, back to back | 08:46-09:15, Switch clock 12:xx (daytime) | 29/30, median 74 s | 11 ambush passes; route battles 18/19. Time of day ruled out |
| A4 | `3acb69ae0` (92109740) | sneak-up ×15, START_AT_CENTRAL | 09:16, daytime | running | R16f always flew in from another region |

R16f sneak-ups came right after catch-psychic, photo-water and catch-ghost
quests (Polar/Canyon) and landed with the region banner (草原區); bench reps
fly Pride Rock -> Savanna Plaza with no banner.
| A4 | `3acb69ae0` (92109740) | sneak-up ×15, from Central | 09:16-09:42, daytime | 15/15, median 67 s | no ambushes (reps start at Central) |
| A5 | `8af688791` (1ea633c3) | sneak-up ×15, from Polar Rest Area | 09:52-10:23, daytime | 13/15, median 82 s | ambush before 6 of 14 next flights (43%) |
| T1 | `fe5b9e482` (e68456f4) | tera-self ×15, back to back, FLY_DIRECTLY on | 10:25-10:57, daytime | 9/15, median 139 s | after a no-battle fail (returns to Central) next rep passed 6/6; after a win (KO, stays at Pride Rock through the day skip) next rep failed 6/8 |
| T2 | same | tera-self, FLY_DIRECTLY off | 10:59-11:37, daytime | 13/14 (stopped in rep 14's return flight), median ~134 s incl. return | ~142 s per success vs ~210 s in T1 |

Spawn observation: during A1-A5 a Pyroar mass outbreak sat at Pride Rock (notice
on many day skips) and caused the 14 s ambushes. R16f logged 0
UnexpectedBattleException and many empty looks at Pride Rock: the outbreak was
probably absent then. Hypothesis: sneak-up/tera-self success at Pride Rock
depends on the outbreak, i.e. on the day's spawn state, not on the build.
Farmer risk found: with FLY_DIRECTLY the farmer now waits at Pride Rock after
the day skip, where an ambush makes the next flight throw UnexpectedBattle.

Sneak-up passes in every condition benched (night, day, back to back, from
Central, from another region): 96/105 route attempts. R16f's 0/6 is not
reproduced; the likeliest cause is the day's spawn state at Pride Rock.
Held fix candidate (A/B later): after a Pride Rock quest, flee an ambush before
the next flight (43% of gaps in A5), or return to Central before the day skip.

T1 hypothesis: a KO'd Pride Rock Pokemon does not respawn when the day skip
happens with the player still standing there (FLY_DIRECTLY); leaving the area
first (R12 behaviour, return_to_plaza before the skip) lets it respawn.

**First measured regression: FLY_DIRECTLY (67f2ff114 made it default on).**
Tera-self 9/15 with it on vs 13/14 off, same build/day/profile (Fisher p≈0.03).
Staying at Pride Rock through the day skip after a KO keeps the spot empty.
Likely the same for catch quests (catch removes the target, the player stays,
the retry finds nothing: R16f catch-ghost/psychic 0/4).
Proposed change (one variable): default FLY_DIRECTLY off, i.e. back to the
R12 return-to-Central behaviour; later, a narrower version that leaves the
area before each day skip, benched against this.
Separate bug seen in T2 rep 14: return_to_plaza from Pride Rock flew to a fly
point it could not confirm as Central and gave up after 3 attempts.
Rep 14 detail: the player did land at Central (map centred on 中心廣場, own icon
hidden under the player), but no banner was read, so return_to_plaza retried
and could not find an icon to fly to. Fix candidate: when the closest-icon
search reports the icon occluded under the player, treat that as at Central.

## Changes made from bench results

| Commit (lab/r10) | Change | Evidence | Exe SHA-256 |
|---|---|---|---|
| `e97cf1a5d` | FLY_DIRECTLY default off | T1 vs T2 (9/15 vs 13/14) | 19a6b94d9a22115059b000924c5bed3efbcc7b18bec11c140a24998a17fc00c6 |

| `13fc139b1` | return_to_plaza: no banner then no fly = at Central (UNCONFIRMED) | T2 rep 14; unit tests (192 pass) | 35b7cdbe1b2f2d7edcba61c6bed5077d7010ea7ce7362225eeb5102cdbcf777d |

Profiles saved by older builds store FLY_DIRECTLY=true explicitly; a run that
should use the new default needs it unset or set false in its profile.

T3 (12:02): `13fc139b1`, tera-self ×15 back to back on code defaults
(FLY_DIRECTLY unset = off). Confirms T2 on the default and exercises the
return 15+ times. Before it: the start check failed twice with "No overworld
state found" at Central at night in rain (overworld detector gap; noted, not
fixed). A manual day skip (Switch clock -> 2060-10-27 12:12) cleared it.

T3 result: 5/15 (reps 1-6: 5/6; reps 7-15: 0/9). Only 5 day skips in T3: a
tera-self miss returned to Central with no day skip, so every later rep met
the same empty Pride Rock. The Pyroar outbreak icon at Pride Rock was gone
after the manual date change before T3 (map, 12:20); in-game daytime (map sun icon).
History (user recalled): R11/R12 ran Tera-self at the Chargestone court (6/13
battles); `aa2013e29` moved it to Pride Rock after R12 (12/12 then, with the
outbreak). Not benched yet: Chargestone vs Pride Rock A/B is queued.
In-game time of day is not tracked; the Switch clock is not a proxy (Switch
15:11 was in-game night). Map header shows a sun/moon icon: candidate signal.

| Commit | Change | Evidence | Exe SHA-256 |
|---|---|---|---|
| `0aebff1d1` | bench returns to Central only when away (ensure_at_plaza) | T3 end: return from Central looped | - |
| `09d32f406` | Tera-self skips a day after a miss too | T3 0/9 streak | 1dabe994172fc0e6a07bb6596a3723d6d1b0eeb71e60da4f0ab82b261bd5427f |

T4 (12:38): `09d32f406`, tera-self ×15, defaults, same game session, no
outbreak at Pride Rock.

T4 result: 12/13 (stopped early, signal clear) vs T3 5/15, same game day, no
outbreak. `09d32f406` (miss day skip) kept.

## Iteration rules (user, 2026-10-25 13:10)

- 10 reps per arm, not 15; stop early when the gap is already decisive
  (e.g. a 6+ pass difference, or Fisher p < 0.05).
- Option-level A/Bs alternate A and B rep by rep inside one bench run (same
  spawn state, same time of day), instead of separate blocks.
- Logging lands first so every remaining bench has it; old benches are not
  rerun.
- BP is not a constraint: the final confirming run is short (2 h), after the
  Coastal glide (4f).

## Bench v2 (13:55)

`539a384cb` lab flags + routine events, `b40a2ae9d` bench v2 (exe a57f5451...):
A/B arms alternate rep by rep (VARIANT: tera-self-spot, direct-leave-area);
per rep: events with seconds, opponent species, sky brightness at route end,
screenshots (route_done, no_battle, tera_turn, ambush), CSV at
`~/Projects/Arduino-Source-r10/BBQBench/<run>/reps.csv`. Pass = the route's own
battle (+ Tera turn for tera-self). Next: tera-self spot A/B, 20 reps.
Blocked 13:55: Switch asleep after T4 (colour bars); controller Home does not
wake it.

## Coastal hill (4f, 2026-10-25 16:30-18:20, `lab/r10`, BBQ Lab Console)

Route (a5770d8d4, 3e48bf640): Coastal fly point -> face 35 deg left of the landing view
({-0.57, +0.82}) -> mount -> `jump_glide_fly(3000, 3000, 3200)` -> L -> ride {+0.15, +1}
2 s to the user's spot in the middle of the hill (115 px W of the fly icon on the default
map zoom; ours 110 W, level). 14/14 flights landed on the hill (map-checked).
Riding off the south cliff and banking right flew into the hill's wall (climbing mode,
map won't open). Flight up is left stick UP (y=+1), as in `jump_glide_fly`.

| Sample | Spot | Battles | Flying | Others |
|---|---|---:|---:|---|
| S1 | hill edge (ride -0.2 x) | 6/10 | 2 (Pikipek, Trumbeak) | Deerling, Oddish, Cottonee x2 |
| S2 | user's spot | 8/10 | 2 (Trumbeak, Pikipek) | Grimer x3, Exeggutor, Exeggcute, Deerling |
| A/B A | user's spot | 6/10 | 3 (Toucannon x3) | Cottonee, Grimer, Slakoth |
| A/B B | fly point look-around | 0/10 | 0 | - |

Flying ~7/30 per look (~23%), Alolan Grimer 4/20 at the spot (~20%); ~75-100 s per look
incl. fly back + day skip. User saw a bird fly off while an Exeggcute battle ran: a
non-target battle costs the look. Fly point look-around is useless (0/10) despite the
map icons there ("don't trust Pokemon on the map").

Decision (user): catch Flying and catch Poison/Dark are possible but low ROI -> default
reroll rows (b038d6491; Poison/Dark moved from the 0/5 Muk glide to the hill, 3 looks).

Tooling: fly_to_closest_pokecenter_on_map() can't see Terarium fly points (fast-travel
icons) and retried 17x (~4 min); now capped and fly_back_to_coastal_plaza() tries the
fast-travel icon first (84aaffcbe, map-screenshot tests). Console: ssf_* overlap, shots,
fly_fast, jgf, hill; rerun-overwrite fix b87adad77.
