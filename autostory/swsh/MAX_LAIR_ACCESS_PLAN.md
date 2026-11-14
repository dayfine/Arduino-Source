# Sword/Shield early-game automation: Crown Tundra access

This is a rough design for a native Sword/Shield AutoStory slice. Its purpose is
to take a newly started save to a **DLC-ready Wedgehurst Station**, where Crown
Tundra travel is available and the player can continue to the Max Lair. It is a
design and route-research record, not an executable plan or a console-validated
route.

The initial program should stop at the station. Travelling through Crown Tundra,
losing the allowed Peony battle and walking into the Max Lair are a small follow-up
slice, described below but outside the first implementation. Isle of Armor travel
and story progression are out of scope.

## Completion condition

Physical arrival at Wedgehurst Station is not enough. Nintendo's support page
requires access to the Wild Area and the station Slowpoke event before DLC travel.
The more precise in-game gate is:

1. Reach the Wild Area Station.
2. Obtain the Pokémon Box Link from Sonia at the Meetup Spot.
3. Return to Wedgehurst Station.
4. Trigger the Klara (Sword) or Avery (Shield) scene and **catch** Galarian
   Slowpoke.
5. Talk to the station attendant and observe `Crown Tundra Station` in the
   destination menu.

The Slowpoke gate is shared by both DLC destinations even though this plan only
uses Crown Tundra. While Slowpoke is present, the attendant asks the player to
catch it instead of offering DLC travel. The last observation above is the
acceptance test. Expected story progress is not evidence that the option exists.
After verifying it, cancel the menu, save, and stop facing the attendant so a
user can continue manually.

Sources:

- [Nintendo: Expansion Pass access requirements](https://www.nintendo.com/en-gb/Support/Purchases-Subscriptions/Games/Pokemon-Sword-or-Pokemon-Shield-Expansion-Pass-DLC-Is-Not-Appearing-1833572.html)
- [Wedgehurst Station progression and DLC gates](https://bulbapedia.bulbagarden.net/wiki/Wedgehurst_Station)
- [Crown Tundra opening and loss-tolerant Peony battle](https://bulbapedia.bulbagarden.net/wiki/Walkthrough:The_Crown_Tundra/Part_1)

## What the existing upstream draft gives us

[Upstream draft PokemonAutomation/Arduino-Source#1207](https://github.com/PokemonAutomation/Arduino-Source/pull/1207),
`start SwSh AutoStory (until first Wild Area)`, currently ends at head
`32d506c273879244e0ffc10c59ee01da2d30865e`. Its comments and controller
sequences provide a useful first route trace:

| Old segment | Route represented in the draft | Use here |
|---|---|---|
| `segment_00` | Intro, settings and bag | Keep the boundary; replace blind waits with observed actions. |
| `segment_01` | Hop's house, Leon at Wedgehurst, starter and first Hop battle | Keep, split into restartable checkpoints. |
| `segment_02` | Slumbering Weald and legendary scene | Keep the route order; add battle and scene state handling. |
| `segment_03` | Return through Route 1, lab and Pokédex | Keep; do not require the draft's opportunistic catch. |
| `segment_04` | Route 2, Magnolia, Hop battle and Dynamax Band | Keep; make planned and random battles explicit. |
| `segment_05` | Return to Wedgehurst Station and first Wild Area arrival | Keep. |
| start of `segment_06` | Meetup Spot / Box Link progression | Split here; the later Onix/faint shortcut is out of scope. |

The PR should be treated as route evidence, not as a branch to revive. At the
time of inspection it is an old-path, single-class implementation with no tests
or review and is hundreds of upstream commits behind `main`. Its helpers often
mash for minutes, interpret a black or white transition as a battle, and do not
verify checkpoint arrivals. The new slice should preserve route knowledge while
using the current `SerialPrograms/Source/VideoGames/PokemonSwSh` layout and the
segment/checkpoint shape of the existing SV AutoStory program.

## Proposed native structure

Keep UI/orchestration, checkpoint definitions, reusable actions and inference
separate:

```text
VideoGames/PokemonSwSh/Programs/AutoStory/
  PokemonSwSh_AutoStory.{h,cpp}              program options and orchestration
  PokemonSwSh_AutoStoryTools.{h,cpp}         shared types, stats and bounded helpers
  PokemonSwSh_AutoStoryActions.{h,cpp}       console action seam and implementation
  PokemonSwSh_AutoStoryRoutes.{h,cpp}        short calibrated route legs
  PokemonSwSh_AutoStory_Segment_00.{h,cpp}   intro and bag
  ...
  PokemonSwSh_AutoStory_Segment_07.{h,cpp}   Slowpoke and DLC-ready station

VideoGames/PokemonSwSh/Inference/Story/
  PokemonSwSh_StoryChoiceDetector.{h,cpp}
  PokemonSwSh_StationMenuDetector.{h,cpp}
  PokemonSwSh_StoryOverworldDetector.{h,cpp}  only if existing signals prove insufficient
```

`PokemonSwSh_AutoStory.cpp` should build an ordered list of segment objects and
offer start/end checkpoint selection, as SV AutoStory does. Each checkpoint has
named start and end states and calls a small action interface. Native tests use a
scripted implementation of that interface; console code owns controller and
detector details.

The initial options are:

- game language;
- starter choice;
- start and end checkpoint;
- whether to apply the known settings profile;
- bounded checkpoint retry count; and
- go Home when done.

The entry contract is after language, avatar and name selection, when the intro
cutscene has begun. Automating the system keyboard is not needed for this slice.
The setup contract also requires the matching Sword/Shield Expansion Pass to be
installed.

## Segments and checkpoints

These boundaries intentionally follow the old PR's story order, but are small
enough to save, replay and diagnose independently. Exact stick vectors and
durations remain unbound until recorded on console.

| ID | Start | Required actions | Confirmed end / safe anchor |
|---|---|---|---|
| `00.00` | Intro cutscene after name selection | Clear the opening scene. | Player is controllable in the living room. |
| `00.01` | Living room | Apply/verify settings if requested; collect hat and bag; descend and leave home. | Postwick exterior after the door transition. |
| `01.00` | Outside player's house | Follow Hop, enter his house, clear the family scene, take the safe Route 1 lane to Wedgehurst. | Leon arrival scene has started at the station. |
| `01.01` | Leon arrival scene | Clear the automatic return scene; choose configured starter; complete the first Hop battle. | Controllable outside Hop's house with starter received. |
| `02.00` | Postwick after starter battle | Enter Slumbering Weald; traverse the fixed path; complete required wild battles. | Legendary encounter scene has started. |
| `02.01` | Legendary scene | Resolve the unwinnable encounter and rescue scene. | Controllable at home/Postwick after rescue. |
| `03.00` | Postwick after rescue | Travel Route 1 to Wedgehurst; enter the lab; clear Sonia/Leon dialogue. | Pokédex received and player controllable outside the lab. |
| `03.01` | Outside lab | Handle or decline the Pokémon Center tutorial deterministically; heal if the route policy requires it. | Route 2 entrance available, team state known. |
| `04.00` | Route 2 entrance | Receive the 20 Poké Balls; traverse trainer legs; resolve planned or accidental battles. | Professor Magnolia's house transition/scene detected. |
| `04.01` | Magnolia's house | Clear story dialogue; win the Hop battle; receive the Dynamax Band next morning. | Controllable outside the house with return route open. |
| `05.00` | Magnolia's house exterior | Return south without blind continuation after encounters; enter Wedgehurst Station; clear Camping Gear/train scenes. | Wild Area Station interior is controllable. |
| `06.00` | Wild Area Station | Exit to the Meetup Spot and clear Sonia's scene. | Pokémon Box Link receipt is confirmed and station return route is available. |
| `06.01` | Meetup Spot | Retrace to Wild Area Station; use ticket barrier/stand to return. | Wedgehurst Station interior after the train transition. |
| `07.00` | Wedgehurst Station | Trigger Klara/Avery scene; interact with Slowpoke; catch it with Poké Balls; clear post-catch prompts. | Station interior is controllable and Slowpoke is gone. |
| `07.01` | Station after Slowpoke | Talk to attendant; inspect destinations; confirm Crown Tundra option; cancel and save. | Saved, facing attendant, DLC destination verified. |

The first full-run implementation can still group these into the old eight
segments. Checkpoints are the restart and test units; segments are UI grouping.

## Action vocabulary

Use the common story concepts where possible rather than embedding one long
controller script:

| Action | Contract |
|---|---|
| `legs` | Execute a short calibrated stick/tap sequence from a named anchor. End at another pose belief or a monitored event; never claim displacement after an interruption. |
| `wall_align` | Hold toward a console-verified safe wall/rail/door to collapse position uncertainty. The route record names the collision surface and safe duration. |
| `talk` | Press A only from an interaction pose and wait for a dialogue/choice/transition detector. |
| `answer` | Detect the choice cursor, move from the observed row to the requested answer, press A, and verify the next state. |
| `scene` | Advance dialogue while monitoring all permitted terminal states. It must not equate every black frame with a battle. |
| `battle` | Drive a required or accidental battle from detected battle states to a declared end state. |
| `catch` | Use the existing basic catcher for the required Slowpoke and classify every terminal result. |
| `station_travel` | Detect the station destination menu, select a named destination, then verify the train transition and arrival state. |
| `save` | Reuse the existing detector-backed SwSh `save_game()` helper from an observed overworld state. |

SwSh-specific game actions are appropriate for starter selection, station travel
and the Slowpoke gate. They should still expose typed results rather than hide a
button macro inside `game_action`.

### Navigation policy

Early Galar is mostly fixed-camera and corridor-like, so the first slice does
not need a general 3D planner or YOLO landmarks. It needs repeatable local routes:

1. Begin every route leg at a transition, cutscene endpoint or verified wall
   alignment.
2. Break travel into short legs whose expected interrupt is known: dialogue,
   door fade, trainer battle or timeout.
3. Monitor for battles while moving. An interrupt invalidates the unexecuted
   duration and the claimed arrival; never restart the entire timed hold.
4. After a planned trainer battle, resume only from a measured/reproducible
   post-battle anchor. After a random battle, use a nearby safe wall alignment;
   if none exists, reload the checkpoint.
5. Treat Route 2 grass as uncertainty, not as a harmless delay. Prefer the
   lowest-encounter route and bound the number of accidental battles.
6. Wait for a detector at doors and train transitions. Small settle delays may
   follow a confirmed transition, but a delay is not the confirmation.

The route table should store semantic leg names and evidence references beside
controller values. Measurements from PokemonAutomation/Arduino-Source#1207 are
candidates only until replayed against a current build and both game versions.

### Battle policy

The early route should not depend on catching a random Route 1 Pokémon. Use the
starter as the only required battler and keep the policy simple:

- detect the standard battle command menu before input;
- select a known damaging move slot for the configured starter/progression
  point, rather than assuming every A press is still on `Fight`;
- advance battle dialogue and experience screens as separate states;
- stop only at the expected story scene or observed overworld;
- allow the scripted Slumbering Weald legendary result;
- fail with a screenshot on an unrecognized menu, party wipe or exhausted
  action budget.

If one move must be reordered after the first Hop battle, make that a verified
menu action at a checkpoint. Do not reproduce the old draft's blind sequence.

For Galarian Slowpoke, reuse `basic_catcher()` with `poke-ball`. Route 2 provides
20 balls and Slowpoke must be caught, so defeating it is not a valid completion.
Save at the last safe state before returning to the station. `OWN_FAINTED`,
`OUT_OF_BALLS`, `CANNOT_THROW_BALL` and timeout reload that save within the
checkpoint retry budget; they are not silently accepted.

## Detection inventory

Most state classification already exists in SwSh. The implementation should
compose these before adding new detectors.

| State | Existing primitive | Remaining work |
|---|---|---|
| Screen fade / scene transition | `BlackScreenOverWatcher`, `WhiteScreenOverWatcher` | Couple it to the expected next state; a fade alone is ambiguous. |
| Normal dialogue | black/white dialog watchers and `DialogTriangleWatcher` | Add a bounded scene driver that distinguishes dialogue from choices and battle dialogue. |
| Battle start/menu/end | `StartBattleWatcher`, `StandardBattleMenuWatcher`, `BattleDialogDetector`, `ExperienceGainWatcher` | Add one early-story state machine and scripted test seam. |
| Main menu / save | `MainMenuWatcher`, selection-arrow finder, `save_game()` | Reuse. Verify availability before the first attempted checkpoint save. |
| Overworld after Y-Comm unlock | `YCommIconWatcher` | Determine the exact unlock checkpoint. Before it, use checkpoint-specific end evidence. |
| Pokémon received/caught | `ReceivePokemonWatcher` plus `basic_catcher()` result | Confirm the Slowpoke post-catch prompt flow in both party-full and party-not-full cases; this route should normally have space. |
| Starter choice | none suitable identified | Add a layout/cursor detector or capture a stable selection anchor for all three choices. |
| Yes/no and tutorial choices | selection-arrow components exist | Add a narrow story-choice detector with row/index output. |
| Station destinations | none suitable identified | Add a menu-shape detector plus language OCR for named destinations. This also supplies the final acceptance observation. |
| DLC-ready state | none | Derive only from a successful Crown Tundra destination read, not from elapsed story steps. |

Every new detector needs positive and negative captured-frame tests. In
particular, station OCR fixtures should cover Sword and Shield, the menu before
Box Link, Slowpoke-present state, and the final menu with Crown Tundra available.

## Recovery and checkpoint state

Each checkpoint records its start contract, save identity, action ID, attempt
number and last observation. A full run saves only from a confirmed safe state.
Autosave should be disabled by a verified settings action if reload recovery is
enabled.

Recovery is deliberately narrow:

- Unrecognized screen: stop input, capture a screenshot/report with checkpoint
  and action IDs, then reload only if the checkpoint has a known save.
- Unexpected wild battle: run the battle policy, invalidate pose, and re-anchor
  or reload; do not continue the interrupted joystick duration.
- Missing expected transition/dialogue: make one detector-safe neutralization
  attempt, such as B from a recognized menu. Otherwise stop and report.
- Slowpoke catch failure: reload the previous safe save and retry within budget.
- Missing Crown Tundra destination: report unmet setup/progression, leaving the
  save intact. This is not a navigation retry.

Expected receipts such as Pokédex, Dynamax Band and Box Link remain story claims
until supported by the next gate or a direct observation. The final station menu
is allowed to prove DLC readiness because that is the capability the program is
trying to establish.

## Tests and console evidence

Native C++ tests should cover:

- checkpoint order, start/end selection and retry exhaustion through the action
  seam;
- a fade followed by dialogue versus a fade followed by battle;
- battle-menu desynchronization, move selection and return to overworld;
- an interrupted route leg never reporting its declared destination;
- every `basic_catcher()` terminal result at the Slowpoke checkpoint;
- station menus before and after Box Link/Slowpoke, including absence of the
  Crown Tundra option; and
- Sword/Shield station-event visual differences.

Offline simulation can hold the shared event order, progression gates and
interruption/recovery scenarios. It cannot establish detector accuracy, stick
timings or physical arrivals. Those require one checkpoint-at-a-time console
runs, screenshots for every start/end state, and then a clean full run.

## Implementation slices

1. Capture and label the start/end screen for every checkpoint in a current
   Sword or Shield save. Re-run the PokemonAutomation/Arduino-Source#1207 route
   manually and retain only measured legs.
2. Add program/checkpoint scaffolding and the scripted action seam; implement
   `00` and `01` using existing detectors.
3. Add the early battle state machine and complete Slumbering Weald.
4. Add Wedgehurst/Route 2 routes and interruption-safe recovery.
5. Add first station travel, Wild Area Station and Box Link checkpoints.
6. Add return travel, Slowpoke catch, station destination OCR and the DLC-ready
   acceptance test.
7. After the station slice is reliable, optionally automate Crown Tundra travel,
   allow either result of the Peony battle, walk east to the Max Lair, and hand
   off only after the existing Max Lair entrance state is recognized.

No implementation slice should claim the next one from elapsed inputs alone.
The useful first milestone is not “the old macro ran to the end”; it is that each
checkpoint can state where it began, what interrupted it, and what observation
proved completion.
