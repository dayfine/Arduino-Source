# AutoStory: shared navigation core and the two MVP branches

Status: plan, 2026-10-29. Replaces the T1/T2 sketch in [AutoStory.md](AutoStory.md) §5.1 and §7.7
and answers the open question in [AutoStory-Design.md](AutoStory-Design.md) ("how much of the
planner and map set is shared with FRLG"). Evidence: the BDSP lab branch
`bdsp/new-game-intro-v2` (00.04 runs unattended; 01.05 to Floaroma written) and the FRLG lab
branch `frlg/overworld-nav` (walked Nugget Bridge to Mewtwo on console B, 2026-10-29).

## 1. What the two games built, side by side

| Concern | BDSP (`Pokemon/Navigation`, AutoStory utils) | FRLG (`PokemonFRLG/Overworld`) |
|---|---|---|
| Direction, tile | `GridDirection`, `TilePos` | `MapDirection`, `MapPoint` (same thing again) |
| Map | `TileGrid`, hand-traced from DP maps: blocked, walk, grass, ledge (south), NPC, sign, door, unknown | `OverworldMap`, generated from pokefirered: walls, water, one-way tiles, ledges, doors / cave mouths / ladders / exit mats, elevation, objects with flags and wander boxes |
| Several maps | `MapSet`: grids placed on one world grid; no warps | maps joined by warps and edge connections with offsets |
| Planner | `plan_tile_route` (legs, extra for stopping in the open); `plan_robust_route` (a set of possible tiles, open legs a tile off) | search over (map, tile, elevation, surfing, facing, mid-leg), extra for stopping in the open, warps, Surf |
| Walking | `walk_route`: stick holds, 160 ms per tile across, 240 ms up/down; tracks possible tiles; re-plans after a trainer | `walk_overworld_route`: D-pad holds, 267 ms per tile (133 surfing); warps; Surf |
| Interruptions | hard-wired: trainer fight callback, fail on a wild battle in an open leg, dialog, transition | `OverworldInterrupt` list: battle flash (about 300 ms after the step), battle screen, message / Repel wore off (uses a Max Repel) |
| Facing | tracked; a tap only turns a player facing another way | tracked, and read from the player sprite |
| Trainers | sight lines on the grid | none yet (the decomp has them) |

Written twice: the direction and tile types, legs that end at a wall, the extra cost for
stopping in the open, open leg timing ((n - 1/2) tiles of hold), the turn tap, and what to do
when something stops a leg. Neither planner can serve the other game, because each is bound to
its own map class. That is the layering problem to fix: the shared code should call into the
game once per step, not own the tile legend.

## 2. Layers

**Shared, `Pokemon/Navigation/` (console-free):**

- `Pokemon_Grid.h`: `Direction`, `TilePos`.
- `Pokemon_LegPlanner.h`: the cheapest legs from a start to a goal, templated on a step model.
  The game supplies `press(state, direction)`: the next state, whether the step ends the leg
  (warp, starting or leaving Surf), and the facing after it; and `held_at(state, direction)`:
  whether holding further keeps the player on this tile (a wall, not a person or a map edge).
  The planner searches, merges steps into legs, marks legs that end at a wall, and charges for
  turns, leg ends in the open, grass, warps. FRLG's `press()` and BDSP's `TileGrid::step()` both
  fit as they are.
- `Pokemon_RobustPlanner.h`: planning over a set of possible tiles (BDSP's
  `plan_robust_route`), on the same step model. Only BDSP needs it (stick walking drifts); FRLG
  walks exactly.
- `MapSet` stays: one way for a game to build a step model (maps placed side by side). FRLG's
  warp graph is the other.

**Shared, `Pokemon/Navigation/Pokemon_LegWalker.h` (templated on the controller context, like
`run_until`, so `Pokemon/` stays free of `NintendoSwitch/`):**

- `LegMover`, per game: how to turn (tap), how to hold a direction (D-pad or stick), the time
  per tile for a leg (direction, surfing, biking).
- `LegInterrupt`, per game: a watcher, how long after the step it fires, how to clear it.
- `walk_leg()`, shared: hold the leg while watching every interrupt; on one, clear it, then
  hold a wall leg again, or work out how far an open leg went from when it was seen. Reports
  tiles walked and whether that is exact.

**Per game:** the map data and its generator, the step rules, timings, detectors, interrupt
handlers (trainer, wild battle, message, Repel), the facing reader, landmarks.

## 3. State: what is on screen, and where the player is

Two different things, kept apart:

- **Screen state**: which screen is up (overworld, dialogue, battle, menu, bag, black). The
  shared mechanism is the screen machine and step trace (`CommonTools/ScreenMachine`, already
  game-neutral); the per-game part is the detectors. Menus and scripted scenes run on the
  screen machine. While walking, the same detectors are the leg walker's interrupts, so the
  walker does not need a screen machine of its own. Missing: an FRLG overworld detector
  (overworld vs anything else without opening a menu), planned as D1.
- **Overworld state**: where the player is and what the world looks like. The pose is shared
  vocabulary: map, tile, facing, movement mode (walking, surfing, biking), plus how sure we are
  (one tile, or a set of possible tiles). The world state is per game but follows one pattern
  in both: maps are built from data with flags applied (FRLG event flags hiding objects; BDSP
  defeated trainers and smashed rocks), and the planner reads the result. The walker updates
  the pose after each leg and after an interrupt; the facing reader and landmarks (BDSP) or
  wall anchors (both) correct it.

Rule: the screen is read, the pose is tracked. A pose is never inferred from elapsed time
alone except for an open leg, and then it is marked inexact until a wall or a landmark fixes
it.

## 4. Branches and PRs

```
upstream main
 └─ nav/core                      shared: grid types, leg planner, leg walker, screen machine
     ├─ frlg/brock-mvp            FRLG: maps + step model, D-pad mover, interrupts, facing,
     │                            Brock route (new game -> Boulder Badge)
     └─ bdsp/floaroma-mvp         BDSP: TileGrid/MapSet on the planner, stick mover, interrupts,
                                  AutoStory 00 and 01 to Floaroma, nothing else
```

`nav/core` (one fork PR per item, stacked):

| ID | Contents | Tests |
|---|---|---|
| N1 | `Pokemon_Grid.h`, `Pokemon_LegPlanner.h` | a small made-up game: walls, a warp, a one-way tile, grass, open vs wall legs |
| N2 | `Pokemon_LegWalker.h` | scripted mover and interrupts: wall leg re-walked, open leg estimate, too many interrupts |
| N3 | `CommonTools/ScreenMachine` + `StepTrace` (from the BDSP lab) | none yet: the checks between reads need tests with small fake frames |

`frlg/brock-mvp` (on N1–N3): the FRLG overworld code moves onto the core types in its final
place (no move PRs of our own code later), then the Brock route. Includes the bag fix
(fork #65) and the facing reader. The Mewtwo route stays in the lab branch as the live test of
the walker. Brock route work follows §7.4 segments 00 and 01 of [AutoStory.md](AutoStory.md).

`bdsp/floaroma-mvp` (on N1–N3): only what AutoStory 00 and 01 need to reach Floaroma.

- In: the BDSP maps (Route 201 to Route 204 north, Jubilife, Oreburgh Mine, Ravaged Path) as a
  step model, `walk_route` on the leg walker, the trainer and wild battle interrupts, the
  AutoStory program with segments 00 and 01, the detectors and readers they use, the battle,
  dialogue, Pokémon Center and new-game utilities, the system keyboard detector (in
  `NintendoSwitch/Inference/`, its final home), their tests.
- Out: the old Lab Console and BDSP Lab Console commands (the Command File Runner, fork #58,
  replaces them in the lab), the lighting test program, the unused `walk_steps` /
  `walk_until_*` stubs, anything segment 02+.
- Risk: moving `walk_route` onto the shared walker changes code that ran unattended. The
  branch is not done until 00.04 runs on a console again and 01.05 reaches Floaroma.

Order: N1, then the FRLG child up to its planner tests (it already passes on the core idea),
then N2 with the FRLG walker on it (re-run the Mewtwo route), then the BDSP child. N3 can go
any time. The lab branches keep everything else and merge these branches in.

## 5. Checks before each branch is called done

- N1–N3: unit tests; no game code depends on anything not in the core.
- FRLG child: route tests; the Mewtwo route walked again on the console with the shared walker.
- BDSP child: offline plan tests for every segment plan; 00.04 and 01 run on the console.
- For both: `git diff --stat` against the lab branch shows only dropped lab code, no lost
  game logic.
