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
