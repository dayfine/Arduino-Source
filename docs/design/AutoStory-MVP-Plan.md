# BDSP AutoStory MVP: plan after run 1

Run 1 (English, Lucas, Piplup, console B) is recorded on fork PR #68 (comment "Runs"). Fork
#68 stays as the record of that run. The MVP to propose upstream is a compact series built
after the run, each PR small and doing one thing.

## Now (2026-10-31)
1. Finish run 1: 01.10 (Route 204 North into Floaroma, the human's line as waypoints), then
   three passes each of 01.07 to 01.10 from their saves (01.09 has no program pass yet), on
   `bdsp/event-sandgem-center`. 01.01 to 01.04 have their 3x; 01.05, 01.06, 01.08 one each.
2. Move the shared navigation core (`Pokemon/Navigation`: TileGrid, MapSet) from #68 into
   #66 (`nav/core`). In progress, done by a subagent.

## Maps from the game's data (new, after Lessons 15)
The Platinum decompilation (pret/pokeplatinum) holds every map's collision, behaviour and
NPC placement; BDSP keeps the layout. The next version builds maps from it, not from images.
1. **Decoder in `devtools/maps`**: fetch a cell (`map_data_NNN.bin`) and its events file,
   emit the grid rows (`#` `.` `"` `v` `D` `W` `N` `n`) and an objects list in our frame, with
   the console-verified anchors as the offset. The scripts from the 2026-10-31 comparison
   (`decode.py`, `render.py`, `compare.py`) are the start.
2. **Re-derive the four image-traced maps** (Route 201, Route 202, Jubilife, Route 203) and
   apply the Sandgem and Oreburgh Mine fixes, keeping each map's console anchors; every
   changed map re-runs its checkpoints. Known BDSP-vs-Platinum differences are listed in the
   comparison report and in each map's header.
3. **Annotate**: every map file states source, console checks and status (`Data/MAPS.md`).
4. **Verify by probe**: a D-pad probe batch per map (walk to each anchor, count taps) run once
   before the map's first checkpoint; the human plays any stretch the plan has not walked.
5. **PokemonNavigation**: generate the JSON maps from the same decoder, so the data PR carries
   tiles the game states, and the one-map-one-component rule (#27).

## Planner and walker (next version)
- **Human-like plans**: long straight legs, corners at landmarks, the middle of corridors,
  no run-to-a-wall-and-back; waypoints from a recorded walk until the costs do this alone.
- **Taps everywhere a count matters**; a run to a wall only where a wall is the anchor.
- **Movement observed, not timed**: a step is a background shift of one tile; a bump or an
  NPC passing is not. Replace the whole-frame change with a shift measurement.
- **People**: trainers from the console (facing, range), wanderers waited for or talked to,
  never walled off.

## After run 1: the compact series (stacked on #66)
| # | PR | What |
|---|---|---|
| 1 | Shared navigation core (#66) | TileGrid, MapSet, leg planner, leg walker, screen machine. Game-agnostic. |
| 2 | Movement primitives | `walk_to_wall` (clip), `dpad_tiles` (tile-exact offsets, each tap checked), door probe by fade, leaving buildings. Measured values in comments (AutoStory-Lessons section 14). |
| 3 | Maps as data | Story maps (Route 202 to Floaroma) in the PokemonNavigation JSON schema (encounters, named points, objects), with a small loader. Replaces the C++ string grids. |
| 4 | Event spec | Event / Checkpoint runner (act, expect, recover) from 00.05. |
| 5 | Segment 00 on events | 00.00 to 00.05. |
| 6 | Segment 01 on events | 01.01 to 01.05, with checkpoints starting at fixed points (Center counters, just after Roark). |

Each PR has its own unit tests. Each route uses "clip to a wall, then D-pad offset" and is
checked on the console before its PR.

## Then
- **Route audit:** redo every older leg (00.04 to 01.05) as clip-to-wall plus D-pad
  (Lessons section 14 TODO).
- **Box the second Pokemon** at a Center PC.
- **Run 2 on the compact series:** a different language, the girl (Dawn), console B. It
  checks that detectors don't assume English or Lucas.
- **Upstream:** pitch on Discord first (CONTRIBUTING), then send the series one PR at a time,
  at most two open at once.

## The decomp review (pokemon_sv/BDSP_PLATINUM_DECOMP_REVIEW_20261031.md): taken and critiqued

Taken into the plan:
- **Events are initial placement plus movement rules**; a wanderer's position is observed
  (the Youngster under the Ravaged Path, Lessons 15). Static NPCs and trainers' facing and
  range come from the events file, verified on the console.
- **Scripts give trigger rectangles, NPC visibility by story stage, and scene side effects.**
  Plan into a scene's rectangle instead of searching for it (Rowan in Jubilife cost an hour of
  column searches; Barry's Route 203 trigger is 1x4 and row-dependent); hide or show story
  NPCs by stage; turn a scene's side effects into `expect` checks. The coupon weakness is real:
  "dialogue opened and closed" is not "coupon obtained"; expects look for the item line or jingle.
- **Interaction identities**: a rock, a sign, a person and the story NPC are different tiles
  (ROCK tile type, task #19), so A is pressed at the right thing.
- **Warp arrival tiles** (used for the Ravaged Path) and elevation for one-way edges.

Critique:
- **Order.** The review ranks NPC and script data first and treats terrain as done (Sandgem).
  Run 1 showed terrain is the first-order error: four maps traced at 3/4 scale, every
  checkpoint walking on them. Maps first, scripts second, both validated on the console.
- **Platinum is not Diamond.** The review's sources are pokeplatinum. pret/pokediamond
  (checked 2026-10-31) holds Diamond/Pearl's own `land_data`, `zone_event`, `mapmatrix` and
  scripts; BDSP keeps DP's layout, so pokediamond is the primary source and pokeplatinum the
  better-documented cross-check. Platinum-only edits (trainer spots, Looker, grunts) stop
  being guesswork.
- **The decomp fixes nothing in observation.** Today's failures were a time-counted leg
  blocked by a wanderer and bumps counted as steps; no amount of map data helps a walker that
  cannot tell a step from a bump. The walker's movement observation stays on the plan.
- **Where to start.** Not the president and clowns (00.05 passes three times); start where it
  hurts: re-derive the maps 01.06 to 01.10 walk on, then the two scenes that cost search time
  (Rowan in Jubilife, Barry on Route 203) from their trigger rectangles.
- Route 202's trainer count (four in #68, three plus a wanderer in Platinum) gets settled on
  the console during the re-derivation, like every other Platinum-vs-BDSP difference.

## Do the map diffs change the routes we run? (offline, 2026-10-31)

Every checkpoint walk planned on our grids and on the Platinum terrain (harness in the lab
notes, `pret/route_impact.md`):
- **Route 201, Sandgem's front row, Route 203, the Mine's first walk**: same legs in kind;
  only to-wall counts move (Route 203 south 10 to 11), and the wall absorbs it.
- **Two open legs change**: 00.04's "after training to the Center" south 9 becomes south 12
  (Route 202's bottom plus Sandgem's road is 12 rows, ours 9), and a Sandgem event walk south 6
  becomes 7 on one tile a console check decides ((29,2) is a wall in Platinum).
- **Jubilife**: all six 00.05 walks re-plan, four in kind; the console-placed NPCs sit a tile
  from Platinum's. 00.05 passes today, so a re-derived Jubilife needs its NPCs re-anchored on
  the console before use.
- **Why the short grids pass anyway**: vertical open legs are planned short and timed long by
  about the same 4/3 ("south 5 went 6", "7 went 9"), so the per-tile up/down time compensates.
  Re-deriving rows means re-measuring that constant, or taps everywhere.
- **Side finding**: with the planner's 20000-set cap, "To Jubilife" from the two 00.04 save
  spots finds no plan on our own grid in the harness; the unit test's earlier failure ("After
  training to the Center") hides it. Check before the segment 00 sweep.

## PR chain review (2026-10-31): what goes to the core, what stays per game

The chain: #66 `nav/core` (Grid, TileGrid, MapSet, LegPlanner, LegWalker) -> #79 event model
and #67 FRLG overworld nav (maps generated from pokefirered); #68 BDSP MVP on
`base/bdsp-1717-nav`, with run 1's work on `bdsp/event-sandgem-center` (130 commits past
#68's head, not on `nav/core`). Everything run 1 taught about walking went into
`PokemonBDSP/.../Utils/RouteWalk` and `Walk` because that branch has no core; the series
moves it where it belongs.

**Core (#66 and follow-ups), game-agnostic:**
- **Waypoint walks**: `walk_waypoints()` (plan to each corner of a recorded line) and
  `waypoint_plan_costs()`; later the human-like preferences in the planner itself (straight
  legs, few turns, corridor middles, no run-to-a-wall-and-back).
- **Tap legs as the open-leg default** in LegWalker (LegMover supplies the tap), with the
  blocked-tap handling: wait, tap the rest, then talk / mark / re-plan from the exact tile.
- **Movement observation as a game-supplied step observer** ("did that tap move us?"): BDSP
  gives screen change with its thresholds (step 21 to 50, bump 4 to 13), FRLG its own reader;
  later a background-shift measurement both can share.
- **Trainer stops** (sight lines, re-walk to the wall or count taps, fight, drop the trainer)
  and the time-slack rule "a block short of the wall is never the wall".
- **TileGrid legend aligned with PokemonNavigation's behaviors**: ledge by direction, door,
  warp, water, tall grass, sign, counter, rock (Rock Smash), unknown; a loader for the
  `pokenav-map/1` JSON so maps are data, not C++ rows.
- **Map annotation** (source, console checks, status) as part of the map data, not a comment.

**BDSP only (#68 and the compact series):**
- Map data generated from pokediamond (pokeplatinum as the cross-check), the per-map console
  anchors and known BDSP differences, Route 204 South/North and the Ravaged Path as they are.
- BDSP detectors and timings: dialog, battle menu, overworld, location banner, door fades,
  the Rock Smash prompt, tap thresholds, `talk_or_fight`, the Center heal.
- The segments and their waypoint lines (01.10's `floaroma_waypoints()`), checkpoint saves.

**FRLG (#67, the Brock MVP) takes the same plan:**
- It already generates maps from pokefirered (collision, elevation, warps, objects, metatile
  behaviors, wander ranges): the pattern BDSP copies. Add: ledges as one-way tiles instead of
  walls; the source/checks/status annotation per map; a probe batch per map before its first
  checkpoint.
- Story scenes from pokefirered's scripts (trigger rectangles, flags, side effects) are exact
  there, so FRLG starts with "plan into the rectangle" rather than searching; the human plays
  once only where a scene's trigger is unclear.
- Walk on taps with a step observer from the start; no time-counted legs.
- Keep #56's demo order (verified post-rival save, parcel trip) and its RNG handoffs.

**PokemonNavigation:**
- The image parser (`devtools/maps` in Arduino-Source #83) stops being a source: maps are
  generated from the decomps (pokediamond for BDSP, pokefirered for FRLG) into
  `pokenav-map/1` JSON. The `image` key and the parser stay for maps no decomp covers.
- The schema already holds what the decomps give (blocked, elevation, behaviors, npcs with
  facing/sight/wander, warps, connections, notes with sources). Add `rock` to the behaviors,
  an `encounters` default from tall grass / cave / water tiles, and `source`/`status`
  fields matching the per-map annotation.
- The four BDSP maps checked in (Jubilife, Route 201, Route 202, Sandgem) are image-traced,
  3/4 rows: regenerate them; one map = one connected component (#27).
