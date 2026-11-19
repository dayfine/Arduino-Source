# AutoStory: architecture

Written 2026-11-01, after FireRed's proof of concept: new game to the Boulder Badge on a Switch.
This is the current design. The older docs in this folder still hold useful detail, but where
they disagree, this one wins:
- `AutoStory.md` and `AutoStory-Design.md`: first principles, the screen machine.
- `AutoStory-Navigation-Stack.md`: the first shared core.
- `AutoStory-MVP-Plan.md`: BDSP's plan after run 1.
- `AutoStory-Lessons.md`: what each console round taught; it stays the log.

Two earlier proposals are folded into section 8 and their PRs closed: the event model
([#78](https://github.com/dayfine/Arduino-Source/pull/78)) and the FRLG demo plan with its
map-reuse review ([#56](https://github.com/dayfine/Arduino-Source/pull/56)). FRLG's events after
Brock stay in their own doc ([#130](https://github.com/dayfine/Arduino-Source/pull/130)).

PR order and sizes: [AutoStory-Demo-Plan.md](AutoStory-Demo-Plan.md) and [AutoStory-FRLG-Sizing.md](AutoStory-FRLG-Sizing.md).

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
   games (SwSh, LA) can plug in.
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
   **Room for error.** A step judges by the end state, not by naming every screen on the way.
   It gets a bounded number of safe inputs for anything it doesn't recognise (B advances a line
   but never starts a talk), then confirms the end state (menu, overworld, pose, banner), and
   fails only if that never comes. One extra dialog line or an unclassified screen must not fail
   a run. The bounds and the choice of safe input are explicit and unit-tested.
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
   - Planned: a game simulator (section 6a), which checks that every route ends on the planned
     tile or fails loudly, and exercises battles, items and whiteouts.
8. **Evidence by default.**
   - One log format for checkpoints, events and routes across games.
   - Run records (planned, [#116](https://github.com/dayfine/Arduino-Source/pull/116)/#132) keep each run's log, events and the frames of failed attempts.
   - Route pictures ([#125](https://github.com/dayfine/Arduino-Source/pull/125)) draw the planned walk against the walked one.

## 4. Layers and where they live

| Layer | Shared or per game | Code | Fork PR |
|---|---|---|---|
| Story select database, story index, stats | shared (with SV) | `Pokemon/AutoStory/` | [#43](https://github.com/dayfine/Arduino-Source/pull/43), [#44](https://github.com/dayfine/Arduino-Source/pull/44), [#45](https://github.com/dayfine/Arduino-Source/pull/45) |
| Grid, leg planner, LegWalker, route log, JSON maps (MapData) | shared | `Pokemon/Navigation/` | [#114](https://github.com/dayfine/Arduino-Source/pull/114) |
| Story events and segments: checkpoints, retry, the `[Checkpoint]`/`[Event]` log | shared | `Pokemon/AutoStory/Pokemon_StoryEvents*` | [#114](https://github.com/dayfine/Arduino-Source/pull/114) |
| Story steps, builder, executor interface; run options (dropdowns, Checks, language, attempts) | shared | `Pokemon/AutoStory/Pokemon_StorySteps*`, `Pokemon/Options/` | [#122](https://github.com/dayfine/Arduino-Source/pull/122) |
| Maps from the decomp, route planning | per game | generator in `devtools/maps/`; map JSON in `Resources/<Game>/Maps/` (Packages); planning in FRLG `Overworld/`, BDSP `Data/` | FRLG [#117](https://github.com/dayfine/Arduino-Source/pull/117); BDSP maps-v4, [#127](https://github.com/dayfine/Arduino-Source/pull/127) |
| Walker: movers, interrupts, recovery | per game, on the shared LegWalker | FRLG `Overworld/`; BDSP `Walk/` | FRLG [#118](https://github.com/dayfine/Arduino-Source/pull/118), [#119](https://github.com/dayfine/Arduino-Source/pull/119); BDSP [#129](https://github.com/dayfine/Arduino-Source/pull/129) |
| Program, story table, executor, actions, battle, screen readers | per game | `Programs/AutoStory/` | FRLG [#120](https://github.com/dayfine/Arduino-Source/pull/120); BDSP [#128](https://github.com/dayfine/Arduino-Source/pull/128) |
| Lab channel, run records, route pictures, lab CLI | dev tooling | `CommonFramework/Windows/Lab*`, `devtools/` | [#107](https://github.com/dayfine/Arduino-Source/pull/107), [#116](https://github.com/dayfine/Arduino-Source/pull/116)/#132, [#125](https://github.com/dayfine/Arduino-Source/pull/125), [#131](https://github.com/dayfine/Arduino-Source/pull/131) |

`Pokemon/Navigation/` holds what moves the player (grids, maps, legs, the walker); `Pokemon/AutoStory/`
holds what the story is made of (events, segments, steps, the select database). Navigation never
includes AutoStory.

Generated maps are game data, like `Resources/PokemonFRLG/EncounterSlotsFR.json`: they ship in
the Packages repo and load at runtime with `RESOURCE_PATH()`, so a full game's maps (about 1.5 MB for
FireRed) stay out of code review. The small test maps under `Pokemon/Navigation/TestMaps/` stay
compiled in, so unit tests need no resources. Today both games still compile their maps in; moving
them is a follow-up.

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

A 3D game (SwSh, LA) would keep the story engine and steps, and supply its own position type: a
map point or a fly point instead of tiles. That's why `Position` in the engine is opaque, and why
the route log is being made grid-free (planned).

| Game | Movement | Camera | Walk primitive |
|---|---|---|---|
| FRLG | strict tile grid | fixed, top-down | tile legs to wall, NPC or warp anchors |
| BDSP | tiles in practice | fixed, tilted | the same legs; D-pad taps for exact offsets |
| SwSh | free 3D | behind the player | stick direction = world direction; steer to a landmark, arrival by sight |
| LA | free 3D | free | as SwSh, plus camera control |

The first 3D target would be a SwSh run from a new game to the Wedgehurst station. Upstream
has a timed draft of its first segment ([PokemonAutomation#1207](https://github.com/PokemonAutomation/Arduino-Source/pull/1207));
offer the engine to its author rather than take it over.

## 6. Run modes (planned)

Strategy and checking are separate options:
- **Checks** (Thorough or Fast) says how much each step verifies.
- **Run mode** (SPEEDRUN, DEFAULT or SAFE) says which battles to take, whether to flee wild
  Pokemon, the heal threshold, and what to buy where.

SPEEDRUN follows a known speedrun route (pokeguy's LeafGreen 1:59:34 notes). SAFE heals early and
trains to level targets per checkpoint. Tables name the choices a mode switches. Trainer sight
from the decomp lets the planner avoid or seek trainers.

## 6a. Game simulator (planned; runs alongside the MVP, doesn't block it)

A seeded fake game that the story engine and walker run against, with odds taken from the decomp
so it behaves like the real game.

- **Tracked state.**
  - Party: species, level, HP, EXP, status, moves with PP.
  - Bag (Potions, Antidotes, Parlyz Heals, Repels, Poke Balls) and money.
  - Repel steps left.
  - The real tile and facing, beside the tile the walker believes.
  - The last Pokemon Center.
- **Encounters.** Each map's grass tiles, encounter rate and slots (species, levels) from the
  decomp, e.g. pokefirered `wild_encounters.json`. Each grass step rolls for an encounter.
  A Repel blocks encounters below the lead's level, counts down per step and wears off with its
  message.
- **Battles.** A simplified damage model: base stats, move power, type chart and dice. It produces
  HP loss per turn, status from the opponent's moves (e.g. Weedle's Poison Sting), turns to win,
  and the chance of a flee succeeding.
- **Battle aftermath.**
  - EXP and level-ups follow the decomp's yields and growth rates.
  - A new move from the learnset, with a full move set, raises the learn-move prompts.
  - A trainer starts its battle when the player steps into its line of sight.
  - Fainting with the whole party means a whiteout: the player goes back to the last Pokemon
    Center with half the money.
- **Timing.** The program notices a battle 2–5 s late while still holding the D-pad; the
  simulator reproduces that.
- **Uses.**
  1. Walk every story route across thousands of seeds and compare the believed tile with the real
     one. That's the drift bug.
  2. Exercise the rare paths the console seldom hits: Potions and status cures, the heal
     threshold, a Repel wearing off, learn-move prompts, whiteout recovery.
  3. Statistics per checkpoint (success rate, time, Potions used) under each run mode. These set
     the level targets and the trainer plan.
- **Placement.** The core is shared (#114 / #122). Each game supplies a data generator from its
  decomp (encounters, species, learnsets, trainer parties, type chart) and a fake executor.
- **Size.** About 1.2k code and 0.6k tests for the core, plus about 0.5k and generated data per
  game.
- **Steps.**
  1. Encounters, Repel and position: enough for the drift bug.
  2. HP, status, EXP, moves, whiteouts and run-mode statistics.

  Both run in parallel with the MVP; neither blocks it.

## 6b. Reusing the existing programs (planned)

The repo already automates many of the moments a story run passes through. AutoStory calls them
as steps instead of copying them; where a routine lives inside a program class, a small upstream
PR first moves it into a function both can call.

| Story moment | Existing FRLG code | How AutoStory uses it |
|---|---|---|
| Starter choice | `StarterRng` | Hit the RNG'd starter at Oak's table, then continue the story |
| Gift and static Pokemon (Lapras, Eevee, Snorlax, Mewtwo, legendary birds) | `GiftRng`, `StaticRng`, `LegendaryReset`, `RngNavigation`, `RngCalibration` | An optional RNG step at that story row; plain catch otherwise |
| Roaming dog | `RoamingLegendaryRng` | Optional, post-game |
| Elite Four and Champion | `EliteFourFarmer`, `PokemonLeagueDetectors`, `BattleMenuNavigation` | Its battle loop and League detectors run the last segment |
| Healing, party and box checks | `PartyMenuDetector`, `PartyEmptySlotDetector`, box readers | Pre-checks before RNG steps (an RNG program needs an empty party slot) |

An RNG step is a hand-off: AutoStory walks to the spot and saves, the RNG program runs with its
own options (seed, target, calibration), and AutoStory takes over again when the program reports
the Pokemon in the party. Both sides log to the same run record.

## 6c. Milestones after the MVPs

The MVPs (FRLG to Brock, BDSP to Floaroma) prove the engine. The larger goals, in order:

1. **End to end: new game → Hall of Fame.** Every badge, the Elite Four and the Champion in one
   run, with the default choices.
2. **End to end with options.** A different starter, different story choices and the steps they
   change (rival teams, gift picks such as the fossil), and optional tasks (extra catches, items,
   side areas).
3. **End to end with the Sevii Islands and Mewtwo.** The post-game story (Ruby, Sapphire, the
   Network Machine) and Cerulean Cave.
4. **Full RNG integration.** Every RNG-able Pokemon on the way (starter, gifts, statics, legendary
   birds, Mewtwo) can be RNG'd in place through the existing programs (section 6b).

Issue drafts per milestone are on the notes branch: `autostory/MILESTONES.md`. FRLG's events
from Brock to the Hall of Fame, the Sevii Islands and Mewtwo are drafted as table rows in
`AutoStory-FRLG-FutureEvents.md` ([#130](https://github.com/dayfine/Arduino-Source/pull/130)).

## 7. Known limits and open problems

- **Position drift after interrupts (FRLG Viridian Forest).** Four failures at the forest gate
  were most likely a wrong believed tile after battles. The drift hasn't been reproduced in 52
  hand-driven legs with 31 battles. A Thorough-mode check now fails a wall leg that didn't reach
  its wall, with a screenshot, to catch the next occurrence.
- **BDSP 00.01–00.05** are still recorded stick holds until the v4 table rewrite lands. Once it
  does, they need a console run from a new game.
- **Long upstream queue.** About 21 PRs for FRLG alone, at two open at a time.
- **The planner has no search limits.** A search over sets of possible tiles has no cap on
  stored states, queue entries or time. It needs finite limits with cancellation checks, and
  distinct results: route found, no route, limit hit (naming which one), cancelled. A limit hit
  doesn't mean the place is unreachable: re-localise against a nearer anchor or stop, never
  retry the same search with bigger limits. Plan between regions through connections and
  warps, so no map set becomes one huge canvas.
- **Not covered yet:** HMs and field moves, puzzles, the party beyond the starter, RNG hand-offs,
  naming with a keyboard ([#81](https://github.com/dayfine/Arduino-Source/pull/81), [#62](https://github.com/dayfine/Arduino-Source/pull/62)).

## 8. Detail kept from earlier proposals

### 8.1 Steps: conditions, anchors, talks and dialogues (from the event model, #78)

The event model proposed checkpoints as data before the story tables existed. The tables
(section 3, principle 2) took its shape; these rules still apply to how each step works.

- **What we took from prior art.** Pre- and post-conditions with "skip if already true"
  (behaviour trees' PPA pattern, BehaviorTree.CPP's `_skipIf`, GOAP), so resuming is
  re-checking, not re-counting. Wait for something observable, never sleep (Playwright's
  auto-waiting assertions, Screenplay's "questions"). Re-localise on landmarks, then re-plan
  (Nav2's recoveries). A route as data in a small fixed vocabulary interpreted by code
  (Honorbuddy quest profiles, pokebot-gen3's modes). Not taken: a general planner (GOAP, HTN)
  or a behaviour-tree runtime. The story is a fixed order; the only search is the tile planner.
- **Rows are C++, maps are JSON.** The event model left open whether checkpoints are JSON
  files or C++ literals. They are initializer-list rows: reviewers read them in the diff, the
  compiler checks them, and upstream ships no data files for programs. Maps stay JSON because
  they are generated from the decomp.
- **Anchors fix the position**, whatever happened before them:

  | Anchor | Pins | Example |
  |---|---|---|
  | walk until blocked by a wall | one axis | Route 201's corner: north, then west |
  | walk until a still NPC blocks | one axis, next to the NPC | Barry in the Trainers' School |
  | warp or transition, then the location banner | map and arrival tile | Sandgem's banner, a house door |
  | the NPC who answered a talk | next to that NPC | Mom's row is the row that answered |
  | facing reader | facing | FRLG's player sprite |

  A step that needs an exact tile comes after an anchor that gives one. In tall grass a wall
  doesn't show as blocked, so the leg presses A each step and stops on the talk instead.
- **Talks: an approach rule, then proof.**
  - Face a direction from an anchored tile (a still NPC).
  - Walk into a line, pressing A each step (an NPC that wanders along a known line).
  - Probe: face, press A, step sideways, up to N times (one axis known).
  - Each NPC has its own wait for the talk to open (story NPCs zoom first: 8 s for Roark).
  - An opened dialog isn't proof for a story NPC: a rock answers A too (r105). Check the
    effect (the camera zoom, the item, the NPC gone).
  - Advance with B where another A would start the talk again, with A where B would answer No.
- **Dialogs end on a screen, never on a count.** The end is the overworld held for a while (4 s
  covers a trainer's pause before the battle) or the battle menu. The time cap fails the step;
  it isn't the plan. Yes/No prompts are answered by their cursor, not by presses counted.
- **Interrupts say what they do to the position**: unchanged, somewhere on the trainer's sight
  line, or unknown. Never chain two unknown positions; after one, the next exact step needs an
  anchor.
- **Recoveries are named and bounded.** Each names the failure it handles (blocked for no
  reason, probe exhausted, expected screen missing, effect missing) and how often it may run.
  A transient cause (a wanderer, a wild battle) gets retries; a deterministic one doesn't.
  Anything else stops with a screenshot.
- **Resume is a re-check.** After a reload, a step whose skip condition holds on screen is
  skipped (Potions already in the bag: skip the Potion lady). A story flag is set only by a
  confirmed effect and is lost on reload, so either a save follows the step that sets it, or the
  flag can be observed again.

### 8.2 FRLG startup and RNG hand-offs (from the FRLG demo plan, #56)

The demo plan's map slices are done: the FRLG proof of concept walks Pallet to Pewter on maps
generated from pokefirered. Its rules for the RNG programs still apply to section 6b's hand-offs.

- **SID Helper** needs the game stopped on the naming screen with the name typed, before OK.
  Keep its input timing unchanged: observe before and after it, never insert waits into it. It
  doesn't save, so AutoStory must not reset the new identity before its first verified save.
- **Starter RNG** needs a save south of Oak's table facing the chosen ball, stopping before
  the ball is picked. It can end early on a shiny, or after rival battles and levelling during
  calibration: read the screen and party before resuming, and don't take the program finishing
  as proof of a saved starter. Keep the pre-starter save until the user keeps the result.
- **Record** the TID, the candidate SIDs, the chosen one and whether it is confirmed, with the
  game language and sound setting. A candidate SID is not a confirmed one, and a new game
  invalidates the list.
- **Keep the settings the RNG programs need** (Help button mode, the sound setting) the same
  between preparing and running.
- **A hand-off ends without input.** No A press, game close or reset after it. Failing before a
  verified save stops in place; it never reloads an assumed checkpoint.
- **Call, don't embed.** Where AutoStory needs part of an RNG program, a small PR first moves
  that routine into a function with an explicit result; one program never runs another.
