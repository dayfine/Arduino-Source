# FRLG AutoStory: code size by PR, and what remains

Measured on 2026-11-01 at about 13:10, as `git diff --numstat parent...child` on the fork branches.
"Code" excludes tests, map data (JSON), docs and cmake. The FRLG chain is being restacked, so these
shapes are close to final, not final. The plan and PR tree are in [DEMO_PLAN.md](DEMO_PLAN.md).

## 1. Prep work (small refactors before the shared core)

| PR | What | Code | Tests |
|---|---|---|---|
| #43 | Shared start/end checkpoint dropdowns (SV refactor) | 56 | 0 |
| #44 | Shared story index | 33 | 99 |
| #45 | AutoStoryStats moved to Pokemon/AutoStory | 38 | 0 |
| **Total** | Ready to send upstream first | **127** | **99** |

Outside the chain but related: #86 FRLG bag detector tests, and #61 FRLG party check (240 code,
263 tests), which belongs with the RNG work.

## 2. Shared navigation and map infrastructure (#114)

| Part | Code | Tests |
|---|---|---|
| Grid, route planner, LegWalker, MapData/JSON maps, route log, tile poses, map embedding | 1,813 | 1,010 |
| ScreenMachine / StepTrace | 329 | 0 |
| **#114 navigation part** | **~2,140** | **~1,010** (+151 lines of test maps) |

## 3. Shared AutoStory infrastructure

| PR | What | Code | Tests |
|---|---|---|---|
| #114 (story part) | Story events and segments as tables | 1,021 | 966 |
| #122 | Shared story steps, retry policy, run options (Checks, language, dropdowns, stats) | 838 | 637 |
| **Total** | | **~1,860** | **~1,600** |

#114 holds both the navigation and the story-event code (~3.2k code). Upstream it could be split
in two: navigation first, then story events.

## 4. FRLG game program (the MVP)

| PR | What | Code | Tests | Map data |
|---|---|---|---|---|
| #117 | FRLG maps (generator + 25 JSON maps) and route planning | 3,249 | 821 | 3,194 |
| #118 | FRLG walker: steps, warps, grass, signs, ledges | 690 | 0 | |
| #119 | Finding the player again after an interrupted step | 739 | 436 | |
| #120 | FRLG AutoStory, new game → Brock: table, Mart and bag checks, save check, simulation | 3,124 | 837 | |
| **Total** | | **~7,800** | **~2,100** | **3,194** |

#123 is fully merged into #120, shows no diff, and can be closed. #117 is large; splitting the
generator and maps from the planner is an option.

## 5. Still needed for the MVP (estimates)

| # | PR | Where | Size (code + tests) |
|---|---|---|---|
| a | Walker simulator: grid world, fake battles, believed vs real tile | #114 | ~400 + 400 |
| b | Walker fixes: drift after battles; fail a wall step that stops far short | #119 | ~150 + 200 |
| c | Run-record recorder (framework half of #116) | new PR on main | ~630 incl. tests |
| d | Run-event hooks: shared code and FRLG | #114, #120 | ~150 + 100 |
| e | Optionally split #117: maps and generator vs planner | existing code | 0 new |
| | **Total** | 2–3 new PRs plus commits on existing ones | **~1.3k code + ~1k tests** |

Then the console check: FRLG 3/3, new game → Brock, in one run each.

## 6. Later features (rough estimates)

| Feature | PRs | Code + tests | Notes |
|---|---|---|---|
| Run modes SPEEDRUN / DEFAULT / SAFE | 3 | ~1.0k + 0.6k | (1) the mode option and strategy in #122; (2) FRLG level and party reading; (3) the FRLG speedrun route (no Viridian shopping, flee wild battles, Sammy only, Brock at level 11; pokeguy's 1:59:34 notes on the agents branch) |
| Trainer sight and level targets | 2 | ~0.6k + 0.4k | Trainer type, sight and facing in the map schema and generator; the planner scores sight lines and grass |
| RNG program integration | 2–3 | ~0.8k + 0.4k | AutoStory stops at a checkpoint and hands off to the RNG programs (#61, Ten Lines seeds), then resumes the story |
| Mail glitch / ACE and detours | 3–4 | ~1.5k + 0.6k | Builds on #81's naming-screen typer (722 code, 345 tests): mail-screen typing, the payload with verification, story detours to get the mail |
| Story after Brock (#130's outline, 92 rows) | ~10–15 | ~8–12k + 3–4k | About one PR per segment, 500–1,000 lines each, plus new event types (HMs, puzzles, the Silph Scope) |

## Tooling, used by both games

| PR | What | Size |
|---|---|---|
| #107 | Lab tooling: lab channel, Command File Runner, scripts | 2.4k code, 0.5k tests |
| #116 | Run records, about 3 PRs | ~1.5k |
| #125 | Route pictures (devtools/maps, Python) | 1.5k code, 0.4k tests |
| #131 | Agent integration: one lab CLI, MCP later; about 7 PRs | ~2k |

## Summary

The FRLG MVP is about 7.8k lines of game code on about 4.1k of shared code (navigation 2.1k plus
story engine 1.9k), with about 3.8k lines of tests. Finishing it needs roughly 1.3k more code plus
tests and one 3/3 console run.
