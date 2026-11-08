# FRLG checkpoint JSON pilot

`frlg-parcel-00.05.json` describes checkpoint `00.05` (Oak’s Parcel). It is a capture of the native table, using `pokesim-native-checkpoint-export/1`, including its FRLG extension fields. `source_revision` records the original capture revision; it is not the revision currently being tested.

The current C++ story table remains the production source of truth. This pilot adds a strict reader and two native checks:

1. **Data equivalence:** parse the JSON into `StoryCheckpoint`, export it again, and compare it with the current compiled table’s `00.05` row. Both the portable events and serialized FRLG fields must match. A changed table with stale JSON fails this check.
2. **Execution equivalence:** build both rows through the same story engine and run both through the existing fake-game simulator. Compare the event description, execution trace, engine log and saved position.

Read `check_loaded()` in `PokemonFRLG_AutoStoryExport_Tests.cpp` and `test_data_row_parity()` in `PokemonFRLG_AutoStorySimulation_Tests.cpp` for these assertions. Run the registered native tests through the repository’s normal command-line test mode; CI runs them on Linux, Windows and macOS. No console is needed.

This checks equality of the **serialized contract** and execution under the fake-game model. It does not validate real controller timing, detectors, scene internals, live state, or fields the exporter does not serialize. It does not switch the production executor to JSON or establish a general format for every game.

## Migration

Once production loads the checkpoint from data, remove its duplicate C++ row and the table-equivalence assertion. Keep the strict reader, malformed-input tests, and tests of expected actions and outcomes. Retain the exporter only while other C++ rows need conversion.

The cross-repository route replay diagnostic is a separate follow-up. Its Python route candidates and scripted observations are not required to load this JSON or check it against the table.

## Field meanings and authoring direction

Checkpoint fields are ordered `id`, `title`, `start`, `events`; each event is an expanded block beginning with `id`, `kind`. Keep coordinate pairs and short pose fields compact. JSON object order is presentation only.

- `declared_after`: the author-declared pose after an event. It is a fallback/expectation, not evidence of the actual map, tile or facing. The current scene executor advances dialogue until the overworld returns; it does not independently localize the player or verify the Parcel receipt.
- `status: native-table-export`: this document was exported from a compiled story table. It does not claim the checkpoint ran or succeeded.
- `state_basis: native-table-bookkeeping`: the saved values are authored/in-memory bookkeeping, not observed entry state. Defaults such as `level: 0` or `hp_fill: 1` must not become requirements merely because they were exported.
- `format: pokesim-native-checkpoint-export/1`: selects the versioned export contract. Its JSON Schema lives in [PokemonSimulation](https://github.com/dayfine/PokemonSimulation/blob/main/schemas/native-checkpoint-export.schema.json); its shared checkpoint structure is [story-checkpoint.schema.json](https://github.com/dayfine/PokemonSimulation/blob/main/schemas/story-checkpoint.schema.json). The native reader separately enforces its supported contract.
- `native_fields`: FRLG extensions outside the common checkpoint events/poses. This pilot retains the full serialized menu/supplies snapshot for comparison with the C++ table. It is not a list of verified preconditions, nor a checksum proving that earlier checkpoints ran.

For data-authored checkpoints, separate **requirements** (only state this checkpoint depends on), **effects/expected exit state** (what it should change), **runtime state** (inventory, party, observations), and **export provenance**. Entry requirements summarize the relevant consequences of previous progress; unrelated counters, default values and transient battle bookkeeping should not be copied into every checkpoint. A later checkpoint must verify its requirements or explicitly declare an assumption, including when starting from a save or returning from a handoff.

This PR preserves the export contract for equivalence testing. A lean executable-plan contract and its required observation/executor bindings are a separate migration step; simply deleting snapshot fields would weaken the comparison.
