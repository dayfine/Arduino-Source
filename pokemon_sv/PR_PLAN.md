# BBQ work → upstream PRs: layered plan

Written 2026-10-26 from lab/r10 at f20d06c7b against upstream/main d47a41f82.
lab/r10 is 148 non-merge commits ahead (+9.5k / −0.9k in `SerialPrograms/Source`); about 100 of
those commits (+5.5k net) are in no fork PR yet. This plan says how all of it becomes reviewable
PRs, ordered by layer and grouped into chains, ending with the user docs.

## Ground rules

- **Rebuild, don't cherry-pick.** Lab commits mix lab hooks (`BBQLabConsole`, `bbq_lab_event`,
  `BBQLabFlags`) into gameplay hunks. Each PR branch is cut fresh from `upstream/main` (or its
  parent) and the change is re-applied by hand without the hooks. lab/r10 keeps a thin lab-only
  layer on top.
- **One concern per PR, about 400 changed lines or fewer**, not counting moved code, tests and
  reference images. Moves get their own PR. Behavior changes are separate and say so in the title.
- **Layers point down only.** L1 cores land before the L2 routines that call them, L2 before L3.
- **Upstream is serial.** At most two open there; everything else waits as fork drafts in the
  order below, and the user publishes the next one when a slot frees.
- **Freeze before extracting.** Routes still being tuned are extracted only after a run confirms
  them; tuning continues on lab.

## The three layers, mapped to upstream's layout

Upstream has no `Routines/` folder. Shared code lives either directly in `PokemonSV/Programs/`
(`WorldNavigation`, `MenuNavigation`, `Terarium`, `SaveGame`) or in a `*Routines`/`*Tools` file
in a topic folder (`Battles/BasicCatcher`, `ShinyHunting/LetsGoTools`, `Eggs/EggRoutines`).

| Layer | Home | Ours that goes here |
|---|---|---|
| L1 core: detectors, data | `Inference/**`, `Pokemon/`, `CommonFramework/` | MapMenuDetector snow fix, PlaceNameBannerReader, PrideRockDetector, TeraGlowDetector, SpeciesTypes table, exception null-stream fix |
| L1 core: controls | new `Programs/PokemonSV_OverworldControls` (decided) | lock-on (with recall on a silent miss), ride toggle moved out of `AutoStoryTools`, photo-mode open/shoot/close |
| L2 routine: map, fly | `Programs/PokemonSV_WorldNavigation` | map zoom before cursor flights, `closest_flypoint_to_player`, `max_tries` on Pokémon Center flights |
| L2 routine: Terarium | `Programs/PokemonSV_Terarium*` | checked plaza return, landing-banner arrival check, position state object (replaces the global `away_from_plaza`) |
| L2 routine: battle, catch | `Battles/PokemonSV_Battles`, `BasicCatcher` | testable escape (Arena Trap, fight when it can't flee), BasicCatcher options, BBQ's ball loop replaced by `throw_ball` |
| L2 routine: Let's Go | `ShinyHunting/PokemonSV_LetsGoTools` | `stop_early` on `use_lets_go_to_clear_in_front` |
| L2 routine: options, day skip | `Programs/PokemonSV_GameOptions` (new), `MenuNavigation` | `set_game_options_for_programs`, BBQ day skip rebuilt on `day_skip_from_overworld` |
| L2 routine: BBQ spot routes | new `Programs/Farming/PokemonSV_BlueberryRoutes` (recommended) | `central_to_pride_rock`, Rabsca, Slowpoke beach, Coastal Plaza hill and grassland, fly back to Coastal |
| L3 program | `Programs/Farming/PokemonSV_BBQ*`, `Blueberry*` | planner, rerolls, retries, milestones, quest policy, Auto Battle loop, stats, quest-to-route wiring |
| lab only | lab/r10 top layer, never upstream | BBQLabConsole, BBQRoutineBench, BBQLab hooks and reference screenshots, run notes, test glue, macOS signing |

Lock-on is new to the codebase: upstream only uses ZL as map zoom. Its result type should leave
room for the planned species check and target switch, e.g.
`struct LockOnResult { bool locked; std::optional<std::string> species; };`.

## Tree

Sizes are estimated changed lines: `code + tests`. Moves are counted separately as `moved`.
`#N` is a fork PR, `U#N` an upstream PR. `[new]` has no PR yet. `[open]` exists and only needs
rebasing. ⚠ marks a PR that needs rework before it advances.

```
upstream/main
├── U#1631 quest identity → Options module ............................ in review
│   └── B1 [#2 trimmed] BBQ log lines: board, attempt start/return/result ... 300 + 100
│       └── B2 [#3 + lab events] snapshots at named events, off by default .. 200 + 80
├── U#1628 BP reader crop (waits on CommandLineTests#36) ............... in review
│   ├── #7  quest detector tests ......................................... 115 + 105
│   └── B3 [#16] BP earned/spent/net and BP/hour in stats ................ 270 + 140
│
├── P  prep moves (pure refactors, review with --color-moved)
│   ├── P1 [new] split PokemonSV_Tests.cpp by area .................... moved 120
│   ├── P2 [new] BBQ spot routes → PokemonSV_BlueberryRoutes .......... moved 400
│   └── P3 [new] BlueberryCatchPhoto → BlueberryCatch + BlueberryPhoto  moved 1,050
│
├── A  L1 cores (independent of each other)
│   ├── A1 [new] MapMenuDetector snow fix (+5 images) ..................... 6 + 20
│   ├── A2 [new] don't crash reporting an error with no stream/screenshot . 13
│   ├── A3 [new] Pokemon_SpeciesTypes table ............................ 300 + 40
│   ├── A4 [new] PlaceNameBannerReader ................................. 100 + 60
│   ├── A5 [new] PrideRockDetector ..................................... 130 + 80
│   ├── A6 [new] TeraGlowDetector (+14 images) ......................... 120 + 100
│   └── A7 [new] OverworldControls: lock-on, ride, photo mode .. 200 + 80, moved 60  ⚠ lock-on-first rework
│
├── C  L2 navigation
│   ├── C1 [new] map zoom + closest fly point + max_tries .............. 120 + 40
│   └── C2a [#24 core] PlazaReturnActions seam, checked return ......... 450 + 250
│       ├── C2b [#24 rest + 6 lab follow-ups] landing-banner arrival (A4)  250 + 60
│       │   └── C4 [new] position object, start-up position check ...... 150 + 60
│       │       └── C5a [new] LeaveSpotActions seam + escape ........... 350 + 150   (after D1)
│       │           ├── C5b [new] direct flights between quests ........ 300 + 80
│       │           └── C5c [new] reload spot from Central ............. 150 + 50
│       └── C3 [#30] bound the return (behavior change) ................. 50 + 45
│
├── D  L2 battle & catch
│   ├── D1 [new] escape logic into run_from_battle, with a seam ........ 200 + 100
│   ├── D2 [new] BasicCatcher: use 1st move, switch slot, fainted lead . 120 + 40
│   │   └── D3 [new] BBQ catch via throw_ball + opponent type check (A3)  150 + 60
│   └── D4 [#23] catch grace, skip heal after no battle (behavior) ...... 70 + 75
│
├── E  L2/L3 Auto Battle  [open chain]
│   └── #15 seam ....................................................... 200 + 190
│       └── #17 stop at target (behavior) ................................ 60 + 20
│           └── #18 give up after empty passes (behavior) ................ 65 + 60
│               └── #21 heal only below 50% (behavior) .................. 90 + 110
│                   └── #31 pass log adds up to kill total .............. 20 + 47
│                       └── #32 stop when quest leaves board (behavior) .. 35 + 70
│                           └── E7 [new] pickup-10, fly back onto Chargestone, stop_early  230 + 60
│
├── F  L3 farmer controller  [open chain]
│   ├── #37 BBQGameActions seam ........................................ 215 + 220
│   │   ├── #5 endless-reroll guard ....................................... 50 + 60
│   │   ├── #4 save milestones ............................................ 20 + 20
│   │   └── #6 persistent retries ........................................ 55 + 70
│   └── #38 quest planner .............................................. 335 + 150
│       └── #8 verified reroll ⚠ fold in-panel reroll ................... 205 + 215
│           ├── #27 reroll log line ....................................... 15 + 18
│           ├── #25 reroll impossible quests on sight (behavior) ........ 95 + 100
│           └── #10 out-of-retries action (also on #6) ⚠ split ........ 560 + 390
│               └── #12 reuse board ⚠ ...................................... 150 + 110
│                   └── #13 replan after reroll ............................ 20 + 70
│                       └── #22 panel-read reuse ⚠ .......................... 80 + 150
│                           └── #26 batch no-trip quests ................... 75 + 70
│                               └── F14 [new] quest policy: BP/s order, BP/min threshold, in-panel reroll (behavior)  400 + 150
│                                   └── F15 [new] set game options at start-up  110
│
├── G  L2 routes per quest (each live-validated; needs A, C, D)
│   ├── G1 [new] Pride Rock: route + landing check + Pyroar aim; normal/fire photo & catch, catch-any, sneak-up, Tera-self  300 + 40
│   ├── G2 [new] Rabsca / catch bug: glide, glow follow, lock-on ........ 200 + 30
│   ├── G3a [new] Coastal: flying catch, Poison/Dark hill ............... 250 + 30
│   ├── G3b [new] Slowpoke beach: Psychic photo + sneak-up .............. 200 + 20
│   ├── G4 [new] photo-fly Geodude: lock-on-first + distance-gated back-off 100  ⚠ 0/2 in R23
│   ├── G5 [new] Rock/Electric lock-on catch ............................. 60
│   ├── G6 [#35] Kleavor look-around ........................................ 60
│   ├── G7 [#33] verify egg before hatching ⚠ rebase to its 2 commits ...... 30
│   └── G8 [#29 + lab fixes] Make TM: resume, close message, look ahead .. 150
│
└── Z  docs (last)
    ├── Z1 [new] program description and option tooltips ................. 60
    └── Z2 [new] pokemonautomation.github.io BBQSoloFarmer.md ............ 120 (Markdown)
```

## Totals

| Chain | PRs | Code | Tests | Moved |
|---|---:|---:|---:|---:|
| In review (U#1631, U#1628) | 2 | — | — | — |
| B diagnostics + #7 | 4 | 885 | 425 | |
| P prep moves | 3 | | | 1,570 |
| A L1 cores | 7 | 870 | 380 | 60 |
| C navigation | 8 | 1,820 | 735 | |
| D battle & catch | 4 | 540 | 275 | |
| E Auto Battle | 7 | 700 | 560 | |
| F controller | 15 | 2,385 | 1,795 | |
| G routes | 9 | 1,350 | 120 | |
| Z docs | 2 | 60 | | (+120 Markdown) |
| **Total** | **61** | **≈8,600** | **≈4,300** | **≈1,630** |

Existing PRs are measured against their own bases (code = changed lines minus test lines). New
PRs are estimated from the lab commits of their theme. Together that is close to lab/r10's
gameplay change once the lab-only tooling (about 1.6k lines) is left out.

At two upstream slots and recent merge pace (two or three a week) this is months of review. So:
- **A, P, D first.** Small, independent and cheap to review; they bank progress.
- **E and F already exist** as fork drafts and need rebasing, not rework (except the ⚠ ones).
- **G is where the BP/h is.** Each route ships once its A/C/D parents land.

## Decisions

1. **OverworldControls:** a new file. *Decided 2026-10-26.*
2. **CommonFramework exception fix (A2):** recommended to send, early and alone. Upstream
   `FatalProgramException::send_fatal_error_notif_and_telemetry_report()` and both report paths of
   `OperationFailedExceptionWithScreenshot` dereference `*m_screenshot` and `m_stream->history()`
   unconditionally, but those exceptions can be built from a message alone. That crashed the app 5
   times on 2026-10-23. The fix is 13 lines in 2 files and affects every game, so it gets its own
   PR with a cross-game title.
3. **Spot routes:** recommended to move into a new BBQ-owned `PokemonSV_BlueberryRoutes`, not into
   `Terarium`. A spot route is the movement from a plaza to where a quest's Pokémon spawn
   (`central_to_pride_rock`, `central_to_slowpoke_beach`, `coastal_plaza_to_grassland`,
   `fly_back_to_coastal_plaza`, the Rabsca glide). Upstream's `Terarium` already holds plaza-to-plaza
   flights (`central_to_polar_plaza` and others), but only BBQ calls them, and our routes depend on
   BBQ detectors and quest knowledge. A routes file shrinks `BlueberryQuests.cpp` and
   `BlueberryCatchPhoto.cpp` under the 1,000-line limit without claiming they're general.
4. **Logging:** recommended B1 = #2 trimmed, B2 = #3 merged with the lab's named-event snapshots,
   and close #19. #2 gives one-line board and attempt events. The lab's `bbq_lab_event` snapshots
   (route_done, aimed, photo, no_battle, off_rock) are what diagnosed every route fix this month, so
   they replace #3's two fixed capture points. #19's outcome classifier (432 lines) was never
   validated on a Switch and overlaps both.

## Dropped

Closed 2026-10-26 as superseded: #14 (duplicate of #37), #9 (always reroll impossible quests),
#28 (BP/s ordering), #34 (landing-banner start-up), #36 (routes moved off Chargestone court), #20
(checked returns and closest-fly-point instead of name reading). Their branches are kept. The egg
box stats view was added and reverted on lab (net zero) and is not carried.

## Known limitations for the docs (Z2)

- Lock-on takes the nearest Pokémon; species check (read the lock label) and target switching
  are future work.
- Rain shortens glides; a rain detector could lengthen the glide or defer the quest
  (`RETRY_LATER`).
- Photo-fly (floating Geodude) is unreliable.
- Pickup-10 is rerolled on sight: it never completed passively in 103 draws.
