# AutoStory milestones and issue drafts

Drafts for one master tracking issue and one issue per milestone. They're not filed yet: where to
file them (upstream PokemonAutomation/Arduino-Source or the fork) is the user's call, and upstream
text is shown to the user before posting. Design: [AutoStory-Architecture.md](https://github.com/dayfine/Arduino-Source/blob/docs/autostory-architecture/docs/design/AutoStory-Architecture.md).
PR tree and sizes: [FRLG_SIZING.md](FRLG_SIZING.md), [DEMO_PLAN.md](DEMO_PLAN.md).

## Master issue (draft)

**Title:** AutoStory for FRLG and BDSP: map-based story programs (tracking)

> This tracks a shared AutoStory engine for 2D-movement Pokemon games: story beats as tables, movement planned on maps generated from the decomps, every step checked on screen, and checkpoints that save and retry. FireRed reached the Boulder Badge from a new game on a Switch as a proof of concept; BDSP from a new game to Floaroma is being rebuilt on the same engine. Work lands as a queue of small PRs, two open at a time; each milestone below has its own issue with scope, PRs and the done check.
>
> - [ ] M1 Shared AutoStory and navigation core
> - [ ] M2 FireRed MVP: new game → Boulder Badge
> - [ ] M3 BDSP MVP: new game → Floaroma
> - [ ] M4 Run evidence and lab tooling
> - [ ] M5 Game simulator
> - [ ] M6 Run modes, trainer sight and level targets
> - [ ] M7 Names and starter RNG
> - [ ] M8 FireRed end to end: new game → Hall of Fame
> - [ ] M9 Other games: grid-free routes for 3D titles (later)
> - [ ] M10 FireRed end to end with options: other starters, choices, optional tasks
> - [ ] M11 FireRed end to end with the Sevii Islands and Mewtwo
> - [ ] M12 Full RNG integration

## M1 Shared AutoStory and navigation core
- **Scope:** shared select database, story index and stats (SV refactors), grid and route planner, ScreenMachine/StepTrace, JSON maps and loader, LegWalker and the route log, story events and segments, story steps and run options (checkpoint dropdowns, Checks, language, attempts).
- **PRs:** U1–U10, from fork [#43](https://github.com/dayfine/Arduino-Source/pull/43), [#44](https://github.com/dayfine/Arduino-Source/pull/44), [#45](https://github.com/dayfine/Arduino-Source/pull/45), [#114](https://github.com/dayfine/Arduino-Source/pull/114), [#122](https://github.com/dayfine/Arduino-Source/pull/122). About 5k code, 3.3k tests.
- **Done when:** all merged upstream, with unit tests green on three platforms. No game behaviour changes except SV's dropdowns, which keep the same labels.
- **Follow-ups:** a joined multi-map view and coord events in the map schema (requested by BDSP).

## M2 FireRed MVP: new game → Boulder Badge
- **Scope:** FRLG maps generated from pret/pokefirered (25 maps), route planning, the walker with interrupts and recovery, a position check that fails a wall step that stopped short, and the AutoStory program and table (00.01–00.09, with Mart and bag checks, the save-place check and language-free reads).
- **PRs:** U11–U19, from fork [#117](https://github.com/dayfine/Arduino-Source/pull/117), [#118](https://github.com/dayfine/Arduino-Source/pull/118), [#119](https://github.com/dayfine/Arduino-Source/pull/119), [#120](https://github.com/dayfine/Arduino-Source/pull/120). About 7.8k code, 2.1k tests, 3.2k map data.
- **Done when:** the story simulation passes, and the console goes 3/3 from a new game to Brock in one run each, with run records.
- **Known issue:** forest-gate position drift (4 failures on 2026-11-01, not reproduced in 52 legs); the position check catches the next one.

## M3 BDSP MVP: new game → Floaroma
- **Scope:** BDSP JSON maps from pret/pokeplatinum, including interiors; story maps; the walker; actions, battle and screen readers; and the program with an FRLG-style table. No dependency on PokemonAutomation#1717; the pieces kept from it are credited.
- **PRs:** bdsp/maps-v4 → [#127](https://github.com/dayfine/Arduino-Source/pull/127) → [#129](https://github.com/dayfine/Arduino-Source/pull/129) → bdsp/actions-v4 → [#128](https://github.com/dayfine/Arduino-Source/pull/128). Replaces fork #111, #112, #121 and #124.
- **Done when:** the simulation passes in English and Japanese, and the console goes 3/3 from a new game (00.01–00.05 are new planned walks and need console proof).

## M4 Run evidence and lab tooling
- **Scope:** run records (one folder per run: log, events, frames of failed attempts, build info), run-event hooks, route pictures (planned vs walked), the lab channel, and one lab CLI with guards (MCP optional later). Also the lab-channel fix: a window focus change cancels holds.
- **PRs:** [#132](https://github.com/dayfine/Arduino-Source/pull/132) (recorder), [#116](https://github.com/dayfine/Arduino-Source/pull/116) (lab half), [#107](https://github.com/dayfine/Arduino-Source/pull/107), [#125](https://github.com/dayfine/Arduino-Source/pull/125), [#131](https://github.com/dayfine/Arduino-Source/pull/131).
- **Done when:** a failed AutoStory run leaves a record with the failing attempt's frame, and a route picture can be drawn from that record.

## M5 Game simulator
- **Scope:** a seeded fake game with odds from the decomp: encounters, Repel, battles (HP, status, EXP, level-ups, learn-move prompts), trainers, whiteouts, and the battle-detection lag.
- **PRs:** 2 (encounters, Repel and position; then the full battle model and run-mode statistics). About 1.2k code, 0.6k tests for the core, plus about 0.5k and data per game.
- **Done when:** every story route runs across 1,000+ seeds with no silent position errors, and per-checkpoint statistics are produced. Doesn't block M2 or M3.

## M6 Run modes, trainer sight and level targets
- **Scope:** SPEEDRUN, DEFAULT and SAFE run modes (separate from Checks), trainer type, sight and facing in the map schema, the planner avoiding or seeking trainers, level targets per checkpoint, and the FRLG speedrun route (after pokeguy's LeafGreen 1:59:34).
- **PRs:** about 5. About 1.6k code, 1k tests.
- **Done when:** FRLG reaches Brock in each mode on the console, and the simulator's statistics match.

## M7 Names and starter RNG
- **Scope:** typing names (FRLG naming screen [#81](https://github.com/dayfine/Arduino-Source/pull/81); BDSP Switch keyboard [#62](https://github.com/dayfine/Arduino-Source/pull/62)), with "Kosmos" as the lab default; and a starter-RNG hand-off from AutoStory to the FRLG gift RNG program ([#61](https://github.com/dayfine/Arduino-Source/pull/61) party check, plus an entry point).
- **PRs:** about 4–5. Can start now; it joins the story program after M2.
- **Done when:** AutoStory names the player and gets an RNG'd starter, then continues to Brock.

## M8 FireRed end to end: new game → Hall of Fame
- **Scope:** segments from Pewter to the Hall of Fame, following the outline in [#130](https://github.com/dayfine/Arduino-Source/pull/130) (92 rows). Needs HMs and field moves, puzzles, party management, and the Silph Scope and Poke Flute events.
- **PRs:** about 10–15, one per segment. About 8–12k code, 3–4k tests.
- **Reuse:** the Elite Four and Champion segment builds on `EliteFourFarmer` and `PokemonLeagueDetectors`.
- **Done when:** each segment passes 3/3 on the console, and one run goes from a new game to the Hall of Fame with default choices.

## M9 Other games (later)
- **Scope:** a grid-free route log and position type, and adapters for 3D games (SwSh, LA) on the same story engine.
- **Done when:** one 3D-game segment runs on the shared steps.

## M10 FireRed end to end with options
- **Scope:** table rows that branch on a choice: the other two starters (rival teams change), gift picks such as the Mt. Moon fossil, and optional tasks (extra catches, items, side areas) a run can switch on.
- **PRs:** about 4–6, after M8.
- **Done when:** a new game → Hall of Fame run passes with each starter, and each optional task passes 3/3 in its segment.

## M11 FireRed end to end with the Sevii Islands and Mewtwo
- **Scope:** the post-game story (Islands One to Seven, the Ruby and Sapphire, the Network Machine) and Cerulean Cave up to Mewtwo.
- **PRs:** about 6–8, after M8. Sevii maps come from the same pret/pokefirered generator.
- **Done when:** one run goes from the Hall of Fame save to Mewtwo caught, with each segment 3/3.

## M12 Full RNG integration
- **Scope:** every RNG-able Pokemon on the way can be RNG'd in place: starter (M7), gifts (Lapras, Eevee, fossils), statics (Snorlax, legendary birds, Mewtwo) and the roaming dog, through `StarterRng`, `GiftRng`, `StaticRng`, `RoamingLegendaryRng` and the RNG navigation. AutoStory walks there and saves, the RNG program runs, AutoStory continues. Routines locked in program classes move to callable functions in small upstream PRs first.
- **PRs:** about 5–8, after M7; the post-game targets after M11.
- **Done when:** a full run with RNG switched on ends with each targeted Pokemon matching its RNG target.
