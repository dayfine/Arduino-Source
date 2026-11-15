# Sword/Shield AutoStory work plan

Written 2026-11-14 as the implementation plan for the first free-moving game on the shared
AutoStory engine. The architecture names a Sword/Shield run from a new game to Wedgehurst station;
this document turns that target into actions, detectors, routines, investigations and reviewable
slices.

Status: **plan, not implementation**. Existing code is evidence to reuse or characterize, not proof
that the opening story works unattended. In particular, upstream
[PokemonAutomation/Arduino-Source#1207](https://github.com/PokemonAutomation/Arduino-Source/pull/1207)
is a seven-segment timed prototype to the first Wild Area. It supplies the story order and rough
route measurements, but it does not supply the checkpoint/retry contract or verified 3D navigation.

## 1. Scope and gates

Split the work where the risks change.

### Gate A: the architecture target

Run from a profile with no Sword/Shield save to **standing outside Wedgehurst station before talking
to Leon**. This is the proposed endpoint because it is an overworld state that should be observable
and saveable. The first station conversation immediately carries the story toward Hop's house and
the starter choice, so ending after that conversation may not provide a stable checkpoint.

Gate A includes:

- fresh-save launch, language, appearance and player name;
- the Rose introduction and first return of player control;
- verified story options;
- getting the bag and leaving the player's house;
- Hop's house and its scene;
- the Postwick-to-Wedgehurst walk and station approach;
- manual save, reload and bounded checkpoint retries.

Gate A deliberately excludes battles, catches, party management and the Wild Area's free camera.
It isolates the new problem: closed-loop movement in a free-moving game.

### Gate B: the prototype's full target

Continue from Gate A through the starter, Slumbering Weald, Wedgehurst Lab, Route 2, Magnolia's
house, the second station visit and the train to the first Wild Area. This covers the full scope of
#1207 after Gate A proves the navigation and screen contracts.

The first Wild Area is a separate feasibility gate. Early towns and routes may keep the camera
behind the player; the Wild Area permits more camera control and must not inherit that assumption
without console evidence.

### Not in either gate

- a complete Galar map or a general visual SLAM system;
- the story after the first Wild Area;
- automatic save deletion (a destructive operation); the program requires an empty profile and
  stops if it finds an existing save;
- claims of every language, both Switch generations or both versions before each has fixtures;
- an offline model of 3D walking that has not been measured on the game.

## 2. What exists

### 2.1 Shared AutoStory stack on the fork

The game-neutral pieces live on the fork branch `nav/shared`, the trunk for AutoStory work: it
carries `Pokemon/AutoStory/Engine/`, `CommonTools/ScreenEvents/`, `CommonTools/ScreenMachine/`, the
Switch keyboard driver, the failure classes and the story launch. They are prerequisites, not files
to copy into Sword/Shield, and none of them is on upstream `main`. **Every SwSh slice from S1 on
bases on `nav/shared`** (or on a SwSh parent that does) and names that parent in its banner.

| Piece | Where on `nav/shared` | Sword/Shield use |
|---|---|---|
| Action registry, built-in actions, `StoryGame` hooks, JSON checkpoints, lint | `Pokemon/AutoStory/Engine/` (contract in `Engine/MIGRATION.md`) | Register SwSh handlers under the engine's kinds and names (section 3) |
| Checkpoint save modes, retries, logs and postconditions | `Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h` | Reload the last verified save after a failed checkpoint |
| Free-moving input and facing | `StoryBackend::stick()`, the `"heading"` reading (`STORY_HEADING`) | Turn and walk in bounded slices; report facing if the game shows it |
| Named-area pose | `Pokemon::ZonePose`, a step's `declared_after` (`{"place": ...}`) | Declare `PostwickHome`, `Postwick`, `HopHouse` and `Wedgehurst` per step; the runner writes the declared place into the run pose, so no area reader is needed before one exists (section 3) |
| Scripted backend | `FakeStoryBackend<Pose>` | Run the checkpoint plans offline, including `console_only` code with a CONSOLE script |
| Shared screen events and scene loops | `CommonTools/ScreenEvents/`, `run_screen_machine_on_observations()` in `CommonTools/ScreenMachine/` | Interpret SwSh detectors as dialog, choice, fade, battle and player-control events |
| Launch from HOME to the overworld on a save | `Engine/Pokemon_StoryLaunch.*` (fork #477) | Reload and resume. It never starts a new game: today a `policy.new_game` screen ends it as `launch_failed`, so the fresh start needs the typed no-save end of section 3 before `new_game` can run after it |
| Failure classes | `Pokemon/AutoStory/Pokemon_StoryFailureClasses.*` (fork #479) | Name each S9 fault and the retry it implies |
| Shared story files and dev-folder override | `Pokemon/AutoStory/Data/Pokemon_StoryFiles.h` (fork #474) | Load `StoryData/` checkpoints and override them from a dev folder |
| Breakpoints before each step and leg | fork #448 | Pause before a walk leg while measuring |
| Feedback-driven Switch system keyboard | `NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.*`, `NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.*` (fork #62) | Type the player name after a SwSh fixture verifies the same keyboard layout |

Frame replay (fork #306) and failure capture (fork #308) are **not** on `nav/shared`: both are open
on `nav/grid-story-steps`. S9 depends on them merging (section 10).

No game runs on the engine yet. `Engine/MIGRATION.md` moves FRLG, BDSP and PLZA onto it in that
order; FRLG and BDSP still use their own executors. The PLZA adapter is the closest structural
precedent and is still open: fork #354 (base #416, which bases on #448 in `nav/shared`). It shows how
a game without tiles plugs in: a console backend, a screen-event adapter, a scene machine, a
`StoryGame` and JSON checkpoints. Its walks are **not** visually steered. `PokemonLZA_AutoStoryMoves.h`
uses recorded turns and timed walks, the compass and fast travel: walks are `TIMED_LEGS` args, turns
are `stick()` after the camera recenter, and the minimap arrow is the `"heading"` reading. Its only
visual arrival check is `PlatformArrivalDetector` (`wait_for_platform`, `walk_to_prompt`). Its
detectors, timings and route actions remain PLZA-specific and are not copied into SwSh.

### 2.2 Sword/Shield code on upstream main

| Existing code | Reuse status | Work before story use |
|---|---|---|
| `BlackDialogBoxDetector`, `WhiteDialogBoxDetector` (`PokemonSwSh_DialogBoxDetector.h`), `DialogTriangleDetector` | candidate reuse | Test opening dialog variants and quiet frames |
| `MainMenuDetector`, `PartyMenuDetector`, `BoxMenuDetector` | candidate reuse | Add opening-story and first-save fixtures |
| `SelectionArrowDetector`, `RotomPhoneMenuArrowFinder` | candidate reuse | Verify the language/appearance/options/choice layouts separately |
| `BlackScreenOverWatcher`, `WhiteScreenOverWatcher` (`CommonTools/VisualDetectors/BlackScreenDetector.h`) | reuse | Test each opening door and scene transition |
| `save_game()`, `menus_to_mainmenu()` | strong reuse candidate | Wrap as an `Outcome`; verify first-save prompts and return to control |
| `PokemonSwSh_GameEntry` start/reset routines | partial reuse | Existing-save entry is implemented; fresh-save/title handling is not |
| `StandardBattleMenuDetector`, `StartBattleWatcher`, battle-dialog and EXP detectors | Gate B candidate | Register story-specific image cases before building a trainer loop |
| `basic_catcher()` and `ReceivePokemonDetector` | Gate B candidate | Wrap all results; cover nickname, full party and post-catch return |
| party/box detectors and `menus_to_boxsystem()` | Gate B candidate | Add verified move-order and deposit actions instead of fixed cursor scripts |
| `YCommIconDetector` | later reuse | It cannot prove the early overworld before Y-Comm is unlocked |
| `OverworldTargetTracker`, `OverworldTrajectory` (`Programs/OverworldBot/`), `MarkFinder` | limited evidence | It follows exclamation/question marks, not doors or route landmarks; its steering maths may inform the pilot |

Paths are under `SerialPrograms/Source/VideoGames/PokemonSwSh/` on `main`. "Candidate" is
intentional: a detector being used by another program does not establish its false positive/negative
behavior on the opening story.

### 2.3 What must be invented

The new work is bounded to the game adapter and the evidence it needs:

- a SwSh screen observer, console backend, action registry and story table on the shared engine;
- one shared-engine change: a typed successful no-save end for `launch_story_game()`, so a fresh
  profile crosses from HOME to `NEW_GAME` without reading `launch_failed` as success (section 3);
- fresh-save, language, appearance and options detection;
- an early player-control cue that works before Y-Comm;
- walk actions in PLZA's shape (recorded turns, timed legs, a heading reading), plus a visual
  endpoint reader only where R0 shows a timed leg cannot end reliably;
- named opening poses declared per step, and endpoint readers only where the screen identifies a
  place;
- Gate B only: a story battle loop and complete catch/party outcomes around the existing primitives.

The work does not invent another checkpoint engine, JSON contract, retry loop, launch, keyboard
driver or generic tile planner.

### 2.4 What #1207 contributes

The draft records a useful nominal route:

| Prototype segment | Story content | Use in this plan |
|---|---|---|
| 00 | intro, options, bag | screen and route capture checklist |
| first part of 01 | player's house, Hop's house, first station visit | Gate A route worksheet |
| rest of 01 | starter and Hop battle | Gate B starter/battle worksheet |
| 02 | move reorder, Slumbering Weald, mandatory fights, legendary | Gate B story order and nominal headings |
| 03 | home, Wedgehurst Lab, early catches | Gate B route/catch worksheet |
| 04 | Center, Route 2, Magnolia, Hop battle | Gate B checkpoint candidates |
| 05 | return to station and train | Gate B route worksheet |
| 06 | party/box setup and Wild Area crossing | separate free-camera feasibility evidence |

Do not port its long mashes, 15-30 second stick holds, hard-coded menu walks or `save_from_overworld()`
unchanged. Each becomes evidence for a bounded action with an observable end state.

## 3. Actions required

The engine's adapter contract (`Engine/MIGRATION.md` on `nav/shared`) decides where each piece
goes. Built-in actions (`mash`; `talk`, which fails as `talk_no_text` when A opens no text; `answer`
with its `yes` arg; `fight`; `save`) run on the game's screen events; a game replaces one only when
it needs the game's map or menus, and **a replacement uses the built-in's kind and name**. `legs`
and `face` exist only for tile poses, so a SwSh step naming them fails the lint, and the built-in
`route` takes a `GRID_POSE` target, so a `ZonePose` game walks with its own `GAME` steps as PLZA
does. Hook kinds (`new_game`, `options`, `ready`, `heal`, `shop`, `train`) are registered by the game
under the same rule. Anything else is a `GAME` step.
An action that works only through console screens (menus, the keyboard, a battle AI) sets
`spec.console_only`: the engine refuses it off a console and never reloads for it, so scripted
tests run it with a CONSOLE `FakeStoryBackend` script.

Every action states its start screen, end screen, safe inputs, timeout and failure result.

| Engine kind / name | Gate | SwSh handler | console_only | Contract |
|---|---:|---|:---:|---|
| `NEW_GAME` / `new_game` (hook) | A | no-save title, language, appearance, keyboard name, Rose introduction | yes | Starts where #477's launch ended at its typed no-save state (below); rejects an existing save without input; ends at the observed control cue, with `PostwickHome` declared |
| `OPTIONS` / `options` (hook) | A | Rotom Phone options menu | yes | Read each supported row/value, change only mismatches, return to control |
| `MASH` / `mash` (built-in) | A | none: SwSh supplies dialog, choice, fade and control events from `observe()` | no | Ends on the SwSh control cue; replace under the same kind and name only if R0 shows the built-in cannot |
| `TALK` / `talk` (built-in) | A | none | no | A at what the player faces; fails as `talk_no_text` when no text opens |
| `ANSWER` / `answer` with `yes` (built-in) | A | none for yes/no | no | A choice with more than two rows becomes a `GAME` step only if R0 finds one on the route |
| `GAME` / `walk` | A | as PLZA's `walk`: `TIMED_LEGS` legs, the heading it faces first, the event it ends on (section 6) | no | Bounded legs; ends on its endpoint event or returns `lost`/timed out |
| `GAME` / `walk_to_prompt` | A | as PLZA's: straight on in short `stick()` slices until an A prompt (or a fade or dialog) shows | no | Ends on the requested event or its bound |
| `BATTLE` / `fight` (built-in) | B | `SwShConsoleBackend::battle()` over the existing battle detectors | battle AI: yes | Bounded move choice through victory/loss and return to the requested screen |
| `GAME` / `choose_starter` | B | choice framework | yes | Identify or reliably approach the configured starter and verify receipt |
| `GAME` / `catch_story_pokemon` | B | `basic_catcher()` | yes | Report caught/fainted/own-fainted/out-of-balls/unsupported follow-up exactly |
| `GAME` / `reorder_move`, `deposit_party` | B | party/box readers | yes | Observe cursor and result rather than replay a fixed menu script |

`StoryGame` hooks are not actions and are not registered:

| Hook | SwSh handler |
|---|---|
| `save(backend, checkpoint)` | wraps `save_game()`; verifies first-save completion and returned control. The engine owns the SAVE step, so SwSh never registers `save` |
| `reload` | close the game and run #477's launch to the overworld on the save; verify the expected area and control cue before a retry |
| `begin_attempt` | clear per-attempt state; check the preconditions of section 9 item 11 |
| `interrupts` | Gate B: wild and trainer battles during a walk |

**Fresh start.** `launch_story_game()` (`Engine/Pokemon_StoryLaunch.cpp`) fails as
`STORY_LAUNCH_FAILED` ("launch_failed") when `policy.new_game` matches, without a press, and nothing
in the story answers that failure. So today the launch cannot run before `new_game`, and the plan
does not reinterpret `launch_failed` as the fresh-profile branch. Gate A needs one shared-engine PR
on `nav/shared` before S3: a typed successful terminal state for the no-save screen, distinct from
the overworld success and from `launch_failed`, that a request opts into (a reload never does), still
with no press on that screen. The program then dispatches `NEW_GAME` from it. Until that PR merges,
S3 can only be tested from a console already at the no-save screen, which is not the Gate A launch
target.

Readers (`StoryGameReader`), each saying what it `supplies`:

- no generic `"pose"` reader at first. The early control cue proves that the player can move; it
  does not tell `PostwickHome`, `HopHouse`, `Postwick` and `Wedgehurst` apart. Each step carries
  its area in `declared_after`: when an action succeeds without a pose of its own, the runner
  writes the declared place into the run (`Pokemon_StoryStepRunner.cpp`), and a checkpoint end pose
  that no reader supplies is `NOT_CHECKED`, not failed. A reader that supplies `"pose"` is an
  explicit endpoint reader for one place the screen identifies, and sets only that place; a
  generic area classifier comes only after R0 demonstrates one;
- `"heading"` (`STORY_HEADING`, read with `story_heading()`): the player's facing, if SwSh shows a
  minimap or compass that R0 confirms;
- the Gate A station-approach reader, opted in with `StoryReaderUse::AFTER_CHECKPOINT` (cheap and
  safe on any end screen): it supplies `"pose"` as `Wedgehurst` only when the station approach is
  on screen, so 00.03's end pose is checked and the earlier checkpoints' are declared. It replaces
  the earlier `verify_station_approach` action.

Checkpoints are JSON in `StoryData/` with `metadata.phase`. A unit test loads every checkpoint, lints
it with `StoryActionRegistry::lint` and checks `verified.steps_hash` for verification and
reproduction checkpoints, as MIGRATION.md step 3 requires.

## 4. Detectors and readers required

### 4.1 Characterize before inventing

Record positive and near-negative frames for the existing dialog, triangle, menu, arrow, fade,
battle and receive-Pokemon detectors. A story test set includes:

- text still typing versus an advance triangle;
- a cutscene subtitle versus a dialog box;
- quiet cutscene frames versus player control;
- menu transitions and first-use tutorial cards;
- door fades, battle fades and cinematic black frames;
- the same screen at the supported capture resolutions.

If an existing detector is correct, register the fixtures and use it. If it needs a crop or threshold
change, change it in its owning detector PR with non-story regression cases too.

### 4.2 New opening detectors

| Detector/reader | Why it is needed | Minimum evidence |
|---|---|---|
| no-save title / opening screen | preflight and fresh-start launch | existing-save and no-save profiles |
| language screen | choose the configured game language | every initially supported language row |
| appearance screen | start the name flow without blind cursor counts | every supported appearance |
| early player-control cue | scene and walk postconditions before Y-Comm | home, outdoors, Hop's house and Wedgehurst |
| opening choice box | stop before selecting an unsupported choice | positive choices and ordinary dialogs |
| options menu and row/value reader | idempotent settings | every row changed by the program, both values |
| interaction prompt, if stable | end a walk before A | bag, doors, NPCs and negative overworld frames |
| minimap/compass heading, if shown | the `"heading"` reading for turns | each Gate A map, several facings |
| named landmarks, only where R0 needs them | a visual endpoint for a timed leg | approach sequence, occlusion, wrong-facing and other-hour negatives |
| station approach | Gate A postcondition | several arrival offsets and nearby non-station views |

The early control cue is the first detector risk. "No dialog" is not sufficient: cutscenes and
loading frames also lack a dialog. PLZA's `ControlsHintDetector` and `mash_to_move_hints` (on
`plza/story-base`, fork #354) are the pattern: a stable on-screen controls hint that appears only
with control. Look for the SwSh equivalent first; otherwise combine several weak cues and require
them to hold.

### 4.3 Gate B detectors

Gate B adds starter selection/receipt, story-battle victory and loss, learn-move/evolution prompts,
nickname and party-full choices, and a Wild Area/free-camera state. Existing battle and catch
detectors are inputs, not complete outcomes.

## 5. Game routines and layout

The implementation follows the shape of PLZA's adapter (open fork #354, `plza/story-base`). SwSh
lives at `SerialPrograms/Source/VideoGames/PokemonSwSh/` on `main`; #1207's
`PokemonSwSh/Programs/AutoStory/` is the path from before that move.

```text
SerialPrograms/Source/VideoGames/PokemonSwSh/Programs/AutoStory/
  PokemonSwSh_AutoStory.{h,cpp}              program and options
  PokemonSwSh_AutoStoryBackend.{h,cpp}       controller, video, observations, battle call
  PokemonSwSh_AutoStoryGame.{h,cpp}          action handlers, readers, save/reload, interrupts
  PokemonSwSh_AutoStoryReadings.h            reading names shared by backend, readers and tests
  PokemonSwSh_AutoStoryScreenEvents.{h,cpp}  detector-to-event adapter
  PokemonSwSh_AutoStorySceneMachine.{h,cpp}  scene states over the shared observation runner
  PokemonSwSh_AutoStoryStepValidator.{h,cpp} SwSh-specific step checks beside the engine lint
  PokemonSwSh_AutoStoryTable.{h,cpp}         segments and checkpoint list
  PokemonSwSh_AutoStoryStoryFiles.{h,cpp}    StoryData/ loading over #474's shared story files
  Actions/
    PokemonSwSh_AutoStoryNewGame.*
    PokemonSwSh_AutoStoryScenes.*
    PokemonSwSh_AutoStoryOptions.*
    PokemonSwSh_AutoStoryMoves.*
    PokemonSwSh_AutoStoryBattles.*           Gate B
  Detectors/
    PokemonSwSh_AutoStoryNewGame.*
    PokemonSwSh_AutoStoryControlsHint.*
    PokemonSwSh_AutoStoryOptions.*
    PokemonSwSh_AutoStoryLandmarks.*         only if section 6 needs them
  StoryData/
    swsh-00.xx.plan.json
```

The important routines are:

1. `SwShScreenObserver`: translates the game's detectors into shared events such as dialog, choice,
   fade, battle and player control, plus game-specific readings.
2. `SwShConsoleBackend`: implements press, stick, observe, battle, log and cancellation; it does not
   decide story order.
3. `make_swsh_story_game()`: registers actions/readers and owns save, reload and interrupt hooks.
4. SwSh scene machine: uses the shared observation runner; every recognized state takes one safe
   step, and quiet/unknown screens have bounds.
5. Walk actions: recorded turns and timed legs with a heading check, ending on an endpoint event;
   each reports arrived, lost, interrupted or timed out.
6. Scripted backend cases: run the exact checkpoint plans through success, dropped input, detector
   miss, retry and exhaustion without a console.

## 6. The 3D navigation pilot

Do not make a global navigation abstraction before one SwSh route works.

**Walk shape.** SwSh walks are expected to follow PLZA's model: recorded turns, timed legs and a
minimap/compass heading, written as `TIMED_LEGS` args to a `GAME` `walk` step. The final arg shape
is decided after R0 play and a movement-mechanics study (section 9 item 5). If SwSh walks need
anything PLZA lacks, such as a visual endpoint inside a leg, that is a change to `StoryStepJson` and
the lint, and it lands as a shared-engine PR on `nav/shared` before S5.

A walk leg:

1. Turns with `stick()` and checks the `"heading"` reading where the game shows one.
2. Walks a timed slice, releases the stick and observes again.
3. Ends on a fade, dialog, interaction prompt or explicit endpoint reader.
4. Returns `lost` when its endpoint has not appeared within the leg's bound; the checkpoint reloads
   rather than continuing blind.

A timed leg is acceptable only between two observed anchors, marked as assumed in the route log and
bounded by the endpoint detector. A long hold from #1207 is not an anchor.

**Landmark steering has no precedent on the engine.** PLZA's only visual arrival is
`PlatformArrivalDetector`. The project's one landmark loop is upstream #1717's BDSP `match_landmarks`
(`docs/design/AutoStory.md` §4.3): `TM_CCOEFF_NORMED` per landmark, a margin over the best
competitor, several landmarks that must agree, a fine then coarse pass, three consecutive hits,
one-axis nudges and lost/give-up bounds. If R0 shows a SwSh leg needs a visual endpoint, adopt that
loop's shape, including its failure modes. In r28 (`AutoStory-Lessons.md`) its landmark scores fell
to 0.41 and 0.60 after the console clock drifted 3.5 h from the reference captures, and pinning the
Switch clock to avoid that broke runs on its own (r108; `AutoStory-Navigation-Stack.md`, "Drop the
clock pin"). Galar's towns and routes do **not** follow the Switch clock before the player is
Champion: their time of day is set by story progress, and only the Wild Area runs on the system
clock ([Bulbapedia, Time](https://bulbapedia.bulbagarden.net/wiki/Time)). Gate A lighting is
therefore expected to be fixed per story position, which R0 confirms rather than assumes (section 9
item 7); the real-time lighting matrix belongs to the Wild Area feasibility slice (section 8
item 7).

Use named poses initially:

```text
PostwickHome / bag
PostwickHome / exit
Postwick / player_house
HopHouse / interior
Postwick / road_to_wedgehurst
Wedgehurst / station_approach
```

Each step declares its area in `declared_after`, while action-specific readings carry the heading
and endpoint; only `Wedgehurst / station_approach` has a reader that checks it (section 3). Define a
new exact pose type only after a position reader has demonstrated coordinates or another stable
representation.

## 7. Gate A checkpoints

The evidence round may move a boundary when the game does not permit a save there. Each proposed
checkpoint still has one saved start and a separately observed end.

| Checkpoint | Start | Steps | End/postcondition |
|---|---|---|---|
| 00.00 Fresh start | recognized no-save launch | language, appearance, name, Rose introduction | player control in `PostwickHome` |
| 00.01 Bag | saved/control in home | options, walk to bag, interact, clear scene | bag obtained, control restored in home |
| 00.02 Hop's house | saved/control in home | leave through fade, timed walk, enter Hop's house, clear scene | control at a recorded Hop-house anchor |
| 00.03 Station approach | saved/control after Hop's scene | leave, timed walk along the road, stop before Leon | `Wedgehurst/station_approach`, no station scene started |

The areas in the End column are declared per step (`declared_after`), not read; the control cue is
what each checkpoint observes. Only `Wedgehurst/station_approach` has a reader that checks the
end pose (section 3).

If 00.00 cannot save before its end, it remains one fresh-start checkpoint and later checkpoints
start only after the first verified save. Do not pretend a scene boundary is reloadable.

## 8. Gate B slices

Gate B is a sequence of vertical story slices, not one port of `segment_01` through `segment_06`:

1. **Starter and Hop battle.** Starter approach/receipt, first trainer battle, victory/loss and save.
2. **Slumbering Weald.** Route anchors, three mandatory wild battles and the legendary scene;
   Sword/Shield visual variants are explicit fixtures.
3. **Wedgehurst Lab.** Home handoff, Lab approach, Pokédex/catch tutorial and Center return.
4. **Route 2.** Required catches or fights, trainer interrupts, Magnolia's house and a saved end.
5. **Second Hop battle.** Battle policy, Dynamax Band receipt and return route.
6. **Station and train.** Camping gear, station scene, train transition and verified Wild Area arrival.
7. **Wild Area feasibility.** Camera model, first free-camera walk and recovery, and the real-time
   lighting matrix: the Wild Area is the first area on the Switch clock, so its cues are captured
   and tested across the hours here, not in Gate A. Stop expansion if no bounded
   localization/arrival method is demonstrated.

Party deposits and a deliberate faint/teleport from #1207's segment 06 are route-policy choices,
not infrastructure assumptions. Gate B should first prove why they are required and what state they
consume or change.

## 9. Investigation backlog

### Blocking Gate A

1. Capture the fresh-save flow: title, language, appearance, keyboard, intro, first control, options,
   first manual save, bag, doors, Hop's house, road and station approach.
2. Settle the endpoint: confirm that the proposed pre-Leon station approach is observable and
   saveable; otherwise choose the nearest stable state and update the architecture wording.
3. Find the early player-control cue before Y-Comm.
4. Verify first-save, overwrite, autosave and reload behavior. Determine when autosave can be disabled
   and what the opening reload returns to.
5. Movement-mechanics study on each Gate A map: turn rate, displacement per held time, collisions,
   whether the camera recenters, and whether a minimap or compass gives the heading. It decides the
   walk arg shape (section 6).
6. Identify an endpoint event for every walk, and a stable landmark only where no event exists;
   collect wrong-facing, occluded and nearby negative frames.
7. Characterize Gate A lighting before S5. Before the player is Champion, towns and routes use
   story-determined time of day and only the Wild Area follows the Switch clock (Bulbapedia, Time),
   so Gate A lighting should be fixed per story position. R0 confirms it: capture the same Gate A
   screens from the same save at two or three Switch clock hours. If a cue does change with the
   clock, choose cues that hold across the hours or per-hour references as `match_lighting_pair`
   did, and record the hours each was tested; #1717's landmarks failed across hours (r28) and
   pinning the clock broke runs (r108). The real-time matrix itself is item 15.

### Support decisions

8. Start with one declared matrix: English game, QWERTY Switch 1 system keyboard, one version,
   16:9 and the normal capture resolution. Add a matrix row only with fixtures and a live pass.
9. Check Sword versus Shield differences before the station, then the legendary scene separately.
10. Check whether weather, NPC motion or display brightness changes a Gate A cue.
11. Decide which settings are required for reliable execution. Record their values as a precondition
    and verify them rather than replaying #1207's cursor counts.

### Gate B research

12. Characterize each existing battle detector on the starter/Hop and Weald battle screens.
13. Determine the minimal battle policy per starter, possible losses and any move-learn/evolution
    prompts before the Wild Area.
14. Exercise `basic_catcher()` on the tutorial and ordinary early catches, including party-full and
    nickname paths.
15. Measure where the camera model changes and whether a useful objective, minimap or landmark cue
    exists in the Wild Area, and capture each such cue across the Switch clock's hours and weather:
    the Wild Area is the first area whose lighting follows the system clock.

## 10. Implementation and PR sequence

Each code PR bases on `nav/shared` or a SwSh parent on it, and owns the new files it adds. No PR
copies shared engine files. Exact line counts wait for the evidence slice.

| Slice | Deliverable | Acceptance before the next slice |
|---|---|---|
| R0 | capture/route ledger, no gameplay claim | every Gate A screen and candidate anchor named; unsupported states listed |
| S1 | detector fixtures and `SwShScreenObserver` | positives and near-negatives pass; an unknown frame remains unknown |
| S2 | backend, `StoryGame`, program shell and empty table | scripted dispatch, save/reload, retry/exhaustion and log cases pass |
| S3 | the shared launch no-save end (a `nav/shared` PR first, section 3), then fresh-save, language, appearance, name and intro actions | the launch ends typed at the no-save screen on a fresh profile and still fails a reload there; three fresh starts from HOME reach observed control; a dropped press fails boundedly |
| S4 | options and first save/reload | save, injected failure, reload and resume return to the expected state |
| S5 | one walk primitive in PLZA's shape (a shared-engine PR first if section 6 needs one) | repeated prepared-save walks reach one nearby anchor; loss stops safely |
| S6 | 00.01 home/bag | three consecutive runs from its own start state |
| S7 | 00.02 Hop's house | three consecutive runs; door and scene failures reload correctly |
| S8 | 00.03 station approach | three consecutive runs and five Gate A end-to-end runs |
| S9 | frame replay and fault sweep over #479's failure classes; needs open #306 and #308 merged into `nav/shared` | dropped presses, missed events, stale looks and lost walks terminate/retry as designed |

The detector and route evidence should be offered to #1207's author with the engine integration.
Do not edit, close, reopen or mark ready their upstream PR. Any agent-created implementation PR stays
as a draft on `dayfine/Arduino-Source`; upstream publication remains the user's handoff.

## 11. Acceptance contract

Gate A is complete only when:

- every action states its start screen, end screen, timeout and failure result;
- no action presses through an unknown screen except a documented safe input with a finite bound;
- each detector has positive and near-negative image cases;
- every walk observes between bounded movement slices and stops when its endpoint does not appear;
- each saved checkpoint passes three times in a row from its own save;
- the complete Gate A route passes five times in a row from a fresh profile;
- an injected dropped input, missed detector and lost walk cause a finite failure/retry, not
  drift into the next step;
- logs name checkpoint, event, action, observations and stop reason, and retain failed frames;
- the authored plans lint and run through scripted replay;
- every console failure becomes a red-first regression test (AGENTS.md, MIGRATION.md step 5).

Walk failures have no motion model to replay in. **Proposed, user to confirm:** the red-first test
for a walk failure is a frame replay of the failing reader over the captured failure frames (#306),
plus a `FakeStoryBackend` script carrying the observed `"heading"` and endpoint readings that
reproduces the `lost` result. This depends on fork #306 (frame replay) and #308 (failure capture),
both open on `nav/grid-story-steps`; until they merge into `nav/shared`, S9 cannot start.

Its coverage boundary is stated, not implied: the pair reproduces the failed reader and the
propagation of `lost` (the walk returns it, the checkpoint attempt fails, the engine reloads or
stops), not the physical displacement that caused it. No test replays motion. Each such test must
also assert that nothing is dispatched after `lost`: the fake backend's recorded `inputs()`
(`FakeStoryBackend`) hold no press or stick from the walk or any later step after the one that
returned `lost`, and the next input, if any, is the reload's.

The immediate task is R0, followed by running the existing dialog/menu/arrow/fade detectors over the
captured opening frames. That evidence tells us whether Gate A's first implementation risk is screen
perception, navigation, or both.
