---
name: map-fix-evidence
description: Gate for any change to a navigation map (tile grid, walls, doors, map points, coordinates) in AutoStory (BDSP/FRLG) or PokemonSimulation. The user approves every map fix; this skill builds the evidence pack they need - failure-point screenshot, a PNG of the desired route and the actual walked route drawn with PokemonSimulation's tools, and the wrong tile marked with how it bent the route. Use whenever a console failure tempts the conclusion "the map is wrong", before editing any map data.
---

# Map fix evidence

**The user approves every map fix.** A failed walk is first a route,
walk or perception problem; "the map is wrong" is a claim to prove, not
a conclusion to act on (2026-11-08: I called Oreburgh's Center door "one
column off" from two runs; the user was not convinced, rightly). Until
approved: no edits to map data, map points, grid files or sim maps.
A route change that stops depending on the doubtful tile (clip to walls)
is allowed meanwhile; say that it sidesteps, not fixes, the tile.

## The pack (all four, or it is not evidence)

1. **Failure-point screenshot**, shrunk (`sips -Z 640`), with a landmark in
   view (door, sign, counter, rock). If the failure frame has none, take one
   by hand on the CFR at the spot (see `console-iteration` recipes). Say in
   one sentence where the player stands, from the landmark, not the code.
2. **Desired route PNG**: the route the table/planner meant, drawn on the
   pinned map with PokemonSimulation's tool:
   ```sh
   cd ~/Projects/PokemonSimulation
   PYTHONPATH=src python3 -m pokesim.cli.route --game bdsp --map OreburghCity \
       --start 45,57 --goal pokemon_center_doorstep --png /tmp/desired.png --svg /tmp/desired.svg
   ```
   (`--goal` must be a stable tile, not a door/warp: use the doorstep. Runtime
   maps live in `data/games/<game>/maps/runtime/`. `pokesim.cli.render` only
   reads `pokenav-map/1` files.) If the table's legs differ from the planner's
   route, draw the table's legs (waypoints from the log's `[Route] Leg` lines).
3. **Actual route** on the same map: the tiles the console really walked,
   from the run log's legs plus the screenshot's end tile, in a second colour,
   in one PNG with the desired route. `pokesim.cli.route` draws one planned
   route; if it has no option for an extra walked path yet, add one in
   PokemonSimulation (small PR there: `--actual x,y ...` drawn with
   `route_drawing` helpers) rather than hand-drawing.
4. **The wrong tile**: mark it, and state in one or two lines how it bent
   the route: "map has (46,20) as wall, so the plan ran north 36 'to the wall'
   expecting a stop at row 21; the console walked through a door there."
   Plus the measurement that proves the tile's true kind: a CFR batch (tap
   into it, shot before/after; door = fade, wall = PSNR still) and, when the
   map came from a decomp, the decomp line (`pret-decomp-maps` memory).

Also check map frames: the AutoStory table's coordinates and the runtime
map's may differ by an offset; confirm a known point (a door, a sign) in
both before claiming a tile is wrong.

## Hand-off

Show the user: the screenshot, the one PNG (desired + actual + wrong tile),
the two-line claim, the measurement. Ask for approval of the map edit.
After approval: edit the map in the PR that owns the file
(`pr-owns-its-files`), add a sim/map test that fails on the old tile, and
link the pack (images on the agents notes branch run record) from the commit.

## Not evidence

- "The run went wrong near here." (could be drift, an NPC, a battle re-plan)
- One run. Two runs that disagree (one stopped at row 21, one walked through)
  point at something moving (the hiker) before they point at the map.
- The sim agreeing: the sim uses the same map.
