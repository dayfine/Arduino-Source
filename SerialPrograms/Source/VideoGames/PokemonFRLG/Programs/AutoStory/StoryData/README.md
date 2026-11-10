# FRLG authored checkpoints

Every story checkpoint is an authored plan here, one file per checkpoint: `frlg-<id>.plan.json`, and `frlg-parcel-00.05.plan.json` for 00.05. `story_checkpoints()` is `load_authored_story()`: every embedded `*.plan` file, in order of id. There are no C++ row definitions; the files were exported once from the C++ table, and `ReplayData/frlg-story-sim-trace.json` keeps the whole story's sim trace from before that (`PokemonFRLG::AutoStory - Authored` tests it).

- A checkpoint has `sections: [{id, title, notes?, events}]`, or plain `events` (one unnamed section, as 00.05 keeps so its bytes stay those PokemonSimulation pins). Events are numbered across the sections, and an event's log line names its section: `[Event] 00.28/12 [b1f_east] winston_line: start`.
- `notes` (a list of paragraphs) on the checkpoint, a section or an event hold what the C++ table's comments said: the maps, trainers, numbers and console runs a step was planned from. No executor reads them.
- `variant` says how FRLG does a step: `run` or `bike` on a route or legs, `trainer_line`/`trainer_wait` on a step, and on a `game_action` the step's plan as its parser reads it (`mart ...`, `teach ...`, `grind ...`, `grind_check ...`, `capture ...`, `pc ...`, `fossil ...`, `level ...`, or `cut_tree`, `trash_cans`, `close_trainer_card`, `leave_gift_summary`, `center_enter`, `center_exit`, `ride_bike`, `use_item ...` and the Rocket Hideout's steps from `PokemonFRLG_AutoStoryHideout.h`). An unknown variant is rejected at load.
- `once_per_save` marks a step done once per save (a TEACH, a Mart).
- `execution` holds FRLG's fields: `menu`, and when not a new game's, `supplies` (`potions`, `antidotes`, `parlyz_heals`, `lead: "after_brock"`), `live_start` and `handoff` (`sid_helper`, `starter_rng`, `gift_rng`).
- Unknown fields anywhere are rejected.

## The Parcel pilot

`frlg-parcel-00.05.plan.json` is the production definition of checkpoint `00.05`. The story table loads it with `load_authored_checkpoint()`: there is no second C++ Parcel event list. Its `pokesim-authored-checkpoint/1` contract contains the common checkpoint plus FRLG executor configuration (`execution.menu`). It has no export revision, bookkeeping snapshot, or claimed observation.

The same file is checked into PokemonSimulation at `data/story/parcel-pilot/frlg-parcel-00.05.plan.json`; the paired CI check compares its exact bytes and runs the existing seven replay scenarios there. The schema is `schemas/authored-checkpoint.schema.json`, referencing `story-checkpoint.schema.json`.

## The two gates

1. Native tests load the actual table entry through JSON, run the existing engine/fake-game simulator, and compare its events/trace/save pose with the captured pre-migration checkpoint. Existing full-story progression tests also use the JSON-backed table entry. Unsupported fields/menu values and malformed common actions are rejected.
2. PokemonSimulation loads the same authored file with explicit map/action bindings and scripted evidence, then asserts the seven scenarios, including the full successful semantic action sequence and interruption/failure cases. It does not import the native supplies snapshot as requirements.

`frlg-parcel-00.05.json` is the old table-export fixture used for migration regression checks only. Its provenance and snapshot fields are not part of the authored plan. It can be removed once the native expectations are independent of the capture.

## Semantics

- Checkpoint fields are ordered `id`, `title`, `start`, `events`; each event has an expanded block beginning with `id`, `kind`.
- `start` is the assumed checkpoint/save pose, not a localization observation.
- `declared_after` preserves the current scene executor’s author-declared arrival fallback. It is not independent evidence of position or Parcel receipt. Real door/scene verification remains a separate executor change.
- `execution.menu` configures the existing FRLG save implementation. Inventory, party and battle bookkeeping keep their existing native initialization; they are neither asserted preconditions nor observations.
- This first contract makes no new inventory/party requirements. Add explicit requirements only when an action depends on them, with supported verification/assumption semantics on both sides. Relevant prior progress is not proof of a complete execution history.
- The simulator’s map pins, scene evidence and stop/no-save policy live in the binding and cases, not in the authored checkpoint. Python replay is a model check; native fake-game execution checks integration, not live timing/detector correctness.

No console is required for either gate.

## Native JSON code

`Pokemon/AutoStory/Pokemon_AuthoredCheckpointJson.*` owns the common authored envelope, checkpoint codec delegation, and embedded-resource lookup. `../Data/JsonParser.*` validates FRLG execution settings and retains the legacy capture reader. `JsonWriter.*` is retained for migration comparisons and export diagnostics; production authored loading does not depend on it. `Json_Tests.*` keeps the codec tests beside those sources. Once migration expectations are independent of the old capture, remove the legacy reader, writer, and `LegacyJsonFormat.h`; retain the authored parser.
