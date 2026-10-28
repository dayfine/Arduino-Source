# AutoStory: design of the closed-loop pieces

Companion to [AutoStory-Lessons.md](AutoStory-Lessons.md): that doc lists the failures, this one
describes the pieces built in response and how they fit together. Status as of 2026-10-28
(evening): 00.04, from the catch to Dawn in Jubilife, runs unattended on console B (r79);
00.05 is being stepped through. Each section says what is built, where, and what is still
planned.

The common idea: **never act on an assumption the screen could have checked.** Each piece
reads the screen, takes one step, and reads again. Durations are caps, not plans.

```
             story segment (00.04 after-catch, 01 Roark, 02 Floaroma ...)
                 |                       |                        |
        scripted screens          overworld walking            battles
        (intro, menus)            map -> plan -> execute       fight / escape / heal
                 |                       |                        |
          ScreenMachine           TileGrid + MapSet          battle helpers
          + StepTrace             planner, executor          (starter-agnostic)
                 \_______________________|________________________/
                                         |
                              walk_until / screen reads
                     (dialog, black screen, battle menu, screen change)
```

## 1. Screen machine (built)

`CommonTools/ScreenMachine/{ScreenMachine,StepTrace}` (branch `lab/console`, merged into
`bdsp/new-game-intro-v2`). Used by the BDSP new-game intro.

- A routine is a list of `ScreenState`s: a matcher, one step to take on that screen, the
  screens the step may lead to, and visit limits. States are tried in order; the last one
  matches anything ("other").
- Loop: wait for inputs to finish, read the screen (a state counts only when two frames
  150 ms apart agree), check, act. Stop after a terminal state acts.
- Checks between reads turn silent drift into an early, explained failure:
  - the new screen must be one the previous state lists in `next`;
  - no state may be seen more than `max_visits` times, or `max_in_a_row` in a row;
  - eight steps in a row that change nothing on screen is a dead end;
  - an overall timeout.
- `StepTrace` logs every recognised screen change with a numbered screenshot and every input,
  and a failure message ends with the last few screens ("dialog -> keyboard -> dialog").
  This is what makes a failed run explainable from one folder.

Why not a fixed script: a dropped A press or an extra dialog page shifts every later press of
a script. The machine does the right thing for whatever screen is up, so an extra page is just
one more "dialog" visit.

Use it for menu-like stretches with a small set of recognisable screens. Overworld walking is
not a good fit (the screen is always "overworld"); it uses the walk loop below.

## 2. Walking by the screen (built)

`PokemonBDSP_AutoStory_Walk.{h,cpp}`.

`walk_until(x, y, limit)` holds the stick in 400 ms chunks and returns why it stopped:

| Stop | Read from | Meaning |
|---|---|---|
| ARRIVED | caller's matcher | a landmark is on screen |
| DIALOG | short dialog box | an NPC or a trainer stopped us |
| TRANSITION | black screen with no battle after it | door, building, new area |
| BLOCKED | screen change < 10 (RMSD at 192x108) for two chunks | walking into a wall |
| LIMIT | time | none of the above |

Wild battles on the way (white flash, black screen then battle UI, battle menu) are escaped
and the walk resumes; they are counted, not returned.
`walk_to_talk` walks in 400 ms steps and presses A after each until a dialog opens, for NPCs
in tall grass, where the grass sway hides BLOCKED. `walk_expecting` fails with a screenshot if
the stop is not the expected one.

The caller decides what a stop means (lesson 4): a wall leg accepts BLOCKED or LIMIT; a trainer
leg accepts anything but TRANSITION and then waits for the dialog.

Known limits: 400 ms granularity is about 1.5 tiles; BLOCKED does not register in tall grass.
The probe (section 6) samples every ~100 ms instead.

## 3. Map: tile grid and map set (built, being extended)

`Pokemon/Navigation/Pokemon_TileGrid.{h,cpp}`, `Pokemon_MapSet.{h,cpp}`; data in
`PokemonBDSP/Programs/AutoStory/Data/` (Route 201, Sandgem Town, Route 202, `story_maps()`).

**Source.** BDSP keeps DP's overworld layouts, so each grid is traced from Bulbapedia's DP map
(16 px tiles). Fetched maps and the tracing scripts stay out of the repo; only the ASCII grid is
committed, with a comment naming the source image.

**Encoding.** One ASCII row per tile row, so a grid reads like the map and diffs like text:

```
#  blocked (trees, walls, ground items)   .  walkable      "  tall grass (encounters)
v  ledge, south only, hop over           N  NPC/trainer   S  sign
D  door                                  ?  off the map / exit
```

Named tiles (catch spot, Potion giver's talk tile, exits) and trainers with facing and sight
range sit next to the grid.

**Joining maps.** `MapSet` places each grid at an offset on one world grid, so a plan can cross
from Route 201 into Sandgem and on to Route 202. Overlaps are allowed only where both maps are
blocked; a conflict throws at start-up (and in the unit tests). `describe()` turns a world tile
back into "Route 201 (45,3)" for logs.

## 4. Planner (built)

`plan_tile_route(grid, from, to, costs)` returns straight legs (direction + tiles). It is a
cheapest-path search over (tile, direction) with costs per tile, per grass tile, per leg, and
a large extra for a leg that **stops in the open** rather than against a wall.

Why the wall preference: a leg that ends at a wall lands on the same tile however long the stick
is held past it (lesson 1). A leg that ends in the open depends on timing; the planner uses one
only when nothing else reaches the goal, and the executor treats it as the leg to localise
after.

Also built: `walk_to_wall` (where a straight walk stops, and whether it is held there),
`sight_tiles` / `legs_crossing_sight` (which legs walk into a trainer's view; reported, used to
expect a battle, not avoided, since we want the experience before Roark), `trace_legs` (the
tiles a plan visits, for tests and logs).

## 5. Executor, localisation and calibration (built for 00.04)

Built in `Utils/PokemonBDSP_AutoStory_RouteWalk` (`walk_route`), with `plan_robust_route` in
the planner. What it does differs from the plan below in these ways:

- **Plans for uncertainty.** The robust planner tracks every tile we may be on (at most 5
  after the start). An open leg of n tiles may end 1 + n/5 tiles off; wall legs bring the
  possible tiles back together. Offline tests check each plan the segment uses.
- **Turning is its own action.** A tap facing another way only turns, so an open leg turns
  first (`route_turn`). Measured speeds: 160 ms per tile across, 240 ms up or down; an open
  leg holds (n - 1/2) tiles' worth.
- **Things the map lacks.** A walk that stops early waits twice for the way to clear, then
  presses A (a trainer who saw us from the side, or someone standing in the way), fights
  if a battle follows, marks the tile blocked, and plans again.
- **Map changes** are checked against the location banner (OCR).
- **Every leg** saves a screenshot under `Screenshots/AutoStory/route/`, and the overlay and
  log show the checkpoint's steps, the current one and the next.

The original plan:

The piece that turns a plan into stick input:

- **Leg to walk.** Each leg becomes one `walk_until` with an *expected* stop:
  - ends at a wall: hold for `tiles x ms_per_tile` plus a margin; BLOCKED or LIMIT both mean
    "at the wall";
  - ends in the open: hold `tiles x ms_per_tile`, then localise;
  - enters a trainer's sight: expect DIALOG, run the trainer battle, then re-plan from where
    the trainer stopped us;
  - ends at a door or an exit: expect TRANSITION.
- **Localisation.** We know where we are when:
  - a wall stopped us (the stop tile is known from the grid);
  - the route banner appeared (OCR of "Route 201" etc. on entering a map);
  - a scripted scene started (it fixes a tile).
  After any surprise (wrong stop, extra battle), walk to the nearest walls on two axes to
  re-anchor, then re-plan from there. Re-planning beats replaying.
- **Calibration.** `ms_per_tile` (walking, ~267 ms in DP) and the start-up delay are measured
  on the console by probing known wall distances, not guessed. Running shoes would be a second
  speed; we walk.

**Routes first, towns only when trusted.** Routes are open ground between tree walls, where the
DP layout carries over and walls make good anchors. Towns changed more in the remake (buildings,
signs, roofs), so a town grid is used for planning only once the console check (section 6)
confirms it. Until then a town is crossed with screen-driven legs: walk to a wall or fence,
then to the exit, ending on the route banner (OCR) or the transition. Each map carries a
confidence flag that the executor checks before planning across it.

This replaces Butters' recorded legs in 00.04 (catch -> Potion giver -> Jubilife) first, then
segments 01 and 02.

## 6. Verifying the map on the console (next)

Before the executor trusts a grid, check it against the game:

1. `bdsp_probe <dir> <max ms>` Lab Console command: hold the stick, sample the screen every
   ~100 ms, report the time moved before the stop.
2. From the post-catch save, probe the walls in all four directions; compare with the grid's
   predicted tile counts to pin the start tile and fit `ms_per_tile`.
3. Repeat at each corner of the planned route (Route 202, Sandgem, Route 201 to the Potion
   giver), with a screenshot at each stop and the route banner at map joins.
4. Output: predicted vs measured tiles per probe; grid fixes where they disagree.

Probes run on open ground, since grass sway hides the stop.

Doubts to settle first (from tracing the DP images): Route 202's row offset against Sandgem
(the images overlap by only 8 px, so it may be off by one); Sandgem's lab sign at row 6 or 7
and which roof rows block; Route 201's tree line sits about 3 px off the grass grid; trainer
sight range (4 tiles is a guess).

## 7. Battles and healing (built)

- **Move choice:** each turn the Fight menu's move names are read by OCR, with the threshold
  set per button (the one under the cursor is light blue). The strongest known attack is
  used, so Water Gun once Piplup learns it, whatever the starter. If no attack can be read,
  the last move that is not a known status move is used: the starter's own-type attack.
- **Training:** before the Route 202 trainers, three wild battles in the catch grass. The
  run walks up and down between the tree walls north and south of the grass, never in short
  sideways steps (lesson 7).
- **Healing:** 00.04 first gets 10 Potions from the Poké Mart lady on Route 201, saves on
  Route 202, then heals from the Bag. That means full after training, and a Potion after any
  trainer that leaves the lead under half (`use_potion_first_pokemon(…, heal_below)`). The
  Pokémon Center routine (lining up with the lit doorway, the nurse) is kept for later.
- Items are found by OCR of their names (Potion), not by pixel templates.
- Wild battles while walking are run from.
- Planned: the opponent's type in the move choice (Roark), and a lead that faints (the
  "can't battle" party screen).

## 8. Stepping through new ground (Lab Console)

A Lab Console program on the console takes command files: walk until stopped
(`bdsp_walk x y ms`), presses, stick holds, detector reads and screenshots. New ground is
stepped through in batches of 5 to 10 commands (15 to 30 s), each ending with a screenshot,
and each stop and dialogue is written down before the automated version is written. This is
how the Poké Mart lady's tile, her dialogue timing, and Barry's position in the Trainers'
School were found, after whole-run attempts had failed on each.

## 9. Evidence and repeatability (built)

- The clock is set (2026-01-01 00:00:50) at the start of each attempt, because some landmark
  matchers depend on lighting.
- A failure saves its screenshot under `Screenshots/AutoStory/failures/` before the retry
  resets the game; each save prompt is saved under `Screenshots/AutoStory/saves/` (play
  time, badges).
- Runs stop at the first failure and close the game (lesson 3); retries are for real
  randomness, not for deterministic failures.

## Open questions

- Is BDSP really 1:1 with DP everywhere along the route? Section 6 answers this for the
  first three maps; buildings and towns changed most in the remake.
- Can tall grass get a stop signal (e.g. the player sprite's position instead of the whole
  view)? That would remove the special case in section 2.
- How much of the planner and map set is shared with FRLG (same 16 px tiles, same ledges)?
  The code lives in `Pokemon/Navigation` so it can be.
