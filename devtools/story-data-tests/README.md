# Shared story-record codec tests

Run without Qt, controllers, profiles or game saves:

```sh
cmake -S devtools/story-data-tests -B build/story-data-tests
cmake --build build/story-data-tests -j4
ctest --test-dir build/story-data-tests --output-on-failure
build/story-data-tests/StoryDataTests /path/to/PokemonSimulation/data/story/wire-conformance.json
```

For a multi-configuration generator, add `--config Debug` to the build,
`-C Debug` to CTest and use `build/story-data-tests/Debug/StoryDataTests.exe`.
The same native cases are registered in the application's unit-test runner.

`Pokemon_StoryStepJson` encodes and reads the common `StoryCheckpoint`,
`StoryStep` and tile-based `Pose` records. It retains action order, alternative
tiles, certainty, absent facing and explicit false/zero values. Unknown fields,
action/position tags and invalid primitive types fail. Native default values
are omitted canonically; encoding does not establish semantic route validity.

The portable workflow also reads the 31 synthetic conformance cases at
[PokemonSimulation revision 866613f](https://github.com/dayfine/PokemonSimulation/tree/866613fa03e5c4a5eb7e57c91b59fafb44fc7ee3/data/story).
Python reads the same input/expectation records. This fixture revision is a
contract-test dependency, not an approved map-resource revision.

See the [shared checkpoint contract](https://github.com/dayfine/PokemonSimulation/blob/866613fa03e5c4a5eb7e57c91b59fafb44fc7ee3/docs/story/CHECKPOINT_CONTRACT.md)
for the field boundary and proposed common requirements, state, save/retry and
handoff envelope. Both games can use those policies; their menu/detector/action
bindings remain game specific.

These are synthetic codec tests, not exports of either game's authored story
tables or captured controller replay. Evaluated FRLG/BDSP table exporters,
policy/state envelope binding and execution parity remain subsequent work.
No gameplay behavior changes here.

The same CTest command also runs `AuthoredCheckpointTests`: shared authored-envelope parsing/writing and embedded-resource lookup for FRLG, BDSP, and opaque future game settings. Game adapters must validate their own execution object before running anything. The shared codec does not interpret FRLG menus, BDSP save-card policy, native bookkeeping, or controller evidence.
