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
checkpoints), solo with no trades. It is built only in FRLG files and uses
its own copy of the SV-style run flow (checkpoints, retries from the last
save, start/end selection, stats), as BDSP's draft does. I accept that
duplication for now; merging the copies is only worth proposing once one of
them is used.

About 10 PRs, each 150–500 lines, each doing one thing:

1. Tile-path executor with unit tests, FRLG-only.
2. Elite Four farmer moved onto it, with no behaviour change.
3. New-game screen and naming-keyboard detectors.
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
- Having written a third copy of the run flow, I'd offer to move it into a
  shared module, migrate SV AutoStory onto it as pure moves with unit tests,
  and offer the same to BDSP. None of this is a precondition for the FRLG
  work.
