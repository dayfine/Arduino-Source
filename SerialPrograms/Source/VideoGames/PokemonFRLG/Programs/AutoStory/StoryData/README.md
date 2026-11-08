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
