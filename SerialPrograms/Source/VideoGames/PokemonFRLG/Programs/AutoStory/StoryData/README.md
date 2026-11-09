# FRLG authored checkpoint pilot

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
