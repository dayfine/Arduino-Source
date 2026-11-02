# AutoStory: code size by PR, the upstream split, and the queue

Re-measured on 2026-11-01 at about 17:30 (the first version was from 13:10 the same day). Each row is
`git diff --numstat parent...child` on the fork branches, counting added lines. "Code" excludes
tests, map data (JSON), docs and cmake. The plan and PR tree are in
[AutoStory-Demo-Plan.md](AutoStory-Demo-Plan.md); the milestones are issues
[#135](https://github.com/dayfine/Arduino-Source/issues/135)–[#147](https://github.com/dayfine/Arduino-Source/issues/147).

Each fork PR is a collector. Upstream gets smaller PRs of about 400 code lines or less, one concern
each. Each section below lists its fork PRs, then the upstream PRs they become. Tracks are lettered in the summary's order: A (shared core), B (FRLG MVP), C (FRLG maps after Brock),
D (game simulator), E (RNG and names), F (lab and run records). BDSP (M3, [#137](https://github.com/dayfine/Arduino-Source/issues/137)) is tracked separately and not counted here.

## Summary

| Track | Upstream PRs | Code | Tests | Data | Needs the shared core? |
|---|---|---|---|---|---|
| A. Shared core (M1) | 10 | 3,909 | 3,010 | 151 (test maps) | is the core |
| B. FRLG MVP (M2) | 15 | 6,837 + ~150 new | 1,503 + ~200 new | 3,202 | yes, except B1 and B2 |
| C. FRLG maps after Brock | 4 code PRs, + map drops in Packages | ~1,050 new | ~600 new | ~15k lines (~0.4 MB) new, in Packages | yes (A5, B4) |
| D. Game simulator (M5) | 2, +1 per game | ~1,200 new | ~600 new | odds tables | partly: grid, MapData and LegWalker (A4, A5, A6) |
| E. RNG and names (M7, M12) | 5, then 5–8 for M12 | 1,325 + ~300 new | 610 | | no, until the AutoStory hand-off |
| F. Lab tooling and run records (M4) | 11 | 5,179 + ~150 new | 1,898 + ~100 new | | no, except F11 |
| **All tracks** | **47**, plus M12 | **~20.1k** (17.3k written) | **~8.5k** (7.0k written) | 3.4k here, ~15k in Packages | |
| **FRLG MVP path (A + B)** | **25** | **~10.9k** | **~4.7k** | 3.4k | |

"Written" means the code is on a fork branch now; "new" is an estimate for PRs not yet written.
[#131](https://github.com/dayfine/Arduino-Source/pull/131) (agent integration, about 7 PRs, ~2k) and the
story code after Brock (section 6) are not counted; the maps it needs are (track C).

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
| | MapData: schema, loader, CMake embedding | 764 | 435 (+151 test maps, 85 cmake) |
| | LegWalker, route log, tile poses | 610 | 373 |
| | Story events (with the overlay status) | 781 | 677 |
| | Story segments | 297 | 374 |
| | ScreenMachine / StepTrace: used only by BDSP's actions, so it goes upstream with BDSP | (329) | 0 |
| | **[#114](https://github.com/dayfine/Arduino-Source/pull/114) total** (without ScreenMachine) | **3,269** (2,940) | **2,265** |
| [#122](https://github.com/dayfine/Arduino-Source/pull/122) | Story steps and one retry policy | 628 | 646 |
| | Story run options (dropdowns, stats, Checks, language) | 214 | 0 |
| | **[#122](https://github.com/dayfine/Arduino-Source/pull/122) total** | **842** | **646** |

Logging is split across the two halves:
- **Route log** (`Pokemon_RouteLog` in [#114](https://github.com/dayfine/Arduino-Source/pull/114)): one line format for a route walk in both games. FRLG calls it from [#120](https://github.com/dayfine/Arduino-Source/pull/120)'s branch, BDSP from its walker in [#129](https://github.com/dayfine/Arduino-Source/pull/129).
- **Checkpoint and event log** (`[Segment]`, `[Checkpoint]`, `[Event]` in [#114](https://github.com/dayfine/Arduino-Source/pull/114)'s StoryEvents): shared by both games already.
- **Gap:** the route log is grid-bound: tiles, directions, tile counts. 3D games (SV, LA) have route legs too, but no tiles. Planned fix: a grid-free core (leg label, target, how it ended, how the end is known, and an optional position the game formats) with the grid lines as one form of it. The same records become the run events of [#132](https://github.com/dayfine/Arduino-Source/pull/132), so the log and the run record carry one format.

### Upstream split: 10 PRs, 3,909 code, 3,010 tests

| Upstream | What | From | Code | Tests | Needs |
|---|---|---|---|---|---|
| A1 | Shared start/end select database | [#43](https://github.com/dayfine/Arduino-Source/pull/43) | 56 | 40 | main |
| A2 | Shared story index | [#44](https://github.com/dayfine/Arduino-Source/pull/44) | 33 | 59 | A1 |
| A3 | AutoStoryStats to Pokemon/AutoStory (move) | [#45](https://github.com/dayfine/Arduino-Source/pull/45) | 38 | 0 | A2 |
| A4 | Grid types and leg planner | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 488 | 406 | main |
| A5 | MapData: schema, loader, CMake embedding | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 764 | 435 | A4; split 2 if asked |
| A6 | LegWalker, route log, tile poses | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 610 | 373 | A5 |
| A7 | Story events, overlay status | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 781 | 677 | A6 |
| A8 | Story segments | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 297 | 374 | A7 |
| A9 | Story steps and retry policy | [#122](https://github.com/dayfine/Arduino-Source/pull/122) | 628 | 646 | A8, A3 |
| A10 | Story run options | [#122](https://github.com/dayfine/Arduino-Source/pull/122) | 214 | 0 | A9 |

The SV refactors (A1–A3) and the navigation chain (A4–A8) don't depend on each other; they meet at A9.

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
| Run-event hooks (shared code and FRLG) | track F, F11 | ~150 + 100 |

Then the console check: FRLG 3/3, new game → Brock, in one run each. The game simulator moved to its own track (D).

### Upstream split: 15 PRs, 6,837 code (+~150), 1,503 tests (+~200)

| Upstream | What | From | Code | Tests | Needs |
|---|---|---|---|---|---|
| B1 | FRLG readers: item list, save card, naming-screen detector | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 427 | 125 | main |
| B2 | FRLG battle flash watcher and battle-screen detector | [#118](https://github.com/dayfine/Arduino-Source/pull/118) | 212 | 0 | main |
| B3 | FRLG map generator and 25 JSON maps | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 318 | 0 (+3,202 data) | A5 |
| B4 | FRLG maps on MapData | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 540 | 189 | B3 |
| B5, B6 | FRLG route planning, in two halves | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 1,014 | 0 | B4, A4 |
| B7 | Player facing reader | [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 310 | 0 | B4 |
| B8 | FRLG walker | [#118](https://github.com/dayfine/Arduino-Source/pull/118) | 481 | 0 | B2, B6, A6 |
| B9 | Re-anchoring on a known tile | [#119](https://github.com/dayfine/Arduino-Source/pull/119) | 203 | 155 | B8 |
| B10 | Recovery after an interrupted step | [#119](https://github.com/dayfine/Arduino-Source/pull/119) | 624 | 321 | B9 |
| B11 | Walker fixes: drift, short wall steps | new | ~150 | ~200 | B10 |
| B12 | Story actions | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 661 | 0 | A9, B1 |
| B13, B14 | Story battles, in two halves | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 925 | 0 | B12, B11 |
| B15 | Program, story table, events, simulation | [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 1,122 | 713 | B14, A10; split 2 if asked |

B1 and B2 read the screen only and can go upstream now. B1's naming-screen detector is also in
[#81](https://github.com/dayfine/Arduino-Source/pull/81) (122 lines); whichever lands first carries it.

## 3. Game simulator (M5, [#139](https://github.com/dayfine/Arduino-Source/issues/139))

A seeded fake game with odds from the decomp. Not written yet. Doesn't block M2 or M3.

### Upstream split: 2 PRs, ~1,200 code, ~600 tests, then ~500 per game

| Upstream | What | Code | Tests | Needs |
|---|---|---|---|---|
| D1 | Encounters, Repel, believed vs real position | ~600 | ~300 | A4, A5, A6 |
| D2 | HP, status, EXP, moves, whiteouts, run-mode statistics | ~600 | ~300 | D1 |
| D3+ | Per game: odds and data (FRLG first) | ~500 each | | D2 and that game's maps (B4) |

## 4. RNG and names (M7 [#141](https://github.com/dayfine/Arduino-Source/issues/141), M12 [#147](https://github.com/dayfine/Arduino-Source/issues/147))

| PR | What | Code | Tests |
|---|---|---|---|
| [#61](https://github.com/dayfine/Arduino-Source/pull/61) | FRLG RNG: check the party before the program starts | 240 | 263 |
| [#62](https://github.com/dayfine/Arduino-Source/pull/62) | Switch on-screen keyboard (BDSP names) | 363 | 2 |
| [#81](https://github.com/dayfine/Arduino-Source/pull/81) | FRLG naming-screen typer | 722 | 345 |
| **Total** | | **1,325** | **610** |

### Upstream split: 5 PRs, 1,325 code (+~300), 610 tests; then 5–8 for M12

| Upstream | What | From | Code | Tests | Needs |
|---|---|---|---|---|---|
| E1 | FRLG RNG party check | [#61](https://github.com/dayfine/Arduino-Source/pull/61) | 240 | 263 | main |
| E2 | Switch keyboard | [#62](https://github.com/dayfine/Arduino-Source/pull/62) | 363 | 2 | main |
| E3 | FRLG naming-screen keyboard layout | [#81](https://github.com/dayfine/Arduino-Source/pull/81) | 475 | 345 | main |
| E4 | FRLG naming flow (and its detector, if B1 isn't in) | [#81](https://github.com/dayfine/Arduino-Source/pull/81) | 247 | 0 | E3 |
| E5 | Starter-RNG entry point for GiftRng | new | ~300 | | main; testable from a manual save |
| M12 | RNG routines made callable, then AutoStory hand-offs | new | not sized | | refactors: main; hand-offs: B15 |

AutoStory joins this track only at the hand-off: names at 00.01 and the starter after 00.03/00.04, both after B15.

## 5. Lab tooling and run records (M4, [#138](https://github.com/dayfine/Arduino-Source/issues/138))

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
| F1 | RunRecord, RunEvent, RunRecordStore | [#132](https://github.com/dayfine/Arduino-Source/pull/132) | 448 | 657 | main |
| F2 | Session recorder wired into the program sessions | [#132](https://github.com/dayfine/Arduino-Source/pull/132) | 227 | 0 | F1 |
| F3–F6 | Lab channel and requests; Command File Runner (2); scripts | [#107](https://github.com/dayfine/Arduino-Source/pull/107) | 2,416 | 483 | main |
| F7, F8 | Lab run folders; build commit and labctl wait | [#116](https://github.com/dayfine/Arduino-Source/pull/116) | 619 | 345 | F2, F3 |
| F9, F10 | Route pictures | [#125](https://github.com/dayfine/Arduino-Source/pull/125) | 1,469 | 413 | B3 (the JSON maps) |
| F11 | Run-event hooks in story events and FRLG | new | ~150 | ~100 | F1, A7, B15 |

Lab builds use the fork branches, so the upstream order here doesn't slow lab work.

## 6. Later features (rough estimates)

| Feature | PRs | Code + tests | Notes |
|---|---|---|---|
| Run modes SPEEDRUN / DEFAULT / SAFE (M6, [#140](https://github.com/dayfine/Arduino-Source/issues/140)) | 3 | ~1.0k + 0.6k | (1) the mode option and strategy in the story run options; (2) FRLG level and party reading; (3) the FRLG speedrun route (no Viridian shopping, flee wild battles, Sammy only, Brock at level 11; pokeguy's 1:59:34 notes on the agents branch) |
| Trainer sight and level targets (M6) | 2 | ~0.6k + 0.4k | Trainer type, sight and facing in the map schema and generator; the planner scores sight lines and grass |
| Full RNG integration (M12, [#147](https://github.com/dayfine/Arduino-Source/issues/147)) | 5–8 | ~0.8k + 0.4k for the first hand-offs | AutoStory stops at a checkpoint and hands off to the RNG programs, then resumes the story |
| Mail glitch / ACE and detours | 3–4 | ~1.5k + 0.6k | Builds on E3/E4's naming-screen typer: mail-screen typing, the payload with verification, story detours to get the mail |
| Maps after Brock (track C, below) | 4 code + ~10 data drops | ~1.05k + 0.6k, plus ~0.4 MB of JSON | Maps move to Resources (Packages); generator learns water, Cut trees, boulders and switches, spinners and teleport tiles |
| Story after Brock ([#130](https://github.com/dayfine/Arduino-Source/pull/130)'s outline, 92 rows) | ~10–15 | ~8–12k + 3–4k | About one PR per segment, 500–1,000 lines each, plus new event types (HMs, puzzles, the Silph Scope); each needs its segment's maps from track C |

### Maps for the story after Brock

Today's FRLG MVP uses 25 generated maps, 90,924 bytes of JSON (~89 KB, 3,202 lines). The main story
from Pewter to the Hall of Fame needs about 120 more. Counted from the 68 main-story rows of
[#130](https://github.com/dayfine/Arduino-Source/pull/130)'s outline (01.01–10.07, DEFAULT run mode,
east road via Routes 12–15) against
[pokefirered's data/maps](https://github.com/pret/pokefirered/tree/master/data/maps) (427 maps):

| Segment | New maps | Which |
|---|---|---|
| 01 Pewter → Cerulean | 6 | Routes 3, 4, Route 4 Center, Mt. Moon 1F/B1F/B2F |
| 02 Cerulean, Bill, Misty | 10 | Route 25, Sea Cottage, gym, Mart, burgled house, Route 5, Underground Path (2 entrances, tunnel), Route 6 |
| 03 Vermilion, S.S. Anne, Surge | 9 | city, Center, Mart, gym; S.S. Anne exterior, three corridors, captain's office |
| 04 Rock Tunnel → Celadon | 14 | Routes 9, 10, Route 10 Center, Rock Tunnel 1F/B1F, Lavender and its Center, Route 8, Underground Path (2 entrances, tunnel), Route 7, Celadon and its Center |
| 05 Celadon → Poke Flute | 21 | condominiums 1F (Tea), department store 1F/2F, gym, Game Corner, Rocket Hideout B1F–B4F and elevator, Route 7 gate, Saffron, Route 8 gate, Pokemon Tower 1F–7F, Fuji's house |
| 06 Fuchsia, Safari, Koga | 17 | Routes 12–15 and two gatehouses, Fuchsia, Center, Mart, gym, Warden's house, Safari entrance, four Safari areas, Secret House |
| 07 Silph Co., Sabrina | 14 | Saffron Center, Silph 1F–11F and elevator, gym |
| 08 Cinnabar, Blaine | 14 | Route 6 gate, Diglett's Cave (3) for the walk back to Pallet, Routes 21 north/south, Cinnabar, Center, Mart, Mansion 1F/2F/3F/B1F, gym |
| 09 Giovanni, Victory Road | 8 | Viridian gym, Route 22 gate, Route 23, Victory Road 1F–3F, Indigo Plateau and its Center |
| 10 League | 6 | Lorelei, Bruno, Agatha, Lance, Champion, Hall of Fame |
| **Total** | **~120** | about 5x today's 25; ~10–15 more for optional branches (Flash, bike, Fly, extra shops) |

Size: at today's average of 3.6 KB a map, 120 maps are about 0.43 MB, or ~15k JSON lines. The story
set grows from 89 KB to about 0.5 MB, a third of FireRed's full ~1.5 MB. That is why the maps move out
of the code: per [AutoStory-Architecture.md](AutoStory-Architecture.md) §4 they become game data in
`Resources/PokemonFRLG/Maps/` in the Packages repo, loaded with `RESOURCE_PATH()`, and stay out of
code review. Only the shared test maps stay compiled in.

The later maps also need map features today's 25 don't have: Surf water, Cut trees, Strength
boulders and floor switches (Victory Road), spinner tiles (Rocket Hideout, Viridian gym), teleport
pads (Silph Co., Saffron gym), multi-floor warps and elevators.

#### Upstream split: 4 code PRs, ~1,050 code, ~600 tests; map data in Packages

| Upstream | What | Code | Tests | Needs |
|---|---|---|---|---|
| C1 | FRLG maps load from Resources (today's 25 move to Packages) | ~150 | ~100 | B4 |
| C2 | Generator and MapData: Surf water and Cut trees | ~300 | ~150 | C1, A5 |
| C3 | Generator and MapData: Strength boulders and floor switches | ~300 | ~200 | C2 |
| C4 | Generator and MapData: spinners, teleport pads, elevators | ~300 | ~150 | C2 |
| Data | Map drops to Packages, one per story segment (01–10) | 0 | | C1; the drop for a segment goes with that segment's story PR |

## 7. Upstream PR trees and queue

Five trees. Trees 1 and 2 run through the shared core, tree 3 needs only its navigation half, and
trees 4 and 5 start from upstream main. BDSP's tree (with ScreenMachine) is tracked separately.
Sizes are code lines, with tests in brackets.

### Tree 1: shared core → FRLG MVP (A, B)

```
upstream main
├ A1  shared select database                  [#43]   56 (+40)
│ └ A2  shared story index                    [#44]   33 (+59)
│   └ A3  AutoStoryStats move                 [#45]   38
├ A4  grid types + leg planner                [#114] 488 (+406)
│ └ A5  MapData: schema, loader, embedding    [#114] 764 (+435)
│   ├ A6  LegWalker, route log, tile poses    [#114] 610 (+373)
│   │ └ A7  story events, overlay status      [#114] 781 (+677)
│   │   └ A8  story segments                  [#114] 297 (+374)
│   │     └ A9  story steps + retry (+A3)    [#122] 628 (+646)
│   │       └ A10 story run options           [#122] 214
│   └ B3  FRLG map generator + 25 maps        [#117] 318 (+3.2k data)
│     └ B4  FRLG maps on MapData              [#117] 540 (+189)
│       ├ B5, B6  FRLG route planning         [#117] 1,014
│       │ └ B8  FRLG walker (+A6, B2)         [#118] 481
│       │   └ B9  re-anchoring                [#119] 203 (+155)
│       │     └ B10 recovery                  [#119] 624 (+321)
│       │       └ B11 walker fixes            [new]  ~150 (+200)
│       │         └ B12 story actions (+A9, B1)   [#120] 661
│       │           └ B13, B14 story battles  [#120] 925
│       │             └ B15 program, table, simulation (+A10) [#120] 1,122 (+713)
│       └ B7  player facing reader            [#117] 310
├ B1  FRLG readers: item list, save card, naming screen  [#120] 427 (+125)
└ B2  FRLG battle flash + battle screen       [#118] 212
```

### Tree 2: FRLG maps after Brock (C), on tree 1

```
(B4 from tree 1)
└ C1  FRLG maps load from Resources/Packages  [new]  ~150 (+100)
  └ C2  water and Cut trees (+A5)             [new]  ~300 (+150)
    ├ C3  boulders and floor switches         [new]  ~300 (+200)
    ├ C4  spinners, teleport pads, elevators  [new]  ~300 (+150)
    └ map drops 01–10 in Packages             [new]  ~120 maps, ~0.4 MB
      └ story after Brock, one PR per segment (+B15)  ~10–15 PRs
```

### Tree 3: game simulator (D, M5), on the navigation half of the core

```
(A6 from tree 1)
└ D1  encounters, Repel, position             [new] ~600 (+300)
  └ D2  battle model, run-mode statistics     [new] ~600 (+300)
    └ D3+ per-game odds (FRLG after B4)       [new] ~500 each
```

### Tree 4: RNG and names (E, M7 and M12), independent

```
upstream main
├ E1  FRLG RNG party check                    [#61]  240 (+263)
├ E2  Switch keyboard                         [#62]  363 (+2)
├ E3  FRLG naming keyboard layout             [#81]  475 (+345)
│ └ E4  FRLG naming flow                      [#81]  247
├ E5  starter-RNG entry point                 [new]  ~300
└ M12 RNG routines made callable              [new]  5–8 PRs, not sized
      └ AutoStory hand-offs                   join tree 1 after B15
```

### Tree 5: lab tooling and run records (F, M4), independent

```
upstream main
├ F1  RunRecord, RunEvent, store              [#132] 448 (+657)
│ └ F2  session recorder                      [#132] 227
│   ├ F7, F8  lab run folders (+F3)           [#116] 619 (+345)
│   └ F11 run-event hooks (+A7, B15)          [new]  ~150 (+100)
└ F3–F6  lab channel, Command File Runner, scripts   [#107] 2,416 (+483)
(B3 from tree 1)
└ F9, F10  route pictures                     [#125] 1,469 (+413)
```

### Queue (two open at a time; the next opens when one merges)

Slot 1 carries the longest chain. Slot 2 takes the independent trees, so neither slot waits on the other.

| Phase | Slot 1 | Slot 2 |
|---|---|---|
| 1. Core | A4 → A5 → A6 → A7 → A8 → A9 → A10 | A1 → A2 → A3, then B1 → B2 → E1 |
| 2. FRLG maps | B8 → B9 → B10 → B11 (once B6 is in) | B3 → B4 → B5 → B6 → B7 (from when A5 merges) |
| 3. FRLG program | B12 → B13 → B14 → B15 | F1 → F2 → C1 → D1 → D2 → E2 → E3 → E4 → E5 |
| 4. After the MVP | C2 → C3 → C4, then the story after Brock, segment by segment | F3–F6 → F7, F8 → F9, F10 → F11 → D3 → M12 |

Phase 2 overlaps phase 1: B3 can open as soon as A5 merges. The FRLG path (A + B) is 25 PRs, and its
longest chain (A4 → A5 → B3 → … → B15) is 14 merges deep, so two slots cut its wait roughly in
half, not more. Map drops go to the Packages repo, not Arduino-Source, so they don't take a slot.
BDSP's queue is planned separately; when it starts, it shares these two slots.
