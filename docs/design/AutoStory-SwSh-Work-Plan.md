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
house, the second station visit and the train to the first Wild Area. This replaces the full scope of
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

The current fork stack already supplies the game-neutral pieces. They are prerequisites, not files
to copy into Sword/Shield:

| Piece | Existing owner | Sword/Shield use |
|---|---|---|
| Story action registry, backend, JSON plans and scripted fake | `Pokemon/AutoStory/Engine/` | Register SwSh actions and run the same plans on a console or scripted test |
| Checkpoint save modes, retries, logs and postconditions | `Pokemon/AutoStory/` | Reload the last verified save after a failed checkpoint |
| Free-moving `stick()` backend input | `Pokemon_StoryBackend` | Turn and walk in bounded slices |
| Named-area pose | `Pokemon::ZonePose` | Carry `PostwickHome`, `Postwick`, `HopHouse` and `Wedgehurst` before an exact position reader exists |
| Shared screen events and scene judgements | `CommonTools/ScreenEvents/`, `Pokemon/AutoStory/` | Interpret SwSh detectors as dialog, choice, fade, battle and player-control events |
| Failure capture, frame replay and breakpoints | fork AutoStory stack | Preserve what a failed action saw and pause before a step or walk leg |
| Feedback-driven Switch system keyboard | `NintendoSwitch/Inference/` and `Programs/FastCodeEntry/` on the fork stack | Type the player name after a SwSh fixture verifies the same keyboard layout |

Fork [PR #114](https://github.com/dayfine/Arduino-Source/pull/114) holds the shared navigation/core
stack and fork [PR #332](https://github.com/dayfine/Arduino-Source/pull/332) the story engine. They
were merged into their parent branches, not upstream `main`; a SwSh implementation branch must name
its exact parent rather than assume these APIs are upstream.

Pokemon Legends: Z-A is the closest structural precedent. Its console backend, screen observer,
scene machine and landmark-steered walks show how a game without tiles plugs into the engine. Its
detectors, timings and route actions remain PLZA-specific and are not copied into SwSh.

### 2.2 Sword/Shield code on upstream main

| Existing code | Reuse status | Work before story use |
|---|---|---|
| `DialogBoxDetector`, `DialogTriangleDetector` | candidate reuse | Test opening dialog variants and quiet frames |
| `MainMenuDetector`, `PartyMenuDetector`, `BoxMenuDetector` | candidate reuse | Add opening-story and first-save fixtures |
| `SelectionArrowDetector`, Rotom Phone cursor reader | candidate reuse | Verify the language/appearance/options/choice layouts separately |
| black/white fade detectors in `CommonTools` | reuse | Test each opening door and scene transition |
| `save_game()`, `menus_to_mainmenu()` | strong reuse candidate | Wrap as an `Outcome`; verify first-save prompts and return to control |
| SwSh game start/reset routines | partial reuse | Existing-save entry is implemented; fresh-save/title handling is not |
| `StandardBattleMenu`, `StartBattle`, battle-dialog and EXP detectors | Gate B candidate | Register story-specific image cases before building a trainer loop |
| `basic_catcher()` and `ReceivePokemonDetector` | Gate B candidate | Wrap all results; cover nickname, full party and post-catch return |
| party/box detectors and `menus_to_boxsystem()` | Gate B candidate | Add verified move-order and deposit actions instead of fixed cursor scripts |
| Y-Comm icon detector | later reuse | It cannot prove the early overworld before Y-Comm is unlocked |
| overworld mark tracker and trajectory table | limited evidence | It follows exclamation/question marks, not doors or route landmarks; its steering maths may inform the pilot |

"Candidate" is intentional: a detector being used by another program does not establish its false
positive/negative behavior on the opening story.

### 2.3 What must be invented

The new work is bounded to the game adapter and the evidence it needs:

- a SwSh screen observer, console backend, action registry and story table on the shared engine;
- fresh-save, language, appearance and options detection;
- an early player-control cue that works before Y-Comm;
- landmark readers and a bounded visual waypoint walker;
- named opening poses and observed checkpoint postconditions;
- Gate B only: a story battle loop and complete catch/party outcomes around the existing primitives.

The work does not invent another checkpoint engine, JSON contract, retry loop, keyboard driver or
generic tile planner.

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

An action has a documented start screen, end screen, safe inputs, timeout and failure result. New
game actions use existing story kinds where their meaning fits; game-specific mechanics remain
named `GAME` actions rather than adding generic kinds prematurely.

| Action | Gate | Reuse | New contract |
|---|---:|---|---|
| `launch_fresh_save` | A | generic Switch launch | Reach a recognized no-save screen; reject an existing save without input |
| `choose_language` | A | selection movement | Read the language screen, select the configured row, verify the next screen |
| `choose_appearance` | A | selection movement | Select a supported appearance and verify the name handoff |
| `enter_player_name` | A | system keyboard driver | Wait for the keyboard, type, confirm separately, verify the game returns |
| `mash_scene_to_control` | A | shared scene machine/judges | Advance recognized text and choices until a SwSh control cue holds |
| `set_story_options` | A | main-menu navigation | Read each supported row/value, change only mismatches, return to control |
| `interact` | A | A press, dialog/fade events | Confirm the requested scene, prompt or transition started |
| `choose_dialog_row` | A | arrow detection | Move from the observed row, confirm once, verify the choice closed |
| `walk_to_landmark` | A | backend `stick()` | Steer from landmark observations until its explicit arrival condition |
| `walk_until_event` | A | fade/dialog/control events | Move in short slices until the requested event or the bound |
| `save_story_game` | A | `save_game()` | Verify first-save completion and control restored; return an `Outcome` |
| `reload_story_save` | A | close/reopen/load routines | Verify the expected area/control cue before a retry starts |
| `verify_station_approach` | A | landmark reader | Establish the Gate A postcondition without starting Leon's scene |
| `choose_starter` | B | action/choice framework | Identify or reliably approach the configured starter and verify receipt |
| `fight_story_battle` | B | battle detectors | Bounded move choice through victory/loss and return to the requested screen |
| `catch_story_pokemon` | B | `basic_catcher()` | Report caught/fainted/own-fainted/out-of-balls/unsupported follow-up exactly |
| `reorder_move` / `deposit_party` | B | party/box readers | Observe cursor and result rather than replay a fixed menu script |

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
| named landmarks | steer and establish checkpoint anchors | approach sequence, occlusion and wrong-facing negatives |
| station approach | Gate A postcondition | several arrival offsets and nearby non-station views |

The early control cue is the first detector risk. "No dialog" is not sufficient: cutscenes and
loading frames also lack a dialog. Prefer a stable HUD or interaction affordance; otherwise combine
several weak cues and require them to hold.

### 4.3 Gate B detectors

Gate B adds starter selection/receipt, story-battle victory and loss, learn-move/evolution prompts,
nickname and party-full choices, and a Wild Area/free-camera state. Existing battle and catch
detectors are inputs, not complete outcomes.

## 5. Game routines and layout

The implementation should follow the free-moving game shape already proven structurally by PLZA:

```text
PokemonSwSh/Programs/AutoStory/
  PokemonSwSh_AutoStory.{h,cpp}              program and options
  PokemonSwSh_AutoStoryBackend.{h,cpp}       controller, video, observations, battle call
  PokemonSwSh_AutoStoryGame.{h,cpp}          actions, readers, save/reload, interrupts
  PokemonSwSh_AutoStoryScreenEvents.{h,cpp}  detector-to-event adapter
  Actions/
    PokemonSwSh_AutoStoryOpening.*
    PokemonSwSh_AutoStoryScenes.*
    PokemonSwSh_AutoStoryOptions.*
    PokemonSwSh_AutoStoryWalks.*
    PokemonSwSh_AutoStoryBattles.*           Gate B
  Detectors/
    PokemonSwSh_AutoStoryNewGame.*
    PokemonSwSh_AutoStoryControl.*
    PokemonSwSh_AutoStoryOptions.*
    PokemonSwSh_AutoStoryLandmarks.*
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
5. Visual walk controller: steers to one named landmark and reports arrived, lost, interrupted or
   timed out.
6. Scripted backend cases: run the exact checkpoint plans through success, dropped input, detector
   miss, retry and exhaustion without a console.

## 6. The 3D navigation pilot

Do not make a global navigation abstraction before one SwSh route works. The first primitive is a
bounded visual waypoint:

1. A landmark reader returns horizontal bearing, apparent size and confidence.
2. The walker turns or walks for a short slice.
3. It releases the stick and observes again.
4. It corrects the next slice from the new reading.
5. It ends on a fade, dialog, interaction prompt, landmark threshold or explicit endpoint reader.
6. If the landmark disappears, it performs a small bounded scan.
7. If the scan does not reacquire it, the action returns `lost`; the checkpoint reloads rather than
   continuing blind.

Short measured legs are acceptable only between two observed anchors, marked as assumed in the
route log and bounded by the endpoint detector. A long hold from #1207 is not an anchor.

Use named poses initially:

```text
PostwickHome / bag
PostwickHome / exit
Postwick / player_house
HopHouse / interior
Postwick / road_to_wedgehurst
Wedgehurst / station_approach
```

`ZonePose` can carry the area while action-specific readings carry the landmark. Define a new exact
pose type only after a position reader has demonstrated coordinates or another stable representation.

## 7. Gate A checkpoints

The evidence round may move a boundary when the game does not permit a save there. Each proposed
checkpoint still has one saved start and a separately observed end.

| Checkpoint | Start | Steps | End/postcondition |
|---|---|---|---|
| 00.00 Fresh start | recognized no-save launch | language, appearance, name, Rose introduction | player control in `PostwickHome` |
| 00.01 Bag | saved/control in home | options, walk to bag, interact, clear scene | bag obtained, control restored in home |
| 00.02 Hop's house | saved/control in home | leave through fade, landmark route, enter Hop's house, clear scene | control at a recorded Hop-house anchor |
| 00.03 Station approach | saved/control after Hop's scene | leave, follow road landmarks, stop before Leon | `Wedgehurst/station_approach`, no station scene started |

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
7. **Wild Area feasibility.** Camera model, first free-camera walk and recovery; stop expansion if no
   bounded localization/arrival method is demonstrated.

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
5. Measure camera and stick behavior on each Gate A map: turn rate, displacement, collisions and
   whether camera recentering is automatic.
6. Identify at least one stable landmark for every walk and collect wrong-facing, occluded and nearby
   negative frames.

### Support decisions

7. Start with one declared matrix: English game, QWERTY Switch 1 system keyboard, one version,
   16:9 and the normal capture resolution. Add a matrix row only with fixtures and a live pass.
8. Check Sword versus Shield differences before the station, then the legendary scene separately.
9. Check whether time, weather, NPC motion or display brightness changes a Gate A landmark.
10. Decide which settings are required for reliable execution. Record their values as a precondition
    and verify them rather than replaying #1207's cursor counts.

### Gate B research

11. Characterize each existing battle detector on the starter/Hop and Weald battle screens.
12. Determine the minimal battle policy per starter, possible losses and any move-learn/evolution
    prompts before the Wild Area.
13. Exercise `basic_catcher()` on the tutorial and ordinary early catches, including party-full and
    nickname paths.
14. Measure where the camera model changes and whether a useful objective, minimap or landmark cue
    exists in the Wild Area.

## 10. Implementation and PR sequence

Each code PR is a child of its real shared parent and owns the new files it adds. No PR copies shared
engine files. Exact line counts wait for the evidence slice.

| Slice | Deliverable | Acceptance before the next slice |
|---|---|---|
| R0 | capture/route ledger, no gameplay claim | every Gate A screen and candidate anchor named; unsupported states listed |
| S1 | detector fixtures and `SwShScreenObserver` | positives and near-negatives pass; an unknown frame remains unknown |
| S2 | backend, `StoryGame`, program shell and empty table | scripted dispatch, save/reload, retry/exhaustion and log cases pass |
| S3 | fresh-save, language, appearance, name and intro actions | three fresh starts reach observed control; a dropped press fails boundedly |
| S4 | options and first save/reload | save, injected failure, reload and resume return to the expected state |
| S5 | one landmark-walk primitive | repeated prepared-save walks reach one nearby anchor; loss stops safely |
| S6 | 00.01 home/bag | three consecutive runs from its own start state |
| S7 | 00.02 Hop's house | three consecutive runs; door and scene failures reload correctly |
| S8 | 00.03 station approach | three consecutive runs and five Gate A end-to-end runs |
| S9 | frame replay and fault sweep | dropped presses, missed events, stale looks and lost landmarks terminate/retry as designed |

The detector and route evidence should be offered to #1207's author with the engine integration.
Do not edit, close, reopen or mark ready their upstream PR. Any agent-created implementation PR stays
as a draft on `dayfine/Arduino-Source`; upstream publication remains the user's handoff.

## 11. Acceptance contract

Gate A is complete only when:

- every action states its start screen, end screen, timeout and failure result;
- no action presses through an unknown screen except a documented safe input with a finite bound;
- each detector has positive and near-negative image cases;
- every walk observes between bounded movement slices and stops on landmark loss;
- each saved checkpoint passes three times in a row from its own save;
- the complete Gate A route passes five times in a row from a fresh profile;
- an injected dropped input, missed detector and lost landmark cause a finite failure/retry, not
  drift into the next step;
- logs name checkpoint, event, action, observations and stop reason, and retain failed frames;
- the authored plans lint and run through scripted replay;
- visual walking remains explicitly console-only offline until a measured SwSh motion model exists.

The immediate task is R0, followed by running the existing dialog/menu/arrow/fade detectors over the
captured opening frames. That evidence tells us whether Gate A's first implementation risk is screen
perception, navigation, or both.
