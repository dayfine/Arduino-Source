# AutoStory: code size by PR, the upstream split, and the queue

Re-measured on 2026-11-01 at about 17:30 (the first version was from 13:10 the same day). Each row is
`git diff --numstat parent...child` on the fork branches, counting added lines. "Code" excludes
tests, map data (JSON), docs and cmake. The plan and PR tree are in
[AutoStory-Demo-Plan.md](AutoStory-Demo-Plan.md); the milestones are issues
[#135](https://github.com/dayfine/Arduino-Source/issues/135)–[#147](https://github.com/dayfine/Arduino-Source/issues/147).

Each fork PR is a collector. Upstream gets smaller PRs of about 400 code lines or less, one concern
each. Each section below lists its fork PRs, then the upstream PRs they become. Labels: C (shared
core), F (FRLG), B (BDSP), G (game simulator), R (RNG and names), L (lab and run records).

## Summary

| Track | Upstream PRs | Code | Tests | Data | Needs the shared core? |
|---|---|---|---|---|---|
| C. Shared core (M1) | 11 | 4,238 | 3,010 | 151 (test maps) | is the core |
| F. FRLG MVP (M2) | 15 | 6,837 + ~150 new | 1,503 + ~200 new | 3,202 | yes, except F1 and F2 |
| B. BDSP MVP (M3) | ~38 | 14,853 | 2,889 | 1,243 | yes, except B1 (and C5) |
| G. Game simulator (M5) | 2, +1 per game | ~1,200 new | ~600 new | odds tables | partly: grid and LegWalker (C4, C6, C7) |
| R. RNG and names (M7, M12) | 5, then 5–8 for M12 | 1,325 + ~300 new | 610 | | no, until the AutoStory hand-off |
| L. Lab tooling and run records (M4) | 11 | 5,179 + ~150 new | 1,898 + ~100 new | | no, except L11 |
| **All tracks** | **~82**, plus M12 | **~34.2k** (32.4k written) | **~10.8k** (9.9k written) | 4.6k | |
| **FRLG MVP path (C + F)** | **26** | **~11.2k** | **~4.7k** | 3.4k | |

"Written" means the code is on a fork branch now; "new" is an estimate for PRs not yet written.
[#131](https://github.com/dayfine/Arduino-Source/pull/131) (agent integration, about 7 PRs, ~2k) and the
features in section 7 are not counted.

## 1. Shared core (M1, [#135](https://github.com/dayfine/Arduino-Source/issues/135))

### Prep: small SV refactors

| PR | What | Code | Tests |
|---|---|---|---|
| [#43](https://github.com/dayfine/Arduino-Source/pull/43) | Shared start/end select database, now with its test | 56 | 40 |
| [#44](https://github.com/dayfine/Arduino-Source/pull/44) | Shared story index | 33 | 59 |
| [#45](https://github.com/dayfine/Arduino-Source/pull/45) | AutoStoryStats moved to Pokemon/AutoStory | 38 | 0 |
| **Total** | Ready to send upstream first | **127** | **99** |

### Navigation and story engine ([#114](https://github.com/dayfine/Arduino-Source/pull/114), [#122](https://github.com/dayfine/Arduino-Source/pull/122))

[#114](https://github.com/dayfine/Arduino-Source/pull/114) gained today: the MapData fields BDSP and FRLG
maps need, the story engine's move from Pokemon/Navigation to Pokemon/AutoStory, and story lines on
the overlay.

| PR | Part | Code | Tests |
|---|---|---|---|
| [#114](https://github.com/dayfine/Arduino-Source/pull/114) | Grid types and leg planner | 488 | 406 |
| | ScreenMachine / StepTrace | 329 | 0 |
| | MapData: schema, loader, CMake embedding | 764 | 435 (+151 test maps, 85 cmake) |
| | LegWalker, route log, tile poses | 610 | 373 |
| | Story events (with the overlay status) | 781 | 677 |
| | Story segments | 297 | 374 |
| | **[#114](https://github.com/dayfine/Arduino-Source/pull/114) total** | **3,269** | **2,265** |
| [#122](https://github.com/dayfine/Arduino-Source/pull/122) | Story steps and one retry policy | 628 | 646 |
| | Story run options (dropdowns, stats, Checks, language) | 214 | 0 |
| | **[#122](https://github.com/dayfine/Arduino-Source/pull/122) total** | **842** | **646** |

Logging is split across the two halves:
- **Route log** (`Pokemon_RouteLog` in [#114](https://github.com/dayfine/Arduino-Source/pull/114)): one line format for a route walk in both games. FRLG calls it from [#120](https://github.com/dayfine/Arduino-Source/pull/120)'s branch, BDSP from its walker in [#129](https://github.com/dayfine/Arduino-Source/pull/129).
- **Checkpoint and event log** (`[Segment]`, `[Checkpoint]`, `[Event]` in [#114](https://github.com/dayfine/Arduino-Source/pull/114)'s StoryEvents): shared by both games already.
- **Gap:** the route log is grid-bound: tiles, directions, tile counts. 3D games (SV, LA) have route legs too, but no tiles. Planned fix: a grid-free core (leg label, target, how it ended, how the end is known, and an optional position the game formats) with the grid lines as one form of it. The same records become the run events of [#132](https://github.com/dayfine/Arduino-Source/pull/132), so the log and the run record carry one format.

### Upstream split: 11 PRs, 4,238 code, 3,010 tests

| Upstream | What | From | Code | Tests | Needs |
|---|---|---|---|---|---|
| C1 | Shared start/end select database | [#43](https://github.com/dayfine/Arduino-Source/pull/43) | 56 | 40 | main |
| C2 | Shared story index | [#44](https://github.com/dayfine/Arduino-Source/pull/44) | 33 | 59 | C1 |
| C3 | AutoStoryStats to Pokemon/AutoStory (move) | [#45](https://github.com/dayfine/Arduino-Source/pull/45) | 38 | 0 | C2 |
| C4 | Grid types and leg planner | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 488 | 406 | main |
| C5 | ScreenMachine and StepTrace | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 329 | 0 | main (only BDSP's actions use it) |
| C6 | MapData: schema, loader, CMake embedding | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 764 | 435 | C4; split 2 if asked |
| C7 | LegWalker, route log, tile poses | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 610 | 373 | C6 |
| C8 | Story events, overlay status | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 781 | 677 | C7 |
| C9 | Story segments | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 297 | 374 | C8 |
| C10 | Story steps and retry policy | [#122](https://github.com/dayfine/Arduino-Source/pull/122) | 628 | 646 | C9, C3 |
| C11 | Story run options | [#122](https://github.com/dayfine/Arduino-Source/pull/122) | 214 | 0 | C10 |

The SV refactors (C1–C3) and the navigation chain (C4–C9) don't depend on each other; they meet at C10.

## 2. FRLG MVP (M2, [#136](https://github.com/dayfine/Arduino-Source/issues/136))

[#117](https://github.com/dayfine/Arduino-Source/pull/117)–[#120](https://github.com/dayfine/Arduino-Source/pull/120)
now sit on the shared MapData. [#117](https://github.com/dayfine/Arduino-Source/pull/117) lost about 1,070 code
lines in the move.

| PR | What | Code | Tests | Map data |
|---|---|---|---|---|
| [#117](https://github.com/dayfine/Arduino-Source/pull/117) | FRLG maps (generator + 25 JSON maps) on MapData, route planning, facing reader | 2,182 | 189 | 3,202 |
| [#118](https://github.com/dayfine/Arduino-Source/pull/118) | FRLG walker: steps, warps, grass, signs, ledges; battle-flash detectors | 693 | 0 | |
| [#119](https://github.com/dayfine/Arduino-Source/pull/119) | Finding the player again after an interrupted step | 827 | 476 | |
| [#120](https://github.com/dayfine/Arduino-Source/pull/120) | FRLG AutoStory, new game → Brock: table, events, actions, battles, Mart/bag and save checks, simulation | 3,135 | 838 | |
| **Total** | | **6,837** | **1,503** | **3,202** |

Still to write for the MVP:

| PR | Where | Size (code + tests) |
|---|---|---|
| Walker fixes: drift after battles; fail a wall step that stops far short | [#119](https://github.com/dayfine/Arduino-Source/pull/119) | ~150 + 200 |
| Run-event hooks (shared code and FRLG) | track L, L11 | ~150 + 100 |

Then the console check: FRLG 3/3, new game → Brock, in one run each. The game simulator moved to its own track (G).

### Upstream split: 15 PRs, 6,837 code (+~150), 1,503 tests (+~200)

| Upstream | What | From | Code | Tests | Needs |
|---|---|---|---|---|---|
| F1 | FRLG readers: item list, save card, naming-screen detector | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 427 | 125 | main |
| F2 | FRLG battle flash watcher and battle-screen detector | [#118](https://github.com/dayfine/Arduino-Source/pull/118) | 212 | 0 | main |
| F3 | FRLG map generator and 25 JSON maps | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 318 | 0 (+3,202 data) | C6 |
| F4 | FRLG maps on MapData | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 540 | 189 | F3 |
| F5, F6 | FRLG route planning, in two halves | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 1,014 | 0 | F4, C4 |
| F7 | Player facing reader | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 310 | 0 | F4 |
| F8 | FRLG walker | [#118](https://github.com/dayfine/Arduino-Source/pull/118) | 481 | 0 | F2, F6, C7 |
| F9 | Re-anchoring on a known tile | [#119](https://github.com/dayfine/Arduino-Source/pull/119) | 203 | 155 | F8 |
| F10 | Recovery after an interrupted step | [#119](https://github.com/dayfine/Arduino-Source/pull/119) | 624 | 321 | F9 |
| F11 | Walker fixes: drift, short wall steps | new | ~150 | ~200 | F10 |
| F12 | Story actions | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 661 | 0 | C10, F1 |
| F13, F14 | Story battles, in two halves | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 925 | 0 | F12, F11 |
| F15 | Program, story table, events, simulation | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 1,122 | 713 | F14, C11; split 2 if asked |

F1 and F2 read the screen only and can go upstream now. F1's naming-screen detector is also in
[#81](https://github.com/dayfine/Arduino-Source/pull/81) (122 lines); whichever lands first carries it.

## 3. BDSP MVP (M3, [#137](https://github.com/dayfine/Arduino-Source/issues/137))

| PR | What | Code | Tests | Map data |
|---|---|---|---|---|
| [#134](https://github.com/dayfine/Arduino-Source/pull/134) | Story maps as generated JSON, read through MapData | 733 | 420 | 1,243 |
| [#127](https://github.com/dayfine/Arduino-Source/pull/127) | Navigation: story maps, planner, route records | 1,920 | 967 | |
| [#129](https://github.com/dayfine/Arduino-Source/pull/129) | Walker, with battles passed in | 2,825 | 254 | |
| [#152](https://github.com/dayfine/Arduino-Source/pull/152) | ShortDialogDetector: long first lines (new, behavior change) | 98 | 4 | |
| [#148](https://github.com/dayfine/Arduino-Source/pull/148) | Game actions, battles and their detectors | 4,976 | 502 | |
| [#128](https://github.com/dayfine/Arduino-Source/pull/128) | AutoStory v4: new game → Floaroma as one story table | 4,301 | 742 | |
| **Total** | | **14,853** | **2,889** | **1,243** |

### Upstream split: about 38 PRs, 14,853 code, 2,889 tests

Not cut yet; counts are code / 400.

| Upstream | From | PRs | Needs |
|---|---|---|---|
| B1 | [#152](https://github.com/dayfine/Arduino-Source/pull/152) | 1 | main: it changes existing detectors only |
| B2–B3 | [#134](https://github.com/dayfine/Arduino-Source/pull/134) | 2 | C6 |
| B4–B8 | [#127](https://github.com/dayfine/Arduino-Source/pull/127) | 5 | C7 |
| B9–B15 | [#129](https://github.com/dayfine/Arduino-Source/pull/129) | 7 | B8 |
| B16–B27 | [#148](https://github.com/dayfine/Arduino-Source/pull/148) | 12 | B15, C5, C10; its detectors may go from main, like F1 |
| B28–B38 | [#128](https://github.com/dayfine/Arduino-Source/pull/128) | 11 | B27, C11 |

## 4. Game simulator (M5, [#139](https://github.com/dayfine/Arduino-Source/issues/139))

A seeded fake game with odds from the decomp. Not written yet. Doesn't block M2 or M3.

### Upstream split: 2 PRs, ~1,200 code, ~600 tests, then ~500 per game

| Upstream | What | Code | Tests | Needs |
|---|---|---|---|---|
| G1 | Encounters, Repel, believed vs real position | ~600 | ~300 | C4, C6, C7 |
| G2 | HP, status, EXP, moves, whiteouts, run-mode statistics | ~600 | ~300 | G1 |
| G3+ | Per game: odds and data (FRLG first) | ~500 each | | G2 and that game's maps (F4, B3) |

## 5. RNG and names (M7 [#141](https://github.com/dayfine/Arduino-Source/issues/141), M12 [#147](https://github.com/dayfine/Arduino-Source/issues/147))

| PR | What | Code | Tests |
|---|---|---|---|
| [#61](https://github.com/dayfine/Arduino-Source/pull/61) | FRLG RNG: check the party before the program starts | 240 | 263 |
| [#62](https://github.com/dayfine/Arduino-Source/pull/62) | Switch on-screen keyboard (BDSP names) | 363 | 2 |
| [#81](https://github.com/dayfine/Arduino-Source/pull/81) | FRLG naming-screen typer | 722 | 345 |
| **Total** | | **1,325** | **610** |

### Upstream split: 5 PRs, 1,325 code (+~300), 610 tests; then 5–8 for M12

| Upstream | What | From | Code | Tests | Needs |
|---|---|---|---|---|---|
| R1 | FRLG RNG party check | [#61](https://github.com/dayfine/Arduino-Source/pull/61) | 240 | 263 | main |
| R2 | Switch keyboard | [#62](https://github.com/dayfine/Arduino-Source/pull/62) | 363 | 2 | main |
| R3 | FRLG naming-screen keyboard layout | [#81](https://github.com/dayfine/Arduino-Source/pull/81) | 475 | 345 | main |
| R4 | FRLG naming flow (and its detector, if F1 isn't in) | [#81](https://github.com/dayfine/Arduino-Source/pull/81) | 247 | 0 | R3 |
| R5 | Starter-RNG entry point for GiftRng | new | ~300 | | main; testable from a manual save |
| M12 | RNG routines made callable, then AutoStory hand-offs | new | not sized | | refactors: main; hand-offs: F15 |

AutoStory joins this track only at the hand-off: names at 00.01 and the starter after 00.03/00.04, both after F15.

## 6. Lab tooling and run records (M4, [#138](https://github.com/dayfine/Arduino-Source/issues/138))

| PR | What | Code | Tests |
|---|---|---|---|
| [#132](https://github.com/dayfine/Arduino-Source/pull/132) | Run records: one typed record per program run (new) | 675 | 657 |
| [#107](https://github.com/dayfine/Arduino-Source/pull/107) | Lab channel, Command File Runner, scripts | 2,416 | 483 |
| [#116](https://github.com/dayfine/Arduino-Source/pull/116) | Lab half of run records: `Lab/runs/<run_id>/` (now on [#132](https://github.com/dayfine/Arduino-Source/pull/132), via the lab-only [#149](https://github.com/dayfine/Arduino-Source/pull/149)) | 619 | 345 |
| [#125](https://github.com/dayfine/Arduino-Source/pull/125) | Route pictures (devtools/maps, Python) | 1,469 | 413 |
| [#131](https://github.com/dayfine/Arduino-Source/pull/131) | Agent integration design (docs only so far) | 0 | 0 |
| **Total** | | **5,179** | **1,898** |

[#149](https://github.com/dayfine/Arduino-Source/pull/149) and [#150](https://github.com/dayfine/Arduino-Source/pull/150)
are lab-only merge branches and never go upstream. [#151](https://github.com/dayfine/Arduino-Source/pull/151)
(agent-harness rules) merged into the notes branch; it has no code.

### Upstream split: 11 PRs, 5,179 code (+~150), 1,898 tests (+~100)

| Upstream | What | From | Code | Tests | Needs |
|---|---|---|---|---|---|
| L1 | RunRecord, RunEvent, RunRecordStore | [#132](https://github.com/dayfine/Arduino-Source/pull/132) | 448 | 657 | main |
| L2 | Session recorder wired into the program sessions | [#132](https://github.com/dayfine/Arduino-Source/pull/132) | 227 | 0 | L1 |
| L3–L6 | Lab channel and requests; Command File Runner (2); scripts | [#107](https://github.com/dayfine/Arduino-Source/pull/107) | 2,416 | 483 | main |
| L7, L8 | Lab run folders; build commit and labctl wait | [#116](https://github.com/dayfine/Arduino-Source/pull/116) | 619 | 345 | L2, L3 |
| L9, L10 | Route pictures | [#125](https://github.com/dayfine/Arduino-Source/pull/125) | 1,469 | 413 | F3 (the JSON maps) |
| L11 | Run-event hooks in story events and FRLG | new | ~150 | ~100 | L1, C8, F15 |

Lab builds use the fork branches, so the upstream order here doesn't slow lab work.

## 7. Later features (rough estimates)

| Feature | PRs | Code + tests | Notes |
|---|---|---|---|
| Run modes SPEEDRUN / DEFAULT / SAFE (M6, [#140](https://github.com/dayfine/Arduino-Source/issues/140)) | 3 | ~1.0k + 0.6k | (1) the mode option and strategy in the story run options; (2) FRLG level and party reading; (3) the FRLG speedrun route (no Viridian shopping, flee wild battles, Sammy only, Brock at level 11; pokeguy's 1:59:34 notes on the agents branch) |
| Trainer sight and level targets (M6) | 2 | ~0.6k + 0.4k | Trainer type, sight and facing in the map schema and generator; the planner scores sight lines and grass |
| Full RNG integration (M12, [#147](https://github.com/dayfine/Arduino-Source/issues/147)) | 5–8 | ~0.8k + 0.4k for the first hand-offs | AutoStory stops at a checkpoint and hands off to the RNG programs, then resumes the story |
| Mail glitch / ACE and detours | 3–4 | ~1.5k + 0.6k | Builds on R3/R4's naming-screen typer: mail-screen typing, the payload with verification, story detours to get the mail |
| Story after Brock ([#130](https://github.com/dayfine/Arduino-Source/pull/130)'s outline, 92 rows) | ~10–15 | ~8–12k + 3–4k | About one PR per segment, 500–1,000 lines each, plus new event types (HMs, puzzles, the Silph Scope) |

## 8. Upstream PR trees and queue

Six trees. Only two of them run through the shared core; the rest start from upstream main.
Sizes are code lines, with tests in brackets.

### Tree 1: shared core → FRLG MVP (C, F)

```
upstream main
├ C1  shared select database                  [#43]   56 (+40)
│ └ C2  shared story index                    [#44]   33 (+59)
│   └ C3  AutoStoryStats move                 [#45]   38
├ C4  grid types + leg planner                [#114] 488 (+406)
│ └ C6  MapData: schema, loader, embedding    [#114] 764 (+435)
│   ├ C7  LegWalker, route log, tile poses    [#114] 610 (+373)
│   │ └ C8  story events, overlay status      [#114] 781 (+677)
│   │   └ C9  story segments                  [#114] 297 (+374)
│   │     └ C10 story steps + retry (+C3)     [#122] 628 (+646)
│   │       └ C11 story run options           [#122] 214
│   └ F3  FRLG map generator + 25 maps        [#117] 318 (+3.2k data)
│     └ F4  FRLG maps on MapData              [#117] 540 (+189)
│       ├ F5, F6  FRLG route planning         [#117] 1,014
│       │ └ F8  FRLG walker (+C7, F2)         [#118] 481
│       │   └ F9  re-anchoring                [#119] 203 (+155)
│       │     └ F10 recovery                  [#119] 624 (+321)
│       │       └ F11 walker fixes            [new]  ~150 (+200)
│       │         └ F12 story actions (+C10, F1)   [#120] 661
│       │           └ F13, F14 story battles  [#120] 925
│       │             └ F15 program, table, simulation (+C11) [#120] 1,122 (+713)
│       └ F7  player facing reader            [#117] 310
├ F1  FRLG readers: item list, save card, naming screen  [#120] 427 (+125)
└ F2  FRLG battle flash + battle screen       [#118] 212
```

### Tree 2: BDSP MVP (B), on the shared core

```
upstream main
├ B1  ShortDialogDetector fix                 [#152]  98 (+4)     independent
└ (C6, C7 from tree 1)
  └ B2–B3   story maps                        [#134]  733 (+420)
    └ B4–B8   navigation                      [#127] 1,920 (+967)
      └ B9–B15  walker                        [#129] 2,825 (+254)
        └ B16–B27 actions, battles, detectors (+C5, C10) [#148] 4,976 (+502)
          └ B28–B38 program and story table (+C11)    [#128] 4,301 (+742)
```

### Tree 3: game simulator (G, M5), on the navigation half of the core

```
(C7 from tree 1)
└ G1  encounters, Repel, position             [new] ~600 (+300)
  └ G2  battle model, run-mode statistics     [new] ~600 (+300)
    └ G3+ per-game odds (FRLG after F4)       [new] ~500 each
```

### Tree 4: RNG and names (R, M7 and M12), independent

```
upstream main
├ R1  FRLG RNG party check                    [#61]  240 (+263)
├ R2  Switch keyboard                         [#62]  363 (+2)
├ R3  FRLG naming keyboard layout             [#81]  475 (+345)
│ └ R4  FRLG naming flow                      [#81]  247
├ R5  starter-RNG entry point                 [new]  ~300
└ M12 RNG routines made callable              [new]  5–8 PRs, not sized
      └ AutoStory hand-offs                   join tree 1 after F15
```

### Tree 5: lab tooling and run records (L, M4), independent

```
upstream main
├ L1  RunRecord, RunEvent, store              [#132] 448 (+657)
│ └ L2  session recorder                      [#132] 227
│   ├ L7, L8  lab run folders (+L3)           [#116] 619 (+345)
│   └ L11 run-event hooks (+C8, F15)          [new]  ~150 (+100)
└ L3–L6  lab channel, Command File Runner, scripts   [#107] 2,416 (+483)
(F3 from tree 1)
└ L9, L10  route pictures                     [#125] 1,469 (+413)
```

### Tree 6: ScreenMachine (C5), independent

```
upstream main
└ C5  ScreenMachine + StepTrace               [#114] 329          needed by B16–B38 only
```

### Queue (two open at a time; the next opens when one merges)

Slot 1 carries the longest chain. Slot 2 takes the independent trees, so neither slot waits on the other.

| Phase | Slot 1 | Slot 2 |
|---|---|---|
| 1. Core | C4 → C6 → C7 → C8 → C9 → C10 → C11 | C1 → C2 → C3, then F1 → F2 → R1 → B1 |
| 2. FRLG maps | F8 → F9 → F10 → F11 (once F6 is in) | F3 → F4 → F5 → F6 → F7 (from when C6 merges) |
| 3. FRLG program | F12 → F13 → F14 → F15 | L1 → L2 → C5 → G1 → G2 → R2 → R3 → R4 → R5 |
| 4. BDSP and the rest | B2 → … → B38 | L3–L6 → L7, L8 → L9, L10 → L11 → G3 → M12 |

Phase 2 overlaps phase 1: F3 can open as soon as C6 merges. The FRLG path (C + F) is 26 PRs, and its
longest chain (C4 → C6 → F3 → … → F15) is 14 merges deep, so two slots cut its wait roughly in
half, not more. BDSP is the longest track by far (~38 PRs) and goes after the FRLG MVP.
