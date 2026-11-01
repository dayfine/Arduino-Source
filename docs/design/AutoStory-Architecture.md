# AutoStory: architecture

Written 2026-11-01, after FireRed's proof of concept: new game to the Boulder Badge on a Switch.
This is the current design. The older docs in this folder still hold useful detail, but where
they disagree, this one wins:
- `AutoStory.md` and `AutoStory-Design.md`: first principles, the screen machine.
- `AutoStory-Navigation-Stack.md`: the first shared core.
- `AutoStory-MVP-Plan.md`: BDSP's plan.

PR order and sizes are on the notes branch: `autostory/DEMO_PLAN.md` and `autostory/FRLG_SIZING.md`.

## 1. The problem

A story program plays a Pokemon game from a new game (or a save) to a milestone: a badge, a city,
a legendary. Existing AutoStory programs, such as SV's and the BDSP draft in [PokemonAutomation#1717](https://github.com/PokemonAutomation/Arduino-Source/pull/1717),
are code per segment: recorded stick holds and timed button presses, written in order and tuned
on one console. They work, but they're hard to review, they break when a detail changes, and each
new segment means new code.

Goals:
1. **Reliable.** Every step checks what it did on screen. A failure stops with a screenshot and
   a reload from the last save, and never drifts on.
2. **Data, not code.** A story is a table of beats a reviewer can read; places come from map data,
   not from timed holds.
3. **Shared.** FRLG (2D, tiles) and BDSP (3D camera, tile movement) use one engine, and later 3D
   games (SV, LA) can plug in.
4. **Testable without a console.** The tables, routes and retry policy run in unit tests and
   simulations; the console confirms.

## 2. The approach in one picture

```
 story table (data)          maps (data, from the decomp)
 checkpoints → steps         tiles, warps, objects, named points
        │                              │
        ▼                              ▼
 story engine ──── route planner (legs that end on walls, warps, NPCs)
 retries, saves,              │
 reloads, stats               ▼
        │                  walker ── interrupts: wild battles, trainers, signs, messages
        ▼                     │       recovery: find the player again, re-plan
 game executor ───────────────┘
 talk, answer, battle, heal, shop, save (each checked on screen)
        │
        ▼
 console (Switch over the controller), video in
 logs: [Segment] [Checkpoint] [Event] [Route] lines; run records (frames, events)
```

## 3. Principles

1. **Map-based movement.** Every walk is planned on a tile map made from the game's
   decompilation: pret/pokefirered for FRLG, pret/pokeplatinum for BDSP's Sinnoh. Maps are
   JSON files built into the program, with tiles, elevation, warps, objects, map connections and
   named points (e.g. `mom`, `door`, `barry_talk`). The planner chooses **legs that end on
   something the game itself stops at**: a wall, a warp, an NPC. The player's position is then
   known when the leg ends, without reading it off the screen. Timed holds are the exception, and
   each one is marked.
2. **Story as events.** A story is checkpoints, each with a start pose, the supplies it expects,
   and an ordered list of steps in one shared vocabulary:
   NEW_GAME, OPTIONS, ROUTE, LEGS, STEP, TALK, ANSWER, MASH, BATTLE, TRAIN, HEAL, SHOP, SAVE,
   READY, plus GAME steps a game defines by name.
   - Rows are initializer lists, one per story beat, each with a short reason.
   - A beat that can't be a row, such as the intro or the starter choice, is a small named
     handler. The table still names it.
   - FRLG's `PokemonFRLG_AutoStoryTable.cpp` is the reference shape.
3. **Checkpoints are save points.** The game is saved at the start of each checkpoint, so a
   failure reloads that save and retries the checkpoint: 3 attempts by default, or stop at the
   first failure. A checkpoint's start pose must equal the previous checkpoint's end pose; tests
   check this.
   - On load, the program reads the place on the save card and refuses to run if it doesn't match
     the checkpoint's start.
4. **Closed loop.** Each step confirms its effect on screen:
   - a fade for a warp;
   - a dialog box opening and closing;
   - the battle menu;
   - the overworld coming back.
   The Checks level chooses how much: Thorough (the default) also reads the Mart row, the bag
   counts and the item before it's used; Fast trusts counts and timings.
5. **Interrupts are expected.** A walk watches for wild battles, trainers, signs and messages:
   - It handles them through game callbacks (fight or flee, mash B back to the overworld).
   - It then resumes. A leg to a wall is simply held again; an open leg counts the tiles walked.
   - If the position is in doubt, it finds the player again against a known wall and re-plans.
   - Every leg end is logged as walked, assumed or unknown.
6. **No language dependence.** Steps never match dialog or menu text; they read boxes, arrows,
   fades, colours and positions. Where a step must read text (item names in the Mart or bag), it
   takes the game language. With no reader for that language it falls back to positions and
   digits, and never fails because of the language.
7. **Tests before consoles.**
   - Route tests plan every story route on the JSON maps.
   - A story simulation runs the whole table against a fake game (failure injection, reloads,
     the Mart cursor) in English and Japanese.
   - The console runs that remain verify the result.
   - Planned: a walker simulator with a grid world and seeded fake battles, which checks that every
     route either ends on the planned tile or fails loudly.
8. **Evidence by default.**
   - One log format for checkpoints, events and routes across games.
   - Run records (planned, [#116](https://github.com/dayfine/Arduino-Source/pull/116)/#132) keep each run's log, events and the frames of failed attempts.
   - Route pictures ([#125](https://github.com/dayfine/Arduino-Source/pull/125)) draw the planned walk against the walked one.

## 4. Layers and where they live

| Layer | Shared or per game | Code | Fork PR |
|---|---|---|---|
| Story select database, story index, stats | shared (with SV) | `Pokemon/AutoStory/` | [#43](https://github.com/dayfine/Arduino-Source/pull/43), [#44](https://github.com/dayfine/Arduino-Source/pull/44), [#45](https://github.com/dayfine/Arduino-Source/pull/45) |
| Grid, leg planner, LegWalker, route log, JSON maps (MapData) | shared | `Pokemon/Navigation/` | [#114](https://github.com/dayfine/Arduino-Source/pull/114) |
| Story events and segments: checkpoints, retry, the `[Checkpoint]`/`[Event]` log | shared | `Pokemon/Navigation/Pokemon_StoryEvents*` | [#114](https://github.com/dayfine/Arduino-Source/pull/114) |
| Story steps, builder, executor interface; run options (dropdowns, Checks, language, attempts) | shared | `Pokemon/Navigation/Pokemon_StorySteps*`, `Pokemon/Options/` | [#122](https://github.com/dayfine/Arduino-Source/pull/122) |
| Maps from the decomp, route planning | per game | FRLG `Overworld/`; BDSP `Maps/` + `Data/` | FRLG [#117](https://github.com/dayfine/Arduino-Source/pull/117); BDSP maps-v4, [#127](https://github.com/dayfine/Arduino-Source/pull/127) |
| Walker: movers, interrupts, recovery | per game, on the shared LegWalker | FRLG `Overworld/`; BDSP `Walk/` | FRLG [#118](https://github.com/dayfine/Arduino-Source/pull/118), [#119](https://github.com/dayfine/Arduino-Source/pull/119); BDSP [#129](https://github.com/dayfine/Arduino-Source/pull/129) |
| Program, story table, executor, actions, battle, screen readers | per game | `Programs/AutoStory/` | FRLG [#120](https://github.com/dayfine/Arduino-Source/pull/120); BDSP [#128](https://github.com/dayfine/Arduino-Source/pull/128) |
| Lab channel, run records, route pictures, lab CLI | dev tooling | `CommonFramework/Windows/Lab*`, `devtools/` | [#107](https://github.com/dayfine/Arduino-Source/pull/107), [#116](https://github.com/dayfine/Arduino-Source/pull/116)/#132, [#125](https://github.com/dayfine/Arduino-Source/pull/125), [#131](https://github.com/dayfine/Arduino-Source/pull/131) |

A game adds: maps (a generator from its decomp), movers (how one tile is walked), interrupt
handlers, an executor for the step kinds, and its table. Everything else is shared.

## 5. What the games differ in

| | FRLG | BDSP |
|---|---|---|
| Movement | D-pad, one tile per press or hold | stick or D-pad on tiles; the camera follows |
| Position | exact tile after a leg ends on a wall | a set of possible tiles, narrowed by walls, doors and location banners |
| Maps | 25 maps, Pallet → Pewter | Twinleaf → Floaroma, plus interiors (added from the decomp, unverified) |
| Text reading | item names only (Mart, bag) | Mart items, location banners |
| State | proof of concept: new game → Brock, passed in pieces | v3 reached Floaroma on #1717's code; v4 without #1717 is being written |

A 3D game (SV, LA) would keep the story engine and steps, and supply its own position type: a
map point or a fly point instead of tiles. That's why `Position` in the engine is opaque, and why
the route log is being made grid-free (planned).

## 6. Run modes (planned)

Strategy and checking are separate options:
- **Checks** (Thorough or Fast) says how much each step verifies.
- **Run mode** (SPEEDRUN, DEFAULT or SAFE) says which battles to take, whether to flee wild
  Pokemon, the heal threshold, and what to buy where.

SPEEDRUN follows a known speedrun route (pokeguy's LeafGreen 1:59:34 notes). SAFE heals early and
trains to level targets per checkpoint. Tables name the choices a mode switches. Trainer sight
from the decomp lets the planner avoid or seek trainers.

## 7. Known limits and open problems

- **Position drift after interrupts (FRLG Viridian Forest).** Four failures at the forest gate
  were most likely a wrong believed tile after battles. The drift hasn't been reproduced in 52
  hand-driven legs with 31 battles. A Thorough-mode check now fails a wall leg that didn't reach
  its wall, with a screenshot, to catch the next occurrence.
- **BDSP 00.01–00.05** are still recorded stick holds until the v4 table rewrite lands. Once it
  does, they need a console run from a new game.
- **Long upstream queue.** About 21 PRs for FRLG alone, at two open at a time.
- **Not covered yet:** HMs and field moves, puzzles, the party beyond the starter, RNG hand-offs,
  naming with a keyboard ([#81](https://github.com/dayfine/Arduino-Source/pull/81), [#62](https://github.com/dayfine/Arduino-Source/pull/62)).
