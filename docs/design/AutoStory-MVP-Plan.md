# BDSP AutoStory MVP: plan after run 1

Run 1 (English, Lucas, Piplup, console B) is recorded on fork PR #68 (comment "Runs"). Fork
#68 stays as the record of that run. The MVP to propose upstream is a compact series built
after the run, each PR small and doing one thing.

## Now
1. Finish run 1 to Floaroma: 01.04 (beat Roark, 3/3), then 01.05 (gym → Center heal →
   Oreburgh Gate → Route 203 → Jubilife → tag battle → Route 204 → Ravaged Path →
   Floaroma Center), on `bdsp/event-sandgem-center`.
2. Move the shared navigation core (`Pokemon/Navigation`: TileGrid, MapSet) from #68 into
   #66 (`nav/core`). In progress, done by a subagent.

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
