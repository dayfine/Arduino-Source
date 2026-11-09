# Route planner for AutoStory (design note)

Decisions from the user's grill session on 2026-11-08, all agreed. This note
records them; it is not a list of open questions. Perception (fades, banners,
walkers reporting warps) is in [EVENT_PERCEPTION_DESIGN.md](EVENT_PERCEPTION_DESIGN.md)
and is not repeated here. Route drawings for the cases below:
<https://claude.ai/artifact/12as58L8tEXQeS2CvF1JHY>.

## Problem

Three planners grew side by side, each with its own idea of cost:

| Planner | Where | Cost model |
|---|---|---|
| `plan_legs` | shared, `Pokemon/Navigation/Pokemon_LegPlanner.h` | `LegCosts`: per_tile 1, per_turn 2, per_open_stop 8, per_open_wild_stop 0 |
| `plan_tile_route` / `plan_robust_route` | BDSP, `AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h` | `TilePlanCosts` / `RobustPlanCosts`: per_leg 8, leg_not_at_wall 50, per_wander_tile 3; belief sets of up to `max_spread` 5 tiles |
| `plan_recovered_route` | BDSP, on the shared `find_wall_anchors` (`Pokemon_WallRecovery.h`) | anchors ordered by held tiles, then total cost (#278) |

The units are abstract, so the weights fight each other and the console
shows it:

| Case | What happened |
|---|---|
| 01.05 Gate | 7 legs where one run west works. |
| 01.08 | Waited on a wanderer. |
| 01.10 | Recovery picked the far wall by fewest held tiles, ignoring the goal. #278 fixed it by total cost. |
| 01.11 | 8 fixed taps from an arrival that can be x 10..12 landed under the wrong person. #279 clips to the field edge first. |
| 01.03, 00.05b | #277's zone rule made 01.03 `above_the_mine` go from 4 to 6 legs, and 00.05b worse. |

## Decisions

### 1. Objective: expected time to goal, in ms

The planner minimizes the expected time to reach the goal, including
recovery and interruptions. Costs are milliseconds taken from measured data
(walk and run pace, turn, fade, battle), not unit weights. Back-and-forth
and extra legs carry a hidden cost: more tiles in the open means more wild
encounters and more interruptions, each followed by a recovery. That cost
goes into the expectation, not into a separate penalty.

### 2. Position belief: at most two tiles

After a leg the player is on one of at most **two** tiles, along that leg's
axis. Event timing from the detectors gives ±1, no more.

- The two tiles merge only on a leg along that axis that ends at a wall, a
  corner or a warp, or by clipping perpendicular to a wall.
- Pin (collapse to one tile) only when the goal needs it. A Line goal or a
  Warp goal usually doesn't.

Planning becomes a search over at most two routes, not over sets of up to
five tiles (`max_spread`), which today can run for seconds and give up
(`max_searched`).

### 3. Goal kinds

| Kind | Done when |
|---|---|
| Tile | on that tile |
| Line | on any tile of a row or column |
| Warp | a fade OR a banner (perception note, slices 1–2) |
| Talk | on any tile next to the person, facing them |

Talk takes a **set** of sides: `next_to(person, sides)`. 00.05b's TV clown
can be approached from the east or the west, not only from the south; the
planner picks whichever side is cheaper.

### 4. Recovery: one rule

An interruption (battle, scene, blocked) gives an event timestamp; the
timestamp gives the position to ±1; the planner plans again from that
belief, to the same goal. No special recovery planner and no "nearest
wall" rule: the goal is in the cost, so 01.10's far wall loses.

The only wait is for a blocker that is crossing: about 5 s, then plan again.

### 5. Input per leg

| Leg | Input |
|---|---|
| ends at a wall or warp | run; the wall or warp sets the end tile |
| short, ends in the open | D-pad taps (whole tiles) |
| long, ends in the open | run, and carry ±1 into the belief |

The stick is dropped for planned legs.

### 6. Split between shared and per game

| Shared (`Pokemon/Navigation`): what to do | Per game: how it looks or moves |
|---|---|
| goal kinds, the two-tile belief, planning, the recovery rule | detectors, input timing, ms costs |

### 7. Wanderers

- A leg that runs **along** a wander zone (same axis, two or more tiles
  inside it) is forbidden.
- **Crossing** a zone is allowed, at `per_wander_tile` (in ms).
- A wanderer is confirmed from outside its zone, with timed shots.
- **Sidestep on collision.** When a run hits a wanderer, step one tile into
  the side lane and keep running. This is preferred over waiting.

The user's 01.03 example: run down column 45; the pokéfan wanders
(45,37..39). On collision, sidestep east one tile and keep running south
until the fade. With ms costs and this rule, the user's route

    (46,21) → (45,21) → (45,37) → (46,37) → (46,57)

should win over #277's six legs.

## Order of work

| # | Change | Console check after |
|---|---|---|
| 0 | 01.08 fix (#271, #277), done | |
| 1 | #270: route-plan golden dump, so every later change shows its plan diff | |
| 2 | Goal kinds and two-tile planning | |
| 3 | #268 slices 1–2 (door = fade OR banner; walkers report warps), removing #269's stopgaps | 01.05 clean pass |
| 4 | ms costs and the input rule per leg | |
| 5 | The single recovery rule | 01.03–01.11 |
| 6 | FRLG adopts the shared planner | |

Each code change carries sim tests; the golden dump (1) shows the plan
change for every story row, including the evidence cases above.

## Non-goals

- No change to the maps. Map fixes still go through the map-fix evidence
  gate.
- No new perception here; that is the perception note.
- No stick input for planned legs.
