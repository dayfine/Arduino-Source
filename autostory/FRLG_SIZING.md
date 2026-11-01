# FRLG AutoStory: code size by PR, and what remains

Measured on 2026-11-01 at about 13:10, as `git diff --numstat parent...child` on the fork branches.
"Code" excludes tests, map data (JSON), docs and cmake. The FRLG chain is being restacked, so these
shapes are close to final, not final. The plan and PR tree are in [DEMO_PLAN.md](DEMO_PLAN.md).

## 1. Prep work (small refactors before the shared core)

| PR | What | Code | Tests |
|---|---|---|---|
| [#43](https://github.com/dayfine/Arduino-Source/pull/43) | Shared start/end checkpoint dropdowns (SV refactor) | 56 | 0 |
| [#44](https://github.com/dayfine/Arduino-Source/pull/44) | Shared story index | 33 | 99 |
| [#45](https://github.com/dayfine/Arduino-Source/pull/45) | AutoStoryStats moved to Pokemon/AutoStory | 38 | 0 |
| **Total** | Ready to send upstream first | **127** | **99** |

Outside the chain but related: [#86](https://github.com/dayfine/Arduino-Source/pull/86) FRLG bag detector tests, and [#61](https://github.com/dayfine/Arduino-Source/pull/61) FRLG party check (240 code,
263 tests), which belongs with the RNG work.

## 2. Shared navigation and map infrastructure ([#114](https://github.com/dayfine/Arduino-Source/pull/114))

| Part | Code | Tests |
|---|---|---|
| Grid, route planner, LegWalker, MapData/JSON maps, route log, tile poses, map embedding | 1,813 | 1,010 |
| ScreenMachine / StepTrace | 329 | 0 |
| Shared route log (`Pokemon_RouteLog`: plan, step results, stops, re-anchors, arrivals; BDSP's lines lifted as they were) | incl. above | incl. above |
| **[#114](https://github.com/dayfine/Arduino-Source/pull/114) navigation part** | **~2,140** | **~1,010** (+151 lines of test maps) |

Logging is split across the two halves:
- **Route log** (`Pokemon_RouteLog` in [#114](https://github.com/dayfine/Arduino-Source/pull/114)): one line format for a route walk in both games. FRLG calls it from 4 files on [#120](https://github.com/dayfine/Arduino-Source/pull/120)'s branch, BDSP from its walker in [#129](https://github.com/dayfine/Arduino-Source/pull/129).
- **Checkpoint and event log** (`[Segment]`, `[Checkpoint]`, `[Event]` in [#114](https://github.com/dayfine/Arduino-Source/pull/114)'s StoryEvents): shared by both games already.
- **Gap:** the route log is grid-bound: tiles, directions, tile counts. 3D games (SV, LA) have route legs too, but no tiles. Planned fix: a grid-free core (leg label, target, how it ended, how the end is known, and an optional position the game formats) with the grid lines as one form of it. The same records become [#116](https://github.com/dayfine/Arduino-Source/pull/116)'s run events, so the log and the run record carry one format.

## 3. Shared AutoStory infrastructure

| PR | What | Code | Tests |
|---|---|---|---|
| [#114](https://github.com/dayfine/Arduino-Source/pull/114) (story part) | Story events and segments as tables | 1,021 | 966 |
| [#122](https://github.com/dayfine/Arduino-Source/pull/122) | Shared story steps, retry policy, run options (Checks, language, dropdowns, stats) | 838 | 637 |
| **Total** | | **~1,860** | **~1,600** |

[#114](https://github.com/dayfine/Arduino-Source/pull/114) holds both the navigation and the story-event code (~3.2k code). Upstream it could be split
in two: navigation first, then story events.

## 4. FRLG game program (the MVP)

| PR | What | Code | Tests | Map data |
|---|---|---|---|---|
| [#117](https://github.com/dayfine/Arduino-Source/pull/117) | FRLG maps (generator + 25 JSON maps) and route planning | 3,249 | 821 | 3,194 |
| [#118](https://github.com/dayfine/Arduino-Source/pull/118) | FRLG walker: steps, warps, grass, signs, ledges | 690 | 0 | |
| [#119](https://github.com/dayfine/Arduino-Source/pull/119) | Finding the player again after an interrupted step | 739 | 436 | |
| [#120](https://github.com/dayfine/Arduino-Source/pull/120) | FRLG AutoStory, new game → Brock: table, Mart and bag checks, save check, simulation | 3,124 | 837 | |
| **Total** | | **~7,800** | **~2,100** | **3,194** |

[#123](https://github.com/dayfine/Arduino-Source/pull/123) is fully merged into [#120](https://github.com/dayfine/Arduino-Source/pull/120), shows no diff, and can be closed. [#117](https://github.com/dayfine/Arduino-Source/pull/117) is large; splitting the
generator and maps from the planner is an option.

## 5. Still needed for the MVP (estimates)

| # | PR | Where | Size (code + tests) |
|---|---|---|---|
| a | (moved to a parallel track: the game simulator) | [#114](https://github.com/dayfine/Arduino-Source/pull/114) | ~400 + 400 |
| b | Walker fixes: drift after battles; fail a wall step that stops far short | [#119](https://github.com/dayfine/Arduino-Source/pull/119) | ~150 + 200 |
| c | Run-record recorder (framework half of [#116](https://github.com/dayfine/Arduino-Source/pull/116)) | new PR on main | ~630 incl. tests |
| d | Run-event hooks: shared code and FRLG | [#114](https://github.com/dayfine/Arduino-Source/pull/114), [#120](https://github.com/dayfine/Arduino-Source/pull/120) | ~150 + 100 |
| e | Optionally split [#117](https://github.com/dayfine/Arduino-Source/pull/117): maps and generator vs planner | existing code | 0 new |
| | **Total** | 2–3 new PRs plus commits on existing ones | **~1.3k code + ~1k tests** |

Then the console check: FRLG 3/3, new game → Brock, in one run each.

## 6. Later features (rough estimates)

| Feature | PRs | Code + tests | Notes |
|---|---|---|---|
| Run modes SPEEDRUN / DEFAULT / SAFE | 3 | ~1.0k + 0.6k | (1) the mode option and strategy in [#122](https://github.com/dayfine/Arduino-Source/pull/122); (2) FRLG level and party reading; (3) the FRLG speedrun route (no Viridian shopping, flee wild battles, Sammy only, Brock at level 11; pokeguy's 1:59:34 notes on the agents branch) |
| Trainer sight and level targets | 2 | ~0.6k + 0.4k | Trainer type, sight and facing in the map schema and generator; the planner scores sight lines and grass |
| RNG program integration | 2–3 | ~0.8k + 0.4k | AutoStory stops at a checkpoint and hands off to the RNG programs ([#61](https://github.com/dayfine/Arduino-Source/pull/61), Ten Lines seeds), then resumes the story |
| Mail glitch / ACE and detours | 3–4 | ~1.5k + 0.6k | Builds on [#81](https://github.com/dayfine/Arduino-Source/pull/81)'s naming-screen typer (722 code, 345 tests): mail-screen typing, the payload with verification, story detours to get the mail |
| Story after Brock ([#130](https://github.com/dayfine/Arduino-Source/pull/130)'s outline, 92 rows) | ~10–15 | ~8–12k + 3–4k | About one PR per segment, 500–1,000 lines each, plus new event types (HMs, puzzles, the Silph Scope) |


## How the current PRs break up for upstream

Each fork PR above is a collector; upstream gets smaller PRs of about 400 changed lines of code or
less (tests and map data not counted), one concern each.

| Fork PR | Upstream PRs | Split |
|---|---|---|
| [#43](https://github.com/dayfine/Arduino-Source/pull/43)–[#45](https://github.com/dayfine/Arduino-Source/pull/45) | 2–3 | [#43](https://github.com/dayfine/Arduino-Source/pull/43) with its own test; [#44](https://github.com/dayfine/Arduino-Source/pull/44); [#45](https://github.com/dayfine/Arduino-Source/pull/45) (or [#43](https://github.com/dayfine/Arduino-Source/pull/43)+[#44](https://github.com/dayfine/Arduino-Source/pull/44) as one) |
| [#114](https://github.com/dayfine/Arduino-Source/pull/114) | 5 | grid and route planner; ScreenMachine/StepTrace; MapData, JSON maps and embedding; LegWalker and the route log; story events and segments |
| [#122](https://github.com/dayfine/Arduino-Source/pull/122) | 2 | story steps and their builder; run options (dropdowns, stats, Checks, language) |
| [#117](https://github.com/dayfine/Arduino-Source/pull/117) | 2 | FRLG map generator and JSON maps (data); FRLG route planning and the facing reader |
| [#118](https://github.com/dayfine/Arduino-Source/pull/118) | 1 | the FRLG walker |
| [#119](https://github.com/dayfine/Arduino-Source/pull/119) | 1 | finding the player again |
| [#120](https://github.com/dayfine/Arduino-Source/pull/120) | 3 | program and story table with the simulation; Mart and bag checks; save-card check and language-free reads |
| MVP remainder (section 5) | 3–4 | walker simulator; walker fixes; run-record recorder; run-event hooks |
| **FRLG path total** | **~19–21** | upstream allows two open at a time, so this is a long serial queue |

Tooling splits too: [#107](https://github.com/dayfine/Arduino-Source/pull/107) into about 3, [#116](https://github.com/dayfine/Arduino-Source/pull/116) into about 3, [#125](https://github.com/dayfine/Arduino-Source/pull/125) into 1–2, [#131](https://github.com/dayfine/Arduino-Source/pull/131) into about 7.


## Upstream PR tree and queue for the FRLG path

Each upstream PR is cut from the fork collector named in brackets. Upstream allows two open PRs
from us at a time, so the queue runs about two deep. Sizes are code lines, with tests in brackets.

```
upstream main
U1  AutoStory: shared start/end select database (+ its test)        [#43]  ~60 (+40)
└ U2  AutoStory: shared story index                                  [#44]  ~35 (+60)
  └ U3  AutoStory: AutoStoryStats to Pokemon/AutoStory (move)        [#45]  ~40
    └ U4  Pokemon navigation: grid types + leg planner               [#114] ~600 (+370)
      ├ U5  ScreenMachine + StepTrace                                [#114] ~330
      └ U6  Map data: JSON maps, schema, loader, CMake embedding     [#114] ~450 (+240, test maps)
        └ U7  LegWalker + shared route log                           [#114] ~430 (+370)
          └ U8  Story events and segments (checkpoint/event log)     [#114] ~1,000 (+970)  split 2 if asked
            └ U9  Story steps + builder + executor interface         [#122] ~500 (+400)
              └ U10 Story run options: dropdowns, stats, checks, language [#122] ~340 (+240)
                └ U11 FRLG: map generator + 25 JSON maps (data)      [#117] ~200 (+3.2k data)
                  └ U12 FRLG: route planning + facing reader         [#117] ~3,000 (+820)  split 2-3
                    └ U13 FRLG: walker (steps, warps, grass, signs, ledges) [#118] ~690
                      └ U14 FRLG: find the player again              [#119] ~740 (+440)
                        └ U15 Walker simulator (believed vs real tile) [new, [#114](https://github.com/dayfine/Arduino-Source/pull/114)] ~400 (+400)
                          └ U16 Walker fixes: drift, short wall steps  [new, [#119](https://github.com/dayfine/Arduino-Source/pull/119)] ~150 (+200)
                            └ U17 FRLG AutoStory: program, table, simulation  [#120] ~1,600 (+500)
                              ├ U18 FRLG: Mart and bag checks         [#120] ~800 (+200)
                              └ U19 FRLG: save-card check, language-free reads [#120] ~700 (+140)
side branch from main:
U20 Run records: recorder (framework half of [#116](https://github.com/dayfine/Arduino-Source/pull/116))                  [new] ~450 (+180)
└ U21 Run-event hooks in story events and FRLG                      [new] ~150 (+100)
```

U12 is the one PR far over the ~400-line target. It should split into the planner and the
facing reader, maybe three PRs, once its code is reviewed. U8 may need the same.

**Queue** (two open at a time; the next opens when one merges):

| Slot | Order | Notes |
|---|---|---|
| 1 | U1 → U2 → U3 | small, refactor-only, SV-only: the first handoff |
| 2 | U20 alongside U1–U3 | independent of the AutoStory chain |
| then | U4 → U5/U6 → U7 → U8 | the navigation core; U5 can go in parallel with U6 |
| then | U9 → U10 | the shared story steps |
| then | U11 → U12 → U13 → U14 | FRLG navigation |
| then | U16 | the walker check (fails a wall step that stopped short) and fixes |
| last | U17 → U18/U19, U21 | the FRLG MVP program; U18 and U19 can go in parallel |

That's 21 PRs, at roughly one merged per week if review keeps up: a few months of queue. The
shape lets each PR be reviewed alone, and the FRLG MVP only lands at the end.

### Parallel tracks (independent of the U1–U19 chain)

These are based on main, not on the AutoStory chain, so they can be worked on and sent upstream in parallel:

| Track | PRs | What | Joins the chain at |
|---|---|---|---|
| Naming / keyboards | [#81](https://github.com/dayfine/Arduino-Source/pull/81) FRLG naming-screen typer (722 code, 345 tests); [#62](https://github.com/dayfine/Arduino-Source/pull/62) Switch on-screen keyboard (BDSP names) | Type "Kosmos" and nicknames instead of picking a preset name | FRLG 00.01 (player and rival names) after U17; BDSP intro on [#128](https://github.com/dayfine/Arduino-Source/pull/128) |
| Starter RNG | [#61](https://github.com/dayfine/Arduino-Source/pull/61) party check (240 code, 263 tests) + a starter-RNG entry point (~300) | Use the existing FRLG GiftRng (starters are gifts) from AutoStory's save in front of Oak's table | a hand-off after 00.03/00.04 once U17 lands; the RNG program itself can be built and tested now from a manual save |
| Game simulator | 2 PRs: (1) encounters, Repel, position; (2) HP, status, EXP, moves, whiteout, run-mode statistics | Seeded fake game with odds from the decomp; catches drift, exercises rare paths, sets level targets (architecture doc section 6a) | not blocking; core ~1.2k + 0.6k tests, per game ~0.5k + data |
| Run records | U20 [#132](https://github.com/dayfine/Arduino-Source/pull/132) → U21 | Recorder, then hooks | U21 needs U8 and U17 |
| Tooling | [#107](https://github.com/dayfine/Arduino-Source/pull/107) → [#116](https://github.com/dayfine/Arduino-Source/pull/116), [#125](https://github.com/dayfine/Arduino-Source/pull/125), [#131](https://github.com/dayfine/Arduino-Source/pull/131) | Lab channel, run records, route pictures, lab CLI | none (dev only) |
| BDSP | maps-v4 → [#127](https://github.com/dayfine/Arduino-Source/pull/127) → [#129](https://github.com/dayfine/Arduino-Source/pull/129) → [#128](https://github.com/dayfine/Arduino-Source/pull/128) | BDSP AutoStory v4 | after U10 |

With 3–4 upstream slots, a sensible mix is one slot for the chain, one for naming/RNG, and one for tooling or run records.

## Tooling, used by both games

| PR | What | Size |
|---|---|---|
| [#107](https://github.com/dayfine/Arduino-Source/pull/107) | Lab tooling: lab channel, Command File Runner, scripts | 2.4k code, 0.5k tests |
| [#116](https://github.com/dayfine/Arduino-Source/pull/116) | Run records, about 3 PRs | ~1.5k |
| [#125](https://github.com/dayfine/Arduino-Source/pull/125) | Route pictures (devtools/maps, Python) | 1.5k code, 0.4k tests |
| [#131](https://github.com/dayfine/Arduino-Source/pull/131) | Agent integration: one lab CLI, MCP later; about 7 PRs | ~2k |

## Summary

The FRLG MVP is about 7.8k lines of game code on about 4.1k of shared code (navigation 2.1k plus
story engine 1.9k), with about 3.8k lines of tests. Finishing it needs roughly 1.3k more code plus
tests and one 3/3 console run.
