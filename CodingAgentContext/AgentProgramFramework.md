# Agent-like programs: one framework for story, hunting and farming

This note names the concerns every long-running, world-navigating program in
this repo has to handle, says where each one is implemented today, and lays
out how the programs we are writing or planning fit the same shape:

- FRLG and BDSP AutoStory (fork branches `frlg/*`, `bdsp/*`)
- PLZA AutoStory (`VideoGames/PokemonLZA/Programs/AutoStory`, a scaffold)
- SwSh AutoStory (planned)
- isolated shiny hunting in SV (`VideoGames/PokemonSV/Programs/ShinyHunting`)
- the BBQ farmer (`VideoGames/PokemonSV/Programs/Farming`)

The aim is that what we now do ad hoc in each program is done the same way
everywhere, and that the game-independent parts are shared code under
`SerialPrograms/Source/Pokemon/`. A new program then *supplies* an
implementation for each concern instead of reinventing the loop around them.

Written after reviewing an external architecture note (2026-11-08) that
proposed an event bus, a global state machine and positional covariance
tracking. Section 6 says which of its ideas we take and which we do not.

## 1. The shape every agent-like program has

An agent-like program repeats one loop until a goal is met or it gives up:

```
believe   where am I, what has the story / hunt / quest list done so far
plan      the next bounded action from a table, a planner or a quest list
act       controller input, bounded in time, with its interrupts watched
observe   detectors answer named questions about the screen
decide    ok -> next action
          interrupted -> handle it, update the belief
          failed -> recover in place, else reload the last save, else stop
```

What differs between games is how each line is implemented, not the loop.
Each line is a *concern* below. A program is "agent-like" when it needs most
of them; a program that presses the same buttons in a fixed loop (a date
skipper, a box releaser) is not, and does not need this framework.

## 2. The concerns

Each concern says what it is, what the game has to supply, and where it lives
today. "Shared" means `SerialPrograms/Source/Pokemon/`; game code lives under
its `VideoGames/<Game>/` folder (`PokemonBDSP/` is not moved yet).

### 2.1 World model: how space is represented

The representation the program plans and tracks positions in.

| Space | Representation | Games | Code today |
|---|---|---|---|
| Tile grid | walkable tiles, warps, facing | FRLG, BDSP (logically) | shared `Pokemon/Navigation/`: `Pokemon_Grid.h`, `Pokemon_MapData.h`, `Pokemon_RuntimeMap.h`, `Pokemon_TilePose.h` |
| Constrained routes with a camera | route-relative heading, camera-relative stick | SwSh | `PokemonSwSh/Programs/OverworldBot/` (`OverworldTrajectory`, `OverworldMovement`) |
| Open world with landmarks | fly points, map markers, named landmarks, heading | SV, PLZA | SV: `realign_player_from_landmark`, `move_cursor_towards_flypoint_and_go_there` in `PokemonSV_AutoStoryTools.h`; PLZA: `PokemonLZA_FastTravelNavigation`, `PokemonLZA_HyperspaceNavigation` |

The shared type is `Pokemon::Position` (`Pokemon/AutoStory/Pokemon_StoryEvents.h`):
a virtual, game-defined "where the player is, in the game's own terms". The
engine carries and prints it; only the game's actions read it. A grid game
uses a tile pose; an open-world game uses a landmark plus a heading. No
program should invent a second position type for the same game.

A game supplies: its map data (grids from the decomp or Bulbapedia for 2D
games, landmark lists for 3D ones), a `Position` subtype, and the planner that
turns "go to X" into legs (`Pokemon_LegPlanner.h` for grids; for open worlds,
a hand-written leg list per route until a planner is worth it).

### 2.2 Pose belief and anchors

Where the program *believes* the player is, and how sure it is.

Shared: `Pokemon::Pose`, `PoseCertainty` (one axis pinned by a wall or door,
a set of candidates, or exact), `pose_known()` as a condition, and the rule
that only an *anchor* makes a pose certain: a wall, a door, a counter, an NPC
who answered, a fly-point landing, a town or area seam. Movement never
restores certainty; it only spends it.

The leg rule, which every program obeys:

> A leg ends at an anchor, or at a state a detector confirms. Open-ended
> movement is bounded in time, and its end position is treated as uncertain.

In grid games the anchor is usually the wall at the end of the leg
(`LegSpec` "to a wall" in `Pokemon_LegWalker.h`; `Pokemon_WallRecovery.h` and
BDSP's `AutoStory_WallRecovery` plan safe holds that converge every
hypothesis). In SV it is the Pokemon Center roof after a fly, or a minimap
landmark. In PLZA it is a fast-travel landing, a wall or a door. The external
note calls these "absorbing sinks"; same idea, and the name is fine to use in
comments.

A game supplies: which anchors exist and how to detect arriving at each, how
to re-anchor from an uncertain pose (`OverworldReanchor` in FRLG, wall
recovery in both grid games, fly-and-realign in SV), and its camera reset
where the camera matters (SV: L; PLZA: to check on the console before
assuming the same button).

### 2.3 Movement primitives

How a planned leg becomes controller input, and how long to hold.

- Grid games share `Pokemon_LegWalker.h`: `LegSpec` (direction, tiles, to a
  wall or open), `leg_hold()` from a per-game speed model, `LegInterrupt`
  detectors active during the hold, `tiles_before_interrupt()` to turn an
  interrupt's timing into a position estimate. The game supplies its
  movement policy (walk/run/bike pace, turn tap length, whether B is held)
  and its interrupt detectors.
- Open-world and route games have no shared walker yet. SV uses
  `overworld_navigation()` with a `NavigationStopCondition`; SwSh has
  `OverworldMovement`; PLZA has `BasicNavigation`. The common shape is:
  camera reset, then a stick vector held for a duration or until a stop
  condition, with the same interrupt watching as the grid walker. When PLZA
  AutoStory needs its first real leg, write it as this shape so a shared
  open-world walker can be lifted later from two users (PLZA and SwSh), not
  designed up front from one.

### 2.4 Perception: named questions, not a screen classifier

Detectors answer named yes/no questions: "overworld", "dialog open",
"battle menu", "fade", "map open". The shared type is `Pokemon::Condition`
(a named check, logged by name). Programs confirm *end states*, they do not
classify every frame. The repo rule "bounded safe presses, then confirm the
end state" follows from this: pressing B a bounded number of times until
"overworld, quiet for 5 s" holds is robust; naming every intermediate screen
is not.

Every detector a program depends on must be fakeable: FRLG's
`Overworld/PokemonFRLG_OverworldSim.h` (`SimScreen`, `SimWatcher`,
`SimTiming`) and BDSP's `StorySimulation` tests run whole checkpoints against
a fake game. A detector with no sim counterpart cannot gate a step.

A game supplies: its detectors, their detection delays (the walker's guesses
at `SimTiming`), and its sim.

### 2.5 Interrupts and the recovery ladder

What can stop an action, and what the program does about it.

Interrupts: wild battle, trainer sight, NPC in the way, a scene, a stray
dialogue. Shared: `LegInterrupt`, `HoldOutcome`, and in FRLG
`StepEnd`/`StepOutcome` (how a step ended and how far it got).

The ladder, in order, each bounded:

1. **Handle the interrupt** where it happened (fight or flee the wild
   battle, close the dialogue), then update the pose belief with what the
   interrupt's timing says.
2. **Recover in place**: re-anchor on a wall or landmark, then continue the
   plan (`SegmentHooks::recover`, `Recovery` kinds in
   `Pokemon_StoryEvents.h`; `plan_recovered_route()` in BDSP).
3. **Reload the last save** and retry the checkpoint
   (`SegmentPolicy`, attempts per checkpoint).
4. **Stop**, leaving the game where it failed, with the failure named
   (`Outcome::failure`, e.g. `probe_exhausted`) so a sim test can be written
   for it.

Hunting programs have the same ladder with "reload" replaced by "reset the
spawn" (fly away and back, picnic, area seam) and "stop" replaced by "report
and keep hunting": SwSh `EncounterHandler`, SV `LetsGoEncounterBotTracker`.

A game supplies: its interrupt detectors, its handle-and-flee macro per
interrupt, its in-place recoveries, and how to save and reload.

### 2.6 Plan structure and progress

How the goal is broken into bounded units, and what marks progress.

| Program kind | Unit | Progress marker | Code today |
|---|---|---|---|
| Story | checkpoint = a row of steps (`StoryStep`, `StoryStepKind`: route, legs, talk, battle, shop, heal, save, ...) | the save at a checkpoint's end | shared `Pokemon_StorySteps.h`, `Pokemon_StorySegments.h` (`Segment`, `run_segment`, verify mode), `Pokemon_StoryStepJson.h`; FRLG `AutoStoryTable`, BDSP `StoryTable` |
| Hunting | one patrol leg + one verification | encounters per unit time; a reset when the rate drops | SV `EncounterRateTracker`, `DiscontiguousTimeTracker` in `PokemonSV_LetsGoTools.h` |
| Farming (BBQ) | one quest, read from the list | quests done, BP, the ROI ledger | `read_quests()`, `process_quest_list()`, `process_and_do_quest()` in `PokemonSV_BlueberryQuests.h` |

Every unit returns an `Outcome` (success, or a named failure) and knows
whether it changed the pose. Every program has a stats tracker with its
progress counters (`Pokemon_AutoStoryStats.h`; `LetsGoEncounterBotStats`).

A game supplies: its table or loop body, and its `StoryStepExecutor` (the
shared interface with one virtual per step kind; kinds a game does not
implement fail as unsupported, loudly).

### 2.7 Decisions that need data

Battles, shopping, teaching moves, which quest to take: anything that needs
game data and a model, not just a detector.

- FRLG has the fullest version: `BattleData/*.json`, `AutoStoryBattleSim`,
  `AutoStoryWinChance`, `AutoStoryBattleDecisions`, and the rule that a
  one-shot spend (TM, tutor) is shown to the user with sim numbers first.
- BDSP has `AutoStory_Battle` and `AutoStory_BattleMoves`.
- PLZA has `BattleAIOption`; SV AutoStory has no battle model and relies on a
  fixed starter move set (`swap_starter_moves`, `confirm_lead_pokemon_moves`).
- BBQ orders quests by the ROI ledger.

The shared part is the *gate*, not the model: a battle checkpoint is run
through the sim before it is run on the console, and a battle that has lost
twice gets its own checkpoint saved right before it and is verified over
three runs (`VerifyPolicy`). Sharing the battle data schema between FRLG and
BDSP is the next step (section 4).

### 2.8 Testing seams

- Every console failure becomes a red sim test before the fix (the repo's
  "failure becomes sim test" rule). This needs 2.4's sim and 2.6's `Outcome`
  names.
- Grid and route tests gate map changes without the console
  (`Pokemon_Navigation_Tests.h`, BDSP `*_Maps_Tests`, `*_PlannedRoute_Tests`).
- A story table is run end to end against the fake game
  (`*_StorySimulation_Tests`).
- A program that cannot run headless against its sim is not ready for
  overnight runs.

## 3. The contract a new agent-like program fills in

Before writing the first checkpoint or hunt loop, answer each row. The
answer is either "shared, as is", "shared, with this game's policy", or
"game-specific, in this file".

| Concern | Shared seam | The game supplies |
|---|---|---|
| World model | `Pokemon::Position`, grids in `Pokemon/Navigation` | map data, `Position` subtype, planner or leg lists |
| Pose and anchors | `Pose`, `PoseCertainty`, `pose_known()` | anchor list, arrival detectors, re-anchor action, camera reset |
| Movement | `LegSpec`/`LegWalker` (grid); open-world walker to be lifted | speed model, interrupts, stick/D-pad policy |
| Perception | `Condition`, the sim pattern | detectors, delays, sim |
| Interrupts and recovery | `LegInterrupt`, `Recovery`, `SegmentHooks` | per-interrupt macro, in-place recoveries, save/reload |
| Plan and progress | `StoryStep`/`Segment`/`run_segment`; hunt and farm loops per program | table or loop body, `StoryStepExecutor`, stats |
| Decisions | the sim gate and verify mode | battle data, decider, one-shot-spend review |
| Tests | `UnitTest`, sim tests | red test per failure, table simulation |

If a row has no answer, the program is not ready to grow; writing the row
first is cheaper than a console run that fails on it.

## 4. Aligning FRLG and BDSP

Both already sit on `Pokemon/AutoStory` and `Pokemon/Navigation`: same
`StoryStep` kinds, same `Segment` runner, same verify mode, same JSON step
format, same `StoryChecks` level, same leg walker and wall recovery. The
remaining differences are the alignment work, in order of value:

1. **One overworld sim.** FRLG's `OverworldSim` (screens, watchers, timing,
   encounters on grass steps) and BDSP's `StorySimulation` fake the same
   things with different code. Lift the game-independent part (the player
   on a `RuntimeMap`, legs, interrupts, timing) into
   `Pokemon/Navigation`, keyed by each game's movement policy; keep the
   screens each game shows as its own layer. This makes "failure becomes
   sim test" the same exercise in both games and lets a nav fix be tested
   against both grids.
2. **One step-outcome vocabulary.** FRLG's `StepEnd`/`StepOutcome` and
   BDSP's `plan_recovered_route()` input (`interrupted_leg_positions()`)
   describe the same "how far did the interrupted leg get" belief. Agree on
   one type in the shared layer so recovery planning is written once.
3. **Executors of the same shape.** `talk`, `mash`, `heal`, `shop`, `save`
   differ per game in menus and detectors, not in logic. Compare the two
   `StoryStepExecutor`s kind by kind; where only the detectors differ, move
   the logic up and leave a per-game detector set.
4. **Battle data schema.** FRLG's `BattleData/*.json` and sim are the
   template; BDSP's `BattleMoves` should read the same shape of species,
   moves and trainers so one decider and one sim gate serve both. This is a
   data-and-interface change and needs a plan with sim numbers before
   coding, per the TM-spend rule.
5. **Same run tooling.** Both use the lab Switch, a save-slot rule and
   checkpoint indices; keep the run scripts and the handoff format
   identical so an agent can move between the two without relearning.

Do not align by moving code into a PR that owns neither game: shared nav
changes go through the PR that owns the shared files, with unit tests from
both games.

## 5. The programs ahead

### 5.1 PLZA AutoStory

The scaffold (`PokemonLZA_AutoStoryTools.h`) copies SV's older
`AutoStory_Segment`/`AutoStory_Checkpoint` classes and
`checkpoint_reattempt_loop`. That is the pre-shared-layer design. Before
Segment 01 grows past its first checkpoint:

- Decide the world model: landmarks are fast-travel points and named places;
  `Position` = place + heading. Lumiose is continuous, so there is no grid
  and the grid planner does not apply.
- Write the leg rule into the first real leg: camera reset, stick hold with
  a stop condition, end at a wall, door or fast-travel landing, confirm by
  detector. Reuse `BasicNavigation`, `FastTravelNavigation`,
  `HyperspaceNavigation`, `TrainerBattle` and `MenuNavigation` from
  `PokemonLZA/Programs/` before writing new movement code.
- Move to the shared `StoryStepExecutor` + `Segment` once those files are on
  `main`; until then keep the step kinds and `Outcome` names identical so
  the switch is mechanical. Do not add a third checkpoint/segment interface.
- Sim first: a PLZA fake game that answers the detectors the first segment
  uses, and a table simulation test, before the second checkpoint.

### 5.2 SwSh AutoStory

Routes are constrained with a following camera, so the world model is
route-relative: `Position` = route + a point along it + which side. Anchors
are gates, doors, fence ends and the Flying Taxi. `OverworldBot` already has
camera-relative movement and target tracking; `EncounterHandler` is the
wild-battle interrupt handler; `MenuNavigation` and `GameEntry` cover
menus and resets. The new work is the checkpoint table, the detectors for
dialogue and battle in story context, and a route sim. Build on the shared
story layer from the start; this is the second open-world user and the one
that should let the open-world walker be lifted.

### 5.3 Isolated shiny hunting in SV

A hunt is the loop of section 1 with a fixed goal: patrol an anchored leg,
verify spawns (shiny sound detector, Let's Go kill chain), handle the
encounter, reset when the encounter rate falls. `AreaZeroPlatform` and
`Scatterbug` are two instances; `LetsGoTools` holds the shared tracking.

Pick patrols by how they stay anchored, cheapest first:

1. area or town seam oscillation (two points, the seam is the anchor);
2. wall-sliding shuttle (a wall bias keeps the line, a bump at each end is
   the anchor);
3. stationary resets (picnic; a bump into a fixed object every N cycles);
4. open-field shuttle with a periodic fly reset;
5. recorded loops over complex terrain (needs a route recording and the
   most recovery).

A new hunt should be a patrol spec plus a reset condition plugged into one
hunt runner, not a new program skeleton. The runner is the piece to write
when the third hunt is added; until then, keep the two existing programs'
loops shaped the same.

### 5.4 BBQ farmer

Already agent-shaped: observe (read the quest list), decide (filter and
order), act (one handler per quest), observe again. Map it onto the
concerns so it stops being a special case: each quest handler returns an
`Outcome`; "fly to the Central Plaza" is the re-anchor; the ROI ledger is
the decision data; the quest count and BP are the progress markers.
Failures get named and get sim tests like story steps do. Nothing here needs
the shared story types; it needs the same discipline.

## 6. What we take from the external note, and what we do not

Take, under our names:

- absorbing sinks = anchors (2.2); the leg rule is the practical form;
- the uncertainty idea = `PoseCertainty`; a belief, not a number;
- the checkpoint graph = our tables; a line of checkpoints on one save slot
  is all the graph we need;
- the topology table (grid, constrained route, open world) = 2.1;
- the patrol taxonomy for hunts = 5.3.

Do not take:

- **A global event bus.** `run_until()`, detectors as `LegInterrupt`s and
  `CancellableScope` already give bounded preemption. A pub/sub broker is new
  infra under `Common/` with a large blast radius and no second user; its
  sample code also used `std::mutex` (banned here) and called subscribers
  under the lock.
- **A global state machine naming every screen.** It inverts 2.4: we confirm
  end states, we do not classify frames.
- **Positional covariance numbers.** The thresholds would be invented.
  `PoseCertainty` says what we actually know (an axis, a set, exact).
- **A `Framework/` interface layer in infra** (`IGameWorld`, `INavigator`,
  `IOracle`). Shared program code lives under `Pokemon/` and grows by being
  lifted from two concrete users. Infra stays Qt-free, small and reviewed.
- **A level-100 carry as the battle plan.** It does not exist from a new
  game in FRLG or BDSP. We use our own party plus the sim gate.

## 7. Working rules this framework assumes

These are the repo's existing practices, collected because the framework
depends on them:

- A leg ends at an anchor or a detector-confirmed state (2.2).
- Bounded safe presses, then confirm the end state (2.4).
- Every console failure becomes a red sim test before the fix (2.8).
- A battle lost twice gets its own checkpoint and three verify runs (2.7).
- One-shot spends (TMs, tutors) go to the user with sim numbers (2.7).
- Measure one routine several times per build before stacking changes.
- Shared nav and story files change only in the PR that owns them, with
  tests from every game that uses them (4).
