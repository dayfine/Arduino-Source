# Pokemon Navigation: how the route planner should work

This folder holds the navigation code that AutoStory programs share across
games (BDSP and FRLG today). This file is the guide for changing the
planner and the walker. The goal is a planner that gets smarter by learning
game rules, not by learning individual routes.

## The rule

The planner knows the game's rules. A checkpoint gives only map data and a
goal. If a checkpoint needs its own constant or special case to pass, a game
rule is missing: add the rule, not the special case.

## 1. Game rules live in tables, measured once

- **Movement table, per game.** What each input does on the console. For
  example: a D-pad tap moves exactly one tile; a held run overshoots by a
  measured amount; an arrow tile slides until a stop; a lift ignores input
  while it shakes. Values are measured on a console and cite the run they
  came from.
- **Behavior table, per game.** What other objects do, taken from the
  decompilations (pret/pokediamond, pret/pokefirered): trainer sight ranges
  and facings, NPCs that turn or wander, item balls that block tiles.
- The planner, the walker and the simulator all read these tables. A fix
  like "wait one second at this lift" is overfitting; "a lift freezes input
  for N frames" is a rule every lift uses.

## 2. Plan for certainty, not only distance

A route is only as good as the walker's knowledge of where the player is.

- Each story event declares the certainty it needs at its start (exact tile
  and facing, a set of tiles, or the map only) and the certainty it leaves
  at its end.
- Some moves restore certainty: walking into a wall or a funnel pins one
  axis no matter where the leg started. Some moves lose it: long open legs,
  steps the screen can't confirm, warps without a known landing tile.
- The planner prefers routes that pin the position early and often, and
  before any step that has to be exact (a door, an item, a trainer's line).
- A plan where an event needs more certainty than the previous event leaves
  is an error the lint and the simulator report, not something the console
  finds.

Example: Jubilife City's entrance to the Pokemon Center. North a few tiles,
east into the building wall (pins x), west one tile, north into the Center's
wall (pins y), then east and north into the door. It works from a start
several tiles off in either axis.

## 3. Risk is a cost, not a ban

- Tiles a trainer might see (in any direction it can face), tiles next to a
  wandering NPC, and open legs that could end a tile off all add cost.
- A leg must stay safe if it ends one tile past or short of its target.
- The cost weights are few, shared by every map, and set from console
  measurements. Do not tune them for one checkpoint.

## 4. Test against messy starts, never one golden path

- Every route is simulated from starts a few tiles off, with dropped taps,
  misread frames, turning trainers and seeded random NPCs. A route that only
  works from its declared tile fails in the simulator first.
- Every console failure becomes a fault the simulator can inject at every
  applicable step in both games, not a one-off test for that spot.
- The golden plans show when a change to a shared rule moves routes
  elsewhere. Review those diffs; an unexplained change elsewhere means the
  change overfits.

## 5. Human-drawn routes are tests

When a person draws a route, the planner should find it, or one it rates as
good, on its own. If it doesn't, its costs are missing a rule: find and add
the rule, then keep the drawn route as a test case.

## Changing the planner

1. Start from a console failure or a drawn route, with the failure frame.
2. Name the game rule that was missing or wrong. Measure it if it is a
   console behavior; cite the decomp if it is game logic.
3. Add it to the table, add the matching simulator fault, and show a test
   failing before the change and passing after it.
4. Run every checkpoint's sweep for both games and explain every golden plan
   that changed.
