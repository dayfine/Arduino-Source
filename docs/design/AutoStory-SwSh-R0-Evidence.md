# Sword/Shield AutoStory R0 evidence

This is the capture and route ledger for R0 of
[the Sword/Shield AutoStory work plan](AutoStory-SwSh-Work-Plan.md). R0 makes no gameplay claim:
it names the Gate A screens, candidate anchors and unsupported states that S1-S8 must turn into
detectors and bounded actions.

Status: **capture not started**. Rows sourced only from
[PokemonAutomation/Arduino-Source#1207](https://github.com/PokemonAutomation/Arduino-Source/pull/1207)
are hypotheses, not measurements. A row becomes evidence only when it names its console capture or
hand batch.

## 1. Evidence rules and support matrix

- Game screenshots remain local during R0. A later detector PR puts selected fixtures in
  `PokemonAutomation/CommandLineTests` and registers their positive and near-negative expectations.
- Every hand batch starts and ends with a screenshot. Movement batches record the exact inputs and
  take a screenshot after each leg.
- No route leg ends on elapsed time alone. The ledger names the fade, dialog, prompt, control cue or
  place-specific reader that could bound it; `unknown` means the action is not yet supportable.
- A quiet screen is not player control. Control evidence must survive quiet cutscenes, loading,
  interiors and outdoors.
- `declared_after` carries the area during Gate A. Only the station-approach reader is expected to
  supply an observed `pose` unless R0 demonstrates a generic area classifier.

| Dimension | R0 value | Evidence |
|---|---|---|
| Game/version | unknown | choose the installed version before the first capture |
| Game language | English | planned initial support row; not yet exercised |
| Console | Switch 1 | planned initial support row; not yet exercised |
| System keyboard | QWERTY | planned initial support row; not yet exercised |
| Video | 16:9, normal capture resolution | record exact dimensions with the first frame |
| Profile | no Sword/Shield save | required; profile and absence of a save not yet checked |
| Switch clock | unknown | record for every batch; repeat the lighting sample at two later hours |
| Display/capture settings | unknown | record brightness, RGB range and capture device if exposed |

## 2. Source inventory

| Source | What it establishes | What it does not establish |
|---|---|---|
| Current SwSh inference code on `main` | Existing dialog, arrow, menu, fade, battle, receive-Pokemon and Y-Comm primitives | Opening-story accuracy, an early control cue or a place reader |
| Current `CommandLineTests` SwSh fixtures | A small set for black dialog 2, party/menu arrows, Y-Comm and Max Lair | No fresh-start or Gate A screen; no positive triangle test |
| PokemonAutomation/Arduino-Source#1207 at `32d506c273879244e0ffc10c59ee01da2d30865e` | Nominal story order and raw timed inputs through the first Wild Area | Fresh-start screens, repeatability, checkpoint boundaries, observed endpoints or recovery |
| R0 console captures | pending | no claim until the rows below name a batch and frame |

## 3. Screen and transition ledger

`Positive` names frames on which the state must be recognized. `Near-negative` names the closest
screen on which the same reader must stay false. The candidate is not an implementation decision;
S1 may reuse it, tune it with regression coverage or replace it.

| ID | Screen/state | Positive | Near-negative | Candidate cue | R0 result |
|---|---|---|---|---|---|
| F00 | HOME with the Sword/Shield tile selected | pending | another game tile selected | shared HOME tile reader | unsupported pending capture |
| F01 | boot/loading before the no-save screen | pending | black door or scene fade | black/white transition watchers plus launch state | unsupported pending capture |
| F02 | no-save opening screen | pending | existing-save title/load screen | new no-save reader | unsupported pending capture |
| F03 | language selection | pending | appearance and ordinary row menus | new language-screen reader | unsupported pending capture |
| F04 | appearance selection | pending | language and ordinary choice menus | new appearance-screen reader | unsupported pending capture |
| F05 | system keyboard for the player name | pending | an in-game choice/menu | shared system-keyboard detector after SwSh layout check | unsupported pending capture |
| F06 | name confirmation | pending | ordinary yes/no and dialog | choice/arrow evidence | unsupported pending capture |
| F07 | Rose introduction, text still typing | pending | advance triangle and quiet cinematic | existing dialog candidates | unsupported pending capture |
| F08 | Rose introduction, line ready to advance | pending | text still typing and ordinary cinematic | `DialogTriangleDetector` / dialog candidates | unsupported pending capture |
| F09 | first player control in `PostwickHome` | pending | quiet cinematic, loading and dialog-free pause | new early-control cue | unsupported pending capture |
| F10 | Rotom Phone main menu | pending | overworld and options rows | `MainMenuDetector` / `RotomPhoneMenuArrowFinder` | unsupported pending capture |
| F11 | story options, each required row/value | pending | main menu and neighboring row/value | new options reader around existing arrow finder | unsupported pending capture |
| F12 | save card before the first save | pending | overwrite prompt and saved confirmation | existing main-menu/save routine plus new fixtures | unsupported pending capture |
| F13 | first save completed, control restored | pending | save in progress and quiet cinematic | early-control cue plus save outcome | unsupported pending capture |
| F14 | bag interaction prompt or aligned bag view | pending | door/NPC prompt and nearby home views | interaction prompt if stable; otherwise a place-specific endpoint | unsupported pending capture |
| F15 | bag scene/dialog | pending | Rose dialog and ordinary home dialog | existing dialog candidates | unsupported pending capture |
| F16 | control restored in `PostwickHome` after the bag | pending | F09 and scene not finished | early-control cue; story effect remains unobserved | unsupported pending capture |
| F17 | player's front door transition | pending | cinematic/battle black frames | black/white transition watchers | unsupported pending capture |
| F18 | control outdoors in `Postwick` | pending | home/Hop interior control and quiet outdoor cinematic | early-control cue; area declared | unsupported pending capture |
| F19 | Hop's front door/entry transition | pending | player's door and ordinary fade | transition watcher plus endpoint evidence | unsupported pending capture |
| F20 | Hop's house scene, line ready | pending | ordinary dialog and quiet interior | existing dialog candidates | unsupported pending capture |
| F21 | control restored in `HopHouse` | pending | scene pause and `PostwickHome` control | early-control cue; area declared | unsupported pending capture |
| F22 | Hop's front door/exit transition | pending | entry and player's door transitions | transition watcher plus endpoint evidence | unsupported pending capture |
| F23 | control on the Postwick road | pending | nearby off-route views | early-control cue; area declared | unsupported pending capture |
| F24 | Wedgehurst station approach before Leon | pending | nearby road, wrong facing and station scene started | new station-approach reader supplying `pose` | unsupported pending capture |
| F25 | manual-save overwrite prompt | pending | first-save card and ordinary yes/no | existing save routine plus new fixture | unsupported pending capture |
| F26 | existing-save title/load path | pending | no-save screen | StoryLaunch policy | unsupported pending capture |
| F27 | reloaded control at a Gate A save | pending | loading, cinematic and wrong declared area | early-control cue; station reader only for 00.03 | unsupported pending capture |

## 4. Route worksheet

Stick vectors and durations in the `#1207` column are transcribed exactly from that prototype, not
translated into map directions and not instructions for R0. `L(x,y) duration` means its authored
left-stick call. R0 replaces each row with a hand-recorded leg and an observable end. Split or
combine rows when the hand route exposes a better anchor.

| Leg | Declared start | Prototype input from #1207 | Candidate end | R0 batch/result |
|---|---|---|---|---|
| H1 | `PostwickHome / first_control` | no route input; segment 00 starts by mashing the introduction | F09 control | pending |
| H2 | `PostwickHome / first_control` | open the menu, then replay fixed option cursor inputs | F11 rows read and F09 control | pending |
| H3 | `PostwickHome / first_control` | `L(0,-1) 700ms`, `L(1,0) 4s`, `L(0,1) 100ms`, mash A 15 s | F14 prompt/view or F15 dialog | pending |
| H4 | `PostwickHome / bag` | included in the 15 s A mash | F16 control with bag scene finished | pending |
| H5 | `PostwickHome / bag` | #1207 starts segment 01 with `L(-1,-0.5) 5s`, then `L(0,-1) 20s` | F17 door fade, then F18 control | pending; boundary inside the holds unknown |
| P1 | `Postwick / player_house` | mash B 30 s, `L(1,0) 20s`, `L(-1,0) 1s`, `L(0,1) 2s` | F19 entry transition or F20 dialog | pending |
| HH1 | `HopHouse / scene` | mash B 30 s | F21 control | pending |
| HH2 | `HopHouse / interior` | `L(1,-0.3) 2s`, `L(0,-1) 300ms + 300ms`, `L(-1,0) 5s`, `L(0,1) 5s` | F22 exit transition, then F23 road control | pending; prototype boundaries unknown |
| W1 | `Postwick / road_to_wedgehurst` | mash until black, `L(-0.1,1) 30s`, then mash through two black transitions | F24 station approach before any Leon input | pending; prototype continues into the excluded station scene |

## 5. Movement and camera worksheet

Measure from a prepared save and return to that save between trials. A row records displacement and
camera behavior; it does not authorize a timed leg until its endpoint row in section 4 is observed.

| Area | Trial | Start anchor | Input | Displacement/collision | Camera/heading evidence | Repeat margin |
|---|---|---|---|---|---|---|
| `PostwickHome` | cardinal walk and turn | pending | pending | pending | pending | pending |
| `PostwickHome` | bag approach | pending | pending | pending | pending | pending |
| `Postwick` | player-house exit | pending | pending | pending | pending | pending |
| `Postwick` | road leg | pending | pending | pending | pending | pending |
| `HopHouse` | door approach | pending | pending | pending | pending | pending |
| `Wedgehurst` | station approach | pending | pending | pending | pending | pending |

Record explicitly whether the camera recenters and whether any minimap/compass exposes heading. If
Gate A shows no heading cue, the planned `"heading"` reader is unsupported and S5 must not assume it.

## 6. Save, reload and checkpoint worksheet

| Question | Evidence | Result |
|---|---|---|
| Can 00.00 save after first control? | pending | unknown |
| Can autosave be disabled before the first checkpoint? | pending | unknown |
| What does the first manual save show and where does it reload? | pending | unknown |
| Does a first save differ from an overwrite? | pending | unknown |
| Can 00.01 save at its proposed start and end? | pending | unknown |
| Can 00.02 save at its proposed start and end? | pending | unknown |
| Is F24 observable and saveable without starting Leon's scene? | pending | unknown |
| Does a reload on a no-save profile stop at the typed no-save terminal? | shared launch PR, later S3 | outside R0 implementation |

## 7. Lighting and nuisance variation

Capture the same prepared-save screen at two or three Switch clock hours. Gate A is expected to use
story-determined lighting; this table confirms that expectation rather than treating it as true.

| Screen | Clock/sample | Comparison | Result |
|---|---|---|---|
| `PostwickHome` control | pending | pending | unknown |
| `Postwick` outdoors | pending | pending | unknown |
| `HopHouse` control | pending | pending | unknown |
| station approach | pending | pending | unknown |

Also record NPC motion, occlusion, weather and capture brightness where they change a candidate cue.

## 8. R0 exit checklist

- [ ] The support matrix names the exercised game version, profile, language, keyboard, resolution
      and capture settings.
- [ ] F00-F27 each name positive and near-negative evidence, or are explicitly unsupported with a
      reason that constrains the later slice.
- [ ] Every Gate A route leg names its hand batch, exact input, observed end and repeat margin.
- [ ] The early-control cue has positives in home, outdoors, Hop's house and Wedgehurst, with quiet
      cutscene/loading near-negatives.
- [ ] Every proposed checkpoint boundary is checked for saveability and reload behavior.
- [ ] Station approach is observable and saveable before Leon, or Gate A names the nearest stable
      replacement.
- [ ] Gate A lighting is sampled at two or three Switch clock hours.
- [ ] Unsupported heading, prompt, landmark, version or language states are carried into the next
      slice instead of being inferred.

S1 starts only after this checklist is complete. Its first test set runs the current dialog, menu,
arrow and fade detectors over these frames before adding a new reader.
