# FireRed AutoStory: proposal

Status: draft under revision, 2026-10-28. The full internal plan, with the evidence and line
references, is [AutoStory.md](AutoStory.md).

## Problem

FRLG on `main` has most of the low-level pieces an AutoStory needs: dialog,
battle and menu detectors, fly, heal, a tested wild-battle exit, and a
frame-exact tile walker inside the Elite Four farmer. Nothing plays the story,
and users have asked for it
([ComputerControl#259](https://github.com/PokemonAutomation/ComputerControl/issues/259)).
A Poke-Controller script outside this project already plays FireRed to the
Hall of Fame with the starter alone, so the route is known to be automatable.

## What I'm building first: a pilot to Brock

A FireRed AutoStory from a new game to the Boulder Badge (about 12
checkpoints), solo with no trades. The
game-neutral parts (the run flow with checkpoints, retries from the last
save, start/end selection and stats; the tile-path walker; a closed-loop
screen driver) reuse existing shared modules where applicable; FRLG supplies its own
movement and observation implementation.

The first implementation milestone is a map-based demo from a verified
post-rival save: Pallet → Route 1 → Viridian Mart → Oak’s Parcel delivered
→ Pokédex verified and saved. Build it alongside a startup demo from a new
game through the lab rival and first verified save, then chain them.

Reuse the existing BDSP shared map/planner and screen modules with an FRLG
adapter. The demo must demonstrate a wild-battle interruption, verified
map/door transitions, and a deliberate failure followed by verified reload
and replay. These two game implementations determine what executor logic
is worth sharing. Elite Four migration and SV refactoring are independent.

The [FRLG demo work plan](FRLG-Demo-Work-Plan.md) defines JSON map resources,
startup prerequisites and optional SID Helper/Starter RNG handoffs. The
[map-reuse review](AutoStory-Map-Reuse-Review.md) specifies the work order,
source evidence and validation gates. Exact PR splits follow the runnable
slice; the previous ten-PR count is no longer a delivery commitment.

## What I'd like reviewed first

The small map subset and FRLG adapter, especially how observed position,
map transitions and save/reload postconditions constrain the next action.
New detector changes include screenshot fixtures. No general executor
interface is frozen before these behaviors work in the demo.

## The cost, honestly

Mysticial has said that SV AutoStory took over a year. Most of that time
went into hardware hours per checkpoint: writing a checkpoint, then tuning
it and running it again on a console until it is reliable. Shared code and
decomp-planned paths do not shorten that. So the pilot is there to measure
this cost before committing to the rest of the game (about 100 checkpoints
to the Champion).

## Go/no-go gate

The pilot passes if **every pilot checkpoint (new game → Boulder Badge,
about 12) succeeds 3 times in a row starting from its own save**, and this
takes **at most 3 Switch hours per checkpoint on average**, counting both
development and validation. If the pilot misses either target, I stop at
Brock. The pilot stays a working program, and I write up what it cost. If
it passes, the projection is roughly 300 hardware hours to the Champion, and
I'll post the measured numbers before continuing.

## Later, only if the pilot is used

- The remaining segments to the Champion, one PR per segment.
- With FRLG running on the shared run flow, I'd offer to migrate SV
  AutoStory onto it (pure moves with unit tests first) and offer the same to
  BDSP. None of this is a precondition for the FRLG work.
