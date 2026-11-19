# AutoStory: from proof of concept to demo PRs

Written 2026-11-01 13:00. The user paused console work after FRLG's proof of concept: "next step
is to have demo PRs in good shape and have a full fledged plan". [PR_PLAN.md](https://github.com/dayfine/Arduino-Source/blob/notes/autostory-pr-tree/autostory/PR_PLAN.md) has
the older collector plan and the file-ownership rules; this file replaces its tree and order of
work.

Sizes per PR, and estimates for what remains: [AutoStory-FRLG-Sizing.md](AutoStory-FRLG-Sizing.md).

The approach (map-based, story as event tables, checks, interrupts, tests) is in [AutoStory-Architecture.md](https://github.com/dayfine/Arduino-Source/blob/docs/autostory-architecture/docs/design/AutoStory-Architecture.md) ([#133](https://github.com/dayfine/Arduino-Source/pull/133)).

## Where we are

- **FRLG proof of concept.** New game to the Boulder Badge on console B, in pieces:
  - 00.01–00.06 in the morning run;
  - 00.07, 00.08 and 00.09 on 2026-11-01 between 11:15 and 12:05.
  - Brock was beaten in 4 turns with HP green and no Potions used.
  - It is not yet 3/3 from a new game in one run.
- **BDSP.** New game to Floaroma passed on the v3 line earlier, with [#1717](https://github.com/PokemonAutomation/Arduino-Source/pull/1717) underneath. It is
  being rebuilt without [#1717](https://github.com/PokemonAutomation/Arduino-Source/pull/1717) as [#127](https://github.com/dayfine/Arduino-Source/pull/127) and [#128](https://github.com/dayfine/Arduino-Source/pull/128).
- **Known walker bug, FRLG.** A wild battle in the middle of a leg can leave the player a tile
  off the leg's line. "Walking to the wall again" then stops at the wrong wall, and nothing
  notices. This failed the forest gate 4 times.
  - On the passing run, the battles happened on the line, so nothing went wrong.
  - A wall-ending leg never checks how far it walked.
- **Story simulations pass on both games** but don't model the walker; see "Walker simulator".

## Target shape (user approved 2026-11-01): one linear shared stack, then the games

```
upstream main (= fork main)
├─ #43 AutoStory: shared start/end select databases (SV refactor)
│  └─ #44 AutoStory: shared story index
│     └─ #45 AutoStory: AutoStoryStats to Pokemon/AutoStory
│        └─ #114 Pokemon navigation core: grid, leg planner, LegWalker, route log, JSON maps,
│           │     story events, ScreenMachine; walker simulator
│           └─ #122 shared story steps, retry policy, run options (checkpoint dropdowns from
│              │     #43/#44, stats from #45, Checks, language)
│              ├─ FRLG: #117 maps + planner → #118 walker → #119 recovery
│              │        → #120 FRLG AutoStory MVP, new game → Brock (#123 folded in)
│              └─ BDSP: maps-v4 (JSON maps only) → #127 story maps, route records
│                       → #129 walker → [#128 split: actions, battle, inference]
│                       → #128 BDSP AutoStory MVP, new game → Floaroma (table like FRLG's)
├─ #107 lab tooling → #116 run records
├─ #125 route pictures (devtools/maps)
└─ docs: #100 design → #130 FRLG events after Brock; #113 notes (this plan)
```

PR links: [#43](https://github.com/dayfine/Arduino-Source/pull/43), [#44](https://github.com/dayfine/Arduino-Source/pull/44), [#45](https://github.com/dayfine/Arduino-Source/pull/45), [#100](https://github.com/dayfine/Arduino-Source/pull/100), [#107](https://github.com/dayfine/Arduino-Source/pull/107), [#113](https://github.com/dayfine/Arduino-Source/pull/113), [#114](https://github.com/dayfine/Arduino-Source/pull/114), [#116](https://github.com/dayfine/Arduino-Source/pull/116), [#117](https://github.com/dayfine/Arduino-Source/pull/117), [#118](https://github.com/dayfine/Arduino-Source/pull/118), [#119](https://github.com/dayfine/Arduino-Source/pull/119), [#120](https://github.com/dayfine/Arduino-Source/pull/120), [#122](https://github.com/dayfine/Arduino-Source/pull/122), [#123](https://github.com/dayfine/Arduino-Source/pull/123), [#125](https://github.com/dayfine/Arduino-Source/pull/125), [#127](https://github.com/dayfine/Arduino-Source/pull/127), [#128](https://github.com/dayfine/Arduino-Source/pull/128), [#129](https://github.com/dayfine/Arduino-Source/pull/129), [#130](https://github.com/dayfine/Arduino-Source/pull/130).

Superseded; the user closes them, with branches kept:
- BDSP: [#111](https://github.com/dayfine/Arduino-Source/pull/111), [#112](https://github.com/dayfine/Arduino-Source/pull/112), [#121](https://github.com/dayfine/Arduino-Source/pull/121), [#124](https://github.com/dayfine/Arduino-Source/pull/124), [#68](https://github.com/dayfine/Arduino-Source/pull/68), [#109](https://github.com/dayfine/Arduino-Source/pull/109), [#54](https://github.com/dayfine/Arduino-Source/pull/54) (the [#1717](https://github.com/PokemonAutomation/Arduino-Source/pull/1717) mirror).
- FRLG: [#67](https://github.com/dayfine/Arduino-Source/pull/67), [#123](https://github.com/dayfine/Arduino-Source/pull/123) (folded into [#120](https://github.com/dayfine/Arduino-Source/pull/120)).
- Docs: [#126](https://github.com/dayfine/Arduino-Source/pull/126) (moved to [#130](https://github.com/dayfine/Arduino-Source/pull/130)).

Upstream order: [#43](https://github.com/dayfine/Arduino-Source/pull/43), [#44](https://github.com/dayfine/Arduino-Source/pull/44), [#45](https://github.com/dayfine/Arduino-Source/pull/45) (small SV refactors, a good first handoff), [#114](https://github.com/dayfine/Arduino-Source/pull/114)'s parts, [#122](https://github.com/dayfine/Arduino-Source/pull/122), FRLG, BDSP, then tooling.

## What "good shape" means for each PR

1. **Only its own files.** `git diff parent...child --stat` shows nothing from a parent. Shared
   code lives in [#114](https://github.com/dayfine/Arduino-Source/pull/114) or [#122](https://github.com/dayfine/Arduino-Source/pull/122).
2. **Reviewable size.** One concern per PR, about 400 changed lines excluding map data and tests.
   Data tables are initializer lists. `std::format` for messages.
3. **Tests.**
   - Unit tests for the new code.
   - The story simulation for the program PRs.
   - The walker simulator for the nav PRs, once it exists.
   - Fork CI green on Mac, Windows clang and Ubuntu.
4. **Description.** 2–5 sentences, "Depends on #N" at the top, and one sentence on what was
   checked on the console.
5. **Console evidence.** The last console run that covers the PR, recorded in PR_PROGRESS.md.

## Work, in order

1. **Shared core settles ([#114](https://github.com/dayfine/Arduino-Source/pull/114), [#122](https://github.com/dayfine/Arduino-Source/pull/122)).** In progress, agent:
   - LegWalker and the shared route log move into [#114](https://github.com/dayfine/Arduino-Source/pull/114) and merge down the FRLG stack.
   - Decide: keep [#122](https://github.com/dayfine/Arduino-Source/pull/122) separate or fold it into [#114](https://github.com/dayfine/Arduino-Source/pull/114). Recommend separate: it is a different
     concern and can go upstream after [#114](https://github.com/dayfine/Arduino-Source/pull/114).
2. **Walker simulator ([#114](https://github.com/dayfine/Arduino-Source/pull/114)) and the walker fixes ([#119](https://github.com/dayfine/Arduino-Source/pull/119)).**
   - A grid world from the JSON maps that tracks the player's real tile and facing.
   - Holds move one tile per tile time; walls, NPCs and warps stop the player.
   - Seeded wild encounters on grass, with a detection lag; trainer sight from pokefirered.
   - Assertion: every story route, across many seeds, ends on its planned tile or fails loudly.
   - Model it on the measured behaviour: the m28 run's before/after screenshots of every step,
     `DebugDumps/AutoStory/legs`.
   - Then fix [#119](https://github.com/dayfine/Arduino-Source/pull/119) against the failing simulated cases:
     - the drift after a battle;
     - a wall leg that stops far short of its length fails with a screenshot.
3. **FRLG stack in shape ([#117](https://github.com/dayfine/Arduino-Source/pull/117)–[#120](https://github.com/dayfine/Arduino-Source/pull/120)).**
   - Fold [#123](https://github.com/dayfine/Arduino-Source/pull/123) into [#120](https://github.com/dayfine/Arduino-Source/pull/120).
   - [#120](https://github.com/dayfine/Arduino-Source/pull/120) keeps the 00.07/00.08 split, the save-card check on load, the Checks level and
     language-free reads.
   - Review each PR against "good shape". Check that [#117](https://github.com/dayfine/Arduino-Source/pull/117)'s route tests run in CI (maps are
     built in from [#118](https://github.com/dayfine/Arduino-Source/pull/118); decide whether to fold [#118](https://github.com/dayfine/Arduino-Source/pull/118)'s maps into [#117](https://github.com/dayfine/Arduino-Source/pull/117)).
4. **BDSP stack in shape ([#127](https://github.com/dayfine/Arduino-Source/pull/127), [#128](https://github.com/dayfine/Arduino-Source/pull/128)).** In progress, agent:
   - The walker moves to [#127](https://github.com/dayfine/Arduino-Source/pull/127) with battle handling injected.
   - Recorded stick holds are replaced by planned walks where maps exist.
   - The simulation runs 00.00 → Floaroma in English and in Japanese.
5. **Console verification, after 2–4.** One train at a time on console B:
   - FRLG 3/3 new game → Brock in one run each.
   - BDSP v4 new game → Floaroma, 3/3, first on the replaced 00.00–00.05 walks.
   - Each run gets route pictures ([#125](https://github.com/dayfine/Arduino-Source/pull/125)) and a run record ([#116](https://github.com/dayfine/Arduino-Source/pull/116)).
6. **Tooling PRs.**
   - [#116](https://github.com/dayfine/Arduino-Source/pull/116): retitle it as run records, attach route pictures to failed runs.
   - [#125](https://github.com/dayfine/Arduino-Source/pull/125): render route pictures as CI artifacts from the offline route tests. Not goldens.
7. **Gameplay plan (design; the user decides the targets).**
   - Trainer sight, facing and type in the map schema; the planner scores sight lines and grass.
   - Level targets per checkpoint. FRLG Squirtle, from the 2015 speedrun guide: 6 after Route 1,
     8 after the forest, 10 for Liam, 11 for Brock.
   - Sammy is the only planned forest trainer.
   - **Run modes (user, 2026-11-01):** SPEEDRUN / DEFAULT / SAFE. Strategy only (which
     battles to take, flee or fight wild Pokémon, heal threshold, what to buy and when); the Checks
     level (Thorough/Fast, screen verification) stays a separate option. SPEEDRUN follows
     pokeguy's LeafGreen 1:59:34 (`pokemon_sv/FRLG_POKEGUY_15934_VIDEO_NOTES.md` on agents):
     no Viridian shopping, flee every forest wild battle, Sammy L7→8, Liam to L11, Brock at
     L11 with no Potion (survives at 1 HP), Water Gun at L13, first shopping in Pewter.
     DEFAULT is today's table. SAFE heals at a threshold and takes extra wild battles to reach
     the level targets. Not built now; the table should name these choices so a mode can
     switch them.
8. **Upstream handoff (the user publishes; at most two open upstream).**
   - Order: [#114](https://github.com/dayfine/Arduino-Source/pull/114)'s parts (grid and planner, ScreenMachine, story events and maps), then [#122](https://github.com/dayfine/Arduino-Source/pull/122),
     then FRLG [#117](https://github.com/dayfine/Arduino-Source/pull/117) → [#120](https://github.com/dayfine/Arduino-Source/pull/120), then BDSP [#127](https://github.com/dayfine/Arduino-Source/pull/127) → [#128](https://github.com/dayfine/Arduino-Source/pull/128), then the tooling.
   - Each is folded to one commit per upstream PR when handed over.

## Decisions needed from the user

- Fold [#123](https://github.com/dayfine/Arduino-Source/pull/123) into [#120](https://github.com/dayfine/Arduino-Source/pull/120) (recommended), or keep it as its own PR.
- Keep [#122](https://github.com/dayfine/Arduino-Source/pull/122) separate from [#114](https://github.com/dayfine/Arduino-Source/pull/114) (recommended), or fold it in.
- Fold [#118](https://github.com/dayfine/Arduino-Source/pull/118)'s built-in maps into [#117](https://github.com/dayfine/Arduino-Source/pull/117) so [#117](https://github.com/dayfine/Arduino-Source/pull/117)'s route tests run in CI.
- Close the superseded PRs listed above once [#127](https://github.com/dayfine/Arduino-Source/pull/127)/#128 and the FRLG stack are in shape.
- Level targets and the trainer list per checkpoint.
- When to resume console runs: after steps 2–4, as proposed.
