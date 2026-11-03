# FireRed AutoStory: proposal

Status: draft, 2026-10-25. The full internal plan, with the evidence and line
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
checkpoints), solo with no trades. It changes no existing program: the
game-neutral parts (the run flow with checkpoints, retries from the last
save, start/end selection and stats; the tile-path walker; a closed-loop
screen driver) go into new shared folders from the start, with FRLG as
their only user, so nothing has to be moved later.

About 10 PRs, each 150–500 lines, each doing one thing:

1. Tile-path executor with unit tests, in a new `Pokemon/Navigation/`.
2. Elite Four farmer moved onto it, with no behaviour change.
3. A closed-loop screen driver (each input follows a screen read), then the new-game screens and naming keyboard on it.
4. Program shell: options, run flow, checkpoint list.
5. **New game → starter → rival battle. This is the first version users can run.**
6. Overworld detector.
7. Trainer-spotted and item-pickup watchers.
8. Resuming a walk after a battle or dialog interrupts it.
9. Trainer battle handler with unit tests.
10. **Parcel → Pokédex → Viridian Forest → Brock. This completes the pilot.**

Detectors come one per PR, with screenshot tests in CommandLineTests. Paths
are planned offline from the pret/pokefirered decompilation (trainer
positions and sight lines) and checked in as reviewed data.

## What I'd like reviewed first

PRs 1–2: the tile executor's result and resume types, and moving `walk_path`
out of `PokemonFRLG_EliteFourFarmer.cpp`. They touch existing FRLG code, and
the rest of the pilot builds on them.

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
